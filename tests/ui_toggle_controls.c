#include "ab/ui_toggle_button.h"
#include "ab/ui_toggle_indicator.h"
#include "ab/ui_main_menu_options.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Toggle control check failed at %d: %s\n", __LINE__,       \
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

static uint32_t bits(float value) {
    uint32_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static bool named(AbUiPointerValue value, const char *name) {
    return value.kind == AB_UI_POINTER_TEXT && value.text.size == strlen(name) &&
           !memcmp(value.text.data, name, value.text.size);
}

typedef struct {
    unsigned sounds, lookups, child_updates, layouts;
    bool fail_b;
} Fixture;

/* Controlled resource inputs; actual assets/Metal are exercised by the APK
 * integration. These fixtures test real class state and traversal composition. */
static bool localize(void *context, AbSpan image, AbSpan *output, AbError *error) {
    (void)context;
    (void)error;
    *output = image;
    return true;
}

static bool bounds(void *context, AbSpan image, AbVec2 *output, AbError *error) {
    Fixture *fixture = context;
    ++fixture->lookups;
    if (fixture->fail_b && image.size == 1 && image.data[0] == 'B') {
        ab_fail(error, "Prepared B resource failure");
        return false;
    }
    *output = (AbVec2){32, 16};
    return true;
}

static bool pivot(void *context, AbSpan image, AbVec2 *output, AbError *error) {
    (void)context;
    (void)image;
    (void)error;
    *output = (AbVec2){16, 8};
    return true;
}

static bool audio(void *context, AbUiPointerValue name, float volume, bool loop,
                  AbError *error) {
    Fixture *fixture = context;
    (void)error;
    CHECK(named(name, "menu_confirm") && volume == 1 && !loop);
    ++fixture->sounds;
    return true;
}

static bool click(AbUiToggleButton *button, AbUiPointerReply *reply, AbError *error) {
    return ab_ui_widget_pointer(&button->widget, span("LPRESS"), (AbVec2){0}, reply,
                                error) &&
           ab_ui_widget_pointer(&button->widget, span("LRELEASE"), (AbVec2){0}, reply,
                                error);
}

static bool button(void) {
    Fixture fixture = {0};
    AbError error = {0};
    AbUiWidgetContext context = {.images = {&fixture, localize, bounds, pivot},
                                 .maximum_depth = 16,
                                 .audio_context = &fixture,
                                 .play_audio = audio};
    AbUiToggleButton control = {0}, constructor_control = {0};
    AbUiImageButtonArguments arguments = {.return_value = text("SWITCH")};
    CHECK(ab_ui_toggle_button_init(&control, &context, (AbUiWidgetChildren){0},
                                   &arguments, &error));
    CHECK(control.state.number_bits == bits(1) &&
          control.states == &control.empty_states);
    AbDataEntry entries[] = {{{.kind = AB_DATA_NUMBER, .bits = bits(1)},
                              {.kind = AB_DATA_STRING, .string = span("A")}},
                             {{.kind = AB_DATA_NUMBER, .bits = bits(2)},
                              {.kind = AB_DATA_STRING, .string = span("B")}}};
    AbDataTable states = {entries, 2, 2, NULL};
    AbUiImageButtonArguments constructor_arguments = {
        .image.image = {.kind = AB_UI_POINTER_OBJECT, .object = (uintptr_t)&states}};
    CHECK(ab_ui_toggle_button_init(&constructor_control, &context,
                                   (AbUiWidgetChildren){0}, &constructor_arguments,
                                   &error));
    CHECK(constructor_control.states == &constructor_control.empty_states &&
          constructor_control.state.number_bits == bits(1) &&
          constructor_control.widget.state.image.image.kind == AB_UI_POINTER_NONE &&
          fixture.lookups == 0);
    CHECK(ab_ui_toggle_button_set_images(&control, &states, text("OFF"), &error));
    CHECK(named(control.widget.state.image.image, "A"));
    CHECK(ab_ui_toggle_button_set_state(&control, 2, &error));
    CHECK(ab_ui_widget_lifecycle(&control.widget, AB_UI_FRAME_ENTRY, &error));
    CHECK(control.state.number_bits == bits(2));
    AbUiPointerReply reply;
    CHECK(click(&control, &reply, &error));
    CHECK(named(reply.values[0], "SWITCH") &&
          reply.values[2].object == (uintptr_t)&control.widget &&
          control.state.number_bits == bits(1) &&
          named(control.widget.state.image.image, "A") && fixture.sounds == 1);
    CHECK(click(&control, &reply, &error));
    CHECK(control.state.number_bits == bits(2) &&
          named(control.widget.state.image.image, "B"));
    unsigned before = fixture.lookups;
    CHECK(ab_ui_toggle_button_set_state_image(&control, 1, span("B"), &error));
    CHECK(fixture.lookups == before);
    CHECK(ab_ui_toggle_button_set_state_image(&control, 2, span("A"), &error));
    CHECK(named(control.widget.state.image.image, "A"));
    AbUiPointerValue image;
    CHECK(ab_ui_toggle_button_get_state_image(&control, 2, &image, &error) &&
          named(image, "A"));
    CHECK(ab_ui_toggle_button_get_state_image(&control, 9, &image, &error) &&
          image.kind == AB_UI_POINTER_NONE);
    CHECK(ab_ui_toggle_button_set_state(&control, 99, &error));
    CHECK(control.widget.state.image.image.kind == AB_UI_POINTER_NONE);
    CHECK(ab_ui_toggle_button_set_state_image(&control, 2, span("B"), &error));
    CHECK(ab_ui_toggle_button_set_state(&control, 1, &error));
    CHECK(ab_ui_image_button_state_enable(
        &control.widget.state, (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN},
        &context.images, &error));
    CHECK(named(control.widget.state.image.image, "OFF"));
    CHECK(click(&control, &reply, &error) && control.state.number_bits == bits(1));
    CHECK(ab_ui_image_button_state_enable(
        &control.widget.state,
        (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN, .boolean = true},
        &context.images, &error));
    fixture.fail_b = true;
    reply.values[0] = text("sentinel");
    CHECK(ab_ui_widget_pointer(&control.widget, span("LPRESS"), (AbVec2){0}, &reply,
                               &error));
    reply.values[0] = text("sentinel");
    CHECK(!ab_ui_widget_pointer(&control.widget, span("LRELEASE"), (AbVec2){0}, &reply,
                                &error));
    CHECK(context.failed && control.state.number_bits == bits(2) &&
          named(control.widget.state.image.image, "B") &&
          named(reply.values[0], "sentinel"));
    return true;
}

static bool child_update(void *context, AbUiWidget *widget, float seconds, float time,
                         AbError *error) {
    (void)widget;
    (void)seconds;
    (void)time;
    (void)error;
    ++((Fixture *)context)->child_updates;
    return true;
}

static bool indicator(void) {
    Fixture fixture = {0};
    AbUiWidgetContext context = {.maximum_depth = 16};
    AbUiToggleIndicator control = {0};
    AbUiWidget child = {0};
    AbUiFrameNode *frames[1];
    AbUiDrawChild draw[1];
    AbUiPointerChild pointer[1];
    CHECK(ab_ui_toggle_indicator_init(
        &control, &context, (AbUiWidgetChildren){frames, draw, pointer, 1}, NULL));
    CHECK(ab_ui_widget_init(&child, &context, AB_UI_WIDGET_FRAME,
                            (AbUiWidgetChildren){0}, NULL, NULL, NULL, NULL));
    child.methods = (AbUiWidgetMethods){.context = &fixture, .update = child_update};
    CHECK(ab_ui_widget_add(&control.widget, &child, NULL));
    CHECK(ab_ui_toggle_indicator_toggle(&control, NULL));
    CHECK(ab_ui_widget_update(&control.widget, 0.05f, 0, NULL));
    CHECK(control.widget.state.image.frame.fields.angle.number_bits == bits(0.8f));
    CHECK(ab_ui_widget_update(&control.widget, 1, 0, NULL));
    CHECK(control.widget.state.image.frame.fields.angle.number_bits ==
              UINT32_C(0x40490fd0) &&
          named(control.state, "OPEN"));
    CHECK(ab_ui_toggle_indicator_toggle(&control, NULL));
    CHECK(ab_ui_widget_update(&control.widget, 0.01f, 0, NULL));
    CHECK(named(control.state, "CLOSED"));
    uint32_t retained = control.widget.state.image.frame.fields.angle.number_bits;
    CHECK(retained != 0 && retained != UINT32_C(0x40490fd0));
    CHECK(ab_ui_widget_update(&control.widget, 1, 0, NULL));
    CHECK(control.widget.state.image.frame.fields.angle.number_bits == retained &&
          fixture.child_updates == 0);
    CHECK(ab_ui_widget_lifecycle(&control.widget, AB_UI_FRAME_ENTRY, NULL));
    CHECK(control.widget.state.image.frame.fields.angle.number_bits == retained &&
          named(control.state, "CLOSED"));
    return true;
}

static bool environment(void *context, AbUiScalingEnvironment *output, AbError *error) {
    (void)context;
    (void)error;
    *output = (AbUiScalingEnvironment){.screen = {480, 320}, .use_scaling = true};
    return true;
}

static bool parent_lifecycle(void *context, AbUiWidget *widget, AbUiFrameMethod method,
                             AbError *error) {
    if (method == AB_UI_FRAME_LAYOUT)
        ++((Fixture *)context)->layouts;
    return ab_ui_widget_lifecycle_base(widget, method, error);
}

static AbDataValue data_text(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING, .string = span(value)};
}

