#include "ab/ui_main_menu_slider.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Menu slider check failed at %d: %s\n", __LINE__,          \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static AbSpan span(const char *value) {
    return (AbSpan){(const uint8_t *)value, strlen(value)};
}

static AbUiPointerValue text(const char *value) {
    return (AbUiPointerValue){.kind = AB_UI_POINTER_TEXT, .text = span(value)};
}

static AbDataValue key(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING, .string = span(value)};
}

static bool named(AbUiPointerValue value, const char *name) {
    return value.kind == AB_UI_POINTER_TEXT && value.text.size == strlen(name) &&
           !memcmp(value.text.data, name, value.text.size);
}

static float numeric(AbUiPointerValue value) {
    float result;
    memcpy(&result, &value.number_bits, sizeof(result));
    return result;
}

typedef struct {
    AbUiMainMenuSlider *slider;
    AbEventManager *events;
    unsigned notifications, sounds;
    bool redirect, fail;
} Fixture;

/* Explicit environment/resource inputs. Delivery, ownership, lifecycle, updates
 * and pointer handling use their production C implementations. The separate APK
 * integration supplies decoded metadata and real rendering/audio owners. */
static bool environment(void *context, AbUiScalingEnvironment *output, AbError *error) {
    (void)context;
    (void)error;
    *output = (AbUiScalingEnvironment){.screen = {480, 320}, .use_scaling = true};
    return true;
}

static bool localize(void *context, AbSpan name, AbSpan *output, AbError *error) {
    (void)context;
    (void)error;
    *output = name;
    return true;
}

static bool bounds(void *context, AbSpan name, AbVec2 *output, AbError *error) {
    (void)context;
    if (name.size == 23 && !memcmp(name.data, "MENU_SLIDER_BG_TOP_LEFT", 23))
        *output = (AbVec2){40, 20};
    else if (name.size == 12 && !memcmp(name.data, "BUTTON_EMPTY", 12))
        *output = (AbVec2){100, 80};
    else if (name.size == 18 && !memcmp(name.data, "BUTTON_ABOUT_SMALL", 18))
        *output = (AbVec2){32, 24};
    else {
        ab_fail(error, "Unexpected prepared menu resource");
        return false;
    }
    return true;
}

static bool pivot(void *context, AbSpan name, AbVec2 *output, AbError *error) {
    AbVec2 size;
    if (!bounds(context, name, &size, error))
        return false;
    *output = (AbVec2){size.x * 0.5f, size.y * 0.5f};
    return true;
}

static bool audio(void *context, AbUiPointerValue sound, float volume, bool loop,
                  AbError *error) {
    Fixture *fixture = context;
    (void)error;
    CHECK(named(sound, "menu_confirm") && volume == 1 && !loop);
    ++fixture->sounds;
    return true;
}

static bool changed(void *context, AbDataTable *event, AbError *error) {
    Fixture *fixture = context;
    const AbDataValue *item = ab_data_get(event, key("item"));
    const AbDataValue *old = ab_data_get(event, key("oldState"));
    const AbDataValue *next = ab_data_get(event, key("newState"));
    CHECK(item && item->kind == AB_DATA_OBJECT &&
          item->object == &fixture->slider->box.widget && next &&
          next->kind == AB_DATA_STRING);
    if (!fixture->notifications) {
        CHECK(!old && fixture->slider->state.kind == AB_UI_POINTER_NONE &&
              fixture->slider->box.width == 500 && !fixture->slider->height_assigned);
    } else {
        CHECK(old && old->kind == AB_DATA_STRING &&
              fixture->slider->state.text.size == old->string.size &&
              !memcmp(fixture->slider->state.text.data, old->string.data,
                      old->string.size));
    }
    ++fixture->notifications;
    if (fixture->redirect)
        fixture->slider->state = text("observer-write");
    return !fixture->fail || ab_fail(error, "Prepared menu observer failure");
}

static bool queued(void *context, AbDataTable *event, AbError *error) {
    Fixture *fixture = context;
    (void)event;
    CHECK(ab_event_manager_view(fixture->events).buffering);
    unsigned before = fixture->notifications;
    CHECK(ab_ui_main_menu_slider_toggle(fixture->slider, error));
    CHECK(fixture->notifications == before + 1 &&
          ab_event_manager_view(fixture->events).buffered_events == 0);
    return true;
}