static bool options(unsigned profile) {
    Fixture fixture = {0};
    AbError error = {0};
    AbUiWidgetContext context = {.images = {&fixture, localize, bounds, pivot},
                                 .maximum_depth = 16};
    AbEventManager *events = NULL;
    AbUiScaling scaling = {0};
    AbUiWidget root = {0}, prior = {0};
    AbUiMainMenuSlider slider = {0};
    AbUiMainMenuOptions owner = {0};
    AbUiFrameNode *root_frames[1], *frames[AB_UI_OPTION_COUNT + 1];
    AbUiDrawChild root_draw[1], draw[AB_UI_OPTION_COUNT + 1];
    AbUiPointerChild root_pointer[1], pointer[AB_UI_OPTION_COUNT + 1];
    CHECK(ab_event_manager_open((AbEventManagerLimits){2, 2, 0, 0, 4, 8192},
                                (AbEventManagerHost){0}, &events, &error));
    CHECK(ab_ui_scaling_init(
        &scaling,
        (AbUiScalingHost){events, data_text("resolution-fixture"), NULL, environment},
        &error));
    CHECK(
        ab_ui_widget_init(&root, &context, AB_UI_WIDGET_FRAME,
                          (AbUiWidgetChildren){root_frames, root_draw, root_pointer, 1},
                          NULL, NULL, NULL, &error));
    root.methods =
        (AbUiWidgetMethods){.context = &fixture, .lifecycle = parent_lifecycle};
    CHECK(ab_ui_main_menu_slider_init(
        &slider, &context,
        (AbUiWidgetChildren){frames, draw, pointer, AB_UI_OPTION_COUNT + 1}, NULL,
        &scaling, events, data_text("slider-fixture"), &error));
    CHECK(ab_ui_widget_add(&root, &slider.box.widget, &error));
    if (!profile) {
        AbUiFrameFields fields = {.name = text("prior")};
        CHECK(ab_ui_widget_init(&prior, &context, AB_UI_WIDGET_FRAME,
                                (AbUiWidgetChildren){0}, &fields, NULL, NULL, &error));
        CHECK(ab_ui_widget_add(&slider.box.widget, &prior, &error));
    }
    CHECK(ab_ui_widget_lifecycle(&root, AB_UI_FRAME_ENTRY, &error));
    CHECK(ab_ui_widget_lifecycle(&root, AB_UI_FRAME_LAYOUT, &error));
    unsigned before = fixture.layouts;
    AbUiMainMenuOptionsSource source = {.device_model = span("android"),
                                        .keyboard_on_main_menu = true,
                                        .remote_config_present = true,
                                        .allow_low_quality = true,
                                        .low_quality = true,
                                        .powerups_enabled = true,
                                        .childlock_on = true};
    if (profile == 1) {
        source = (AbUiMainMenuOptionsSource){.device_model = span("ipad"),
                                             .keyboard_on_main_menu = true,
                                             .mighty_eagle_buttons = true,
                                             .ios = true,
                                             .iap_enabled = true};
    } else if (profile == 2) {
        source = (AbUiMainMenuOptionsSource){.device_model = span("windows"),
                                             .customer = span("gamehall"),
                                             .keyboard_on_main_menu = true,
                                             .freemium_china = true};
    } else if (profile == 3) {
        source = (AbUiMainMenuOptionsSource){
            .device_model = span("android"),
            .keyboard_on_main_menu = true,
            .remote_config_present = true,
            .remote_keyboard_android = {.kind = AB_UI_POINTER_BOOLEAN},
            .disable_notifications = true};
    }
    CHECK(ab_ui_main_menu_options_init(&owner, &root, &slider, &source, &error));
    CHECK(owner.controls[AB_UI_OPTION_CREDITS] && owner.controls[AB_UI_OPTION_SOUND]);
    CHECK(owner.toggles[3].state.number_bits == bits(1));
    if (!profile) {
        CHECK(slider.box.widget.tree.state->child_count == 7 &&
              frames[0] == &owner.controls[AB_UI_OPTION_KEYBOARD]->tree &&
              frames[1] == &prior.tree && fixture.layouts == before + 2);
        CHECK(owner.toggles[0].state.number_bits == bits(2) &&
              owner.toggles[1].state.number_bits == bits(2) &&
              owner.toggles[2].state.number_bits == bits(2));
        CHECK(frames[2] == &owner.controls[AB_UI_OPTION_QUALITY]->tree &&
              frames[3] == &owner.controls[AB_UI_OPTION_PUSH]->tree &&
              frames[4] == &owner.controls[AB_UI_OPTION_CHILDLOCK]->tree &&
              frames[5] == &owner.controls[AB_UI_OPTION_CREDITS]->tree &&
              frames[6] == &owner.controls[AB_UI_OPTION_SOUND]->tree);
    } else if (profile == 1) {
        CHECK(slider.box.widget.tree.state->child_count == 3 &&
              owner.controls[AB_UI_OPTION_RESTORE] &&
              !owner.controls[AB_UI_OPTION_KEYBOARD] && fixture.layouts == before + 1);
    } else if (profile == 2) {
        CHECK(slider.box.widget.tree.state->child_count == 6 &&
              owner.controls[AB_UI_OPTION_CHINA_ABOUT] &&
              owner.controls[AB_UI_OPTION_MORE_GAMES] &&
              owner.controls[AB_UI_OPTION_FULLSCREEN] && fixture.layouts == before + 2);
    } else {
        CHECK(slider.box.widget.tree.state->child_count == 2 &&
              !owner.controls[AB_UI_OPTION_KEYBOARD] &&
              !owner.controls[AB_UI_OPTION_PUSH] && fixture.layouts == before + 1);
    }
    CHECK(ab_ui_widget_lifecycle(&root, AB_UI_FRAME_EXIT, &error));
    CHECK(ab_ui_scaling_detach(&scaling, &error));
    ab_event_manager_close(events);
    return true;
}

int main(void) {
    if (!button() || !indicator())
        return 1;
    for (unsigned profile = 0; profile < 4; ++profile)
        if (!options(profile))
            return 1;
    puts(
        "Concrete ToggleButton and ToggleIndicator state, input and inheritance pass.");
    return 0;
}