static bool run(void) {
    AbError error = {0};
    AbUiMainMenuSlider slider = {0};
    AbUiScaling scaling = {0};
    AbEventManager *events = NULL;
    Fixture fixture = {.slider = &slider};
    AbUiWidgetContext context = {.images = {&fixture, localize, bounds, pivot},
                                 .maximum_depth = 16,
                                 .animation_installed = true,
                                 .audio_context = &fixture,
                                 .play_audio = audio};
    AbEventListener listener = {span("observer"), &fixture, changed};
    AbEventListener tick_listener = {span("queued-observer"), &fixture, queued};
    CHECK(ab_event_manager_open((AbEventManagerLimits){4, 4, 4, 0, 8, 8192},
                                (AbEventManagerHost){0}, &events, &error));
    fixture.events = events;
    CHECK(ab_ui_scaling_init(
        &scaling,
        (AbUiScalingHost){events, key("resolution-fixture"), NULL, environment},
        &error));
    CHECK(ab_event_manager_add(events, key("slider-fixture"), &listener, &error));
    AbUiFrameNode *frames[1];
    AbUiDrawChild draw[1];
    AbUiPointerChild pointers[1];
    CHECK(ab_ui_main_menu_slider_init(
        &slider, &context, (AbUiWidgetChildren){frames, draw, pointers, 1}, NULL,
        &scaling, events, key("slider-fixture"), &error));
    CHECK(fixture.notifications == 1 && named(slider.state, "CLOSED") &&
          !slider.height_assigned && slider.border_width == 20);
    AbUiWidget about = {0};
    AbUiImageButtonArguments arguments = {.image.frame.name = text("about"),
                                          .return_value = text("ABOUT")};
    CHECK(ab_ui_widget_init(&about, &context, AB_UI_WIDGET_IMAGE_BUTTON,
                            (AbUiWidgetChildren){0}, NULL, NULL, &arguments, &error));
    CHECK(ab_ui_image_button_state_set_image(&about.state, text("BUTTON_ABOUT_SMALL"),
                                             (AbUiPointerValue){0}, &context.images,
                                             &error));
    CHECK(ab_ui_widget_add(&slider.box.widget, &about, &error));
    CHECK(ab_ui_widget_lifecycle(&slider.box.widget, AB_UI_FRAME_ENTRY, &error));
    CHECK(ab_ui_widget_lifecycle(&slider.box.widget, AB_UI_FRAME_LAYOUT, &error));
    CHECK(slider.height_assigned && slider.unscaled_height == 0 &&
          numeric(slider.box.widget.state.image.frame.fields.h) == 64 &&
          !about.state.image.frame.fields.visible.boolean);
    fixture.redirect = true;
    CHECK(ab_ui_main_menu_slider_toggle(&slider, &error));
    fixture.redirect = false;
    CHECK(named(slider.state, "OPENING") &&
          about.state.image.frame.fields.visible.boolean);
    AbUiPointerReply reply;
    CHECK(ab_ui_widget_pointer(&slider.box.widget, span("LPRESS"), (AbVec2){0}, &reply,
                               &error) &&
          reply.values[0].kind == AB_UI_POINTER_NONE);
    CHECK(ab_ui_widget_update(&slider.box.widget, 0.05f, 0, &error));
    CHECK(slider.unscaled_height == 30.75f &&
          numeric(about.state.image.frame.fields.y) == -18.75f);
    CHECK(ab_ui_widget_update(&slider.box.widget, 0.1f, 0, &error));
    CHECK(named(slider.state, "OPEN") && slider.box.height == 64);
    CHECK(ab_ui_widget_pointer(&slider.box.widget, span("LPRESS"), (AbVec2){0, -52},
                               &reply, &error));
    CHECK(ab_ui_widget_pointer(&slider.box.widget, span("LRELEASE"), (AbVec2){0, -52},
                               &reply, &error));
    CHECK(named(reply.values[0], "ABOUT") &&
          reply.values[2].object == (uintptr_t)&about && fixture.sounds == 1);
    CHECK(ab_ui_main_menu_slider_toggle(&slider, &error));
    slider.unscaled_height = 0; /* Explicit endpoint: closing uses strict <0. */
    CHECK(ab_ui_widget_update(&slider.box.widget, 0, 0, &error) &&
          named(slider.state, "CLOSING"));
    CHECK(ab_ui_widget_update(&slider.box.widget, 0.001f, 0, &error) &&
          named(slider.state, "CLOSED") &&
          !about.state.image.frame.fields.visible.boolean);
    CHECK(ab_event_manager_add(events, key("begin-fixture"), &tick_listener, &error));
    AbDataEntry entry = {key("id"), key("begin-fixture")};
    AbDataTable event = {&entry, 1, 1, NULL};
    CHECK(ab_event_manager_queue(events, &event, &error));
    CHECK(ab_event_manager_tick(events, 0, &error) && named(slider.state, "OPENING"));
    CHECK(ab_ui_main_menu_slider_set_state(&slider, text("CLOSED"), &error));
    fixture.fail = true;
    CHECK(!ab_ui_main_menu_slider_toggle(&slider, &error) && context.failed &&
          named(slider.state, "CLOSED") &&
          !slider.box.widget.state.image.frame.fields.visible.boolean);
    CHECK(ab_ui_scaling_detach(&scaling, &error));
    ab_event_manager_close(events);
    return true;
}

int main(void) {
    if (!run())
        return 1;
    puts("MainMenuSlider lifecycle, state events, motion and button input pass.");
    return 0;
}
