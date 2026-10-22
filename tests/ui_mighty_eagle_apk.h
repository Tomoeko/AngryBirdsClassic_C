#ifndef AB_UI_MIGHTY_EAGLE_APK_TEST_H
#define AB_UI_MIGHTY_EAGLE_APK_TEST_H

#include "ab/ui_mighty_eagle_button.h"
#include "ab/ui_eagle_services.h"

typedef struct {
    AbUiPointerValue values[AB_UI_EAGLE_QUERY_COUNT];
    size_t calls[AB_UI_EAGLE_QUERY_COUNT], used_calls, clock_calls;
    AbUiPointerValue last_free;
    AbDataValue used;
    AbCalendarTime now;
} EagleFixture;

static bool eagle_query(void *context, AbUiEagleQuery kind, AbUiPointerValue *output,
                        AbError *error) {
    EagleFixture *fixture = context;
    if ((unsigned)kind >= AB_UI_EAGLE_QUERY_COUNT)
        return ab_fail(error, "Unexpected Eagle class query");
    ++fixture->calls[kind];
    *output = fixture->values[kind];
    return true;
}

static bool eagle_power(void *context, const AbDataTable **output, AbError *error) {
    (void)context;
    (void)error;
    *output = NULL;
    return true;
}

static bool eagle_used(void *context, AbUiPointerValue free_enabled,
                       AbDataValue *output, AbError *error) {
    EagleFixture *fixture = context;
    (void)error;
    ++fixture->used_calls;
    fixture->last_free = free_enabled;
    *output = fixture->used;
    return true;
}

static bool eagle_clock(void *context, AbCalendarTime *output, AbError *error) {
    EagleFixture *fixture = context;
    (void)error;
    ++fixture->clock_calls;
    *output = fixture->now;
    return true;
}

static bool eagle_notify(Resources *resources, AbEventManager *events, const char *name,
                         AbError *error) {
    AbDataEntry entry = {{.kind = AB_DATA_STRING, .string = span("id")},
                         pause_id(resources, name)};
    AbDataTable notice = {&entry, 1, 1, NULL};
    return ab_event_manager_notify(events, &notice, error);
}

static bool eagle_class(Resources *resources, AbEventManager *events,
                        AbUiMightyEagleButton *button, AbError *error) {
    Fixture fixture;
    CHECK(initialize(&fixture, resources, false, false, false, false, error));
    FontGetterFixture global = {.value = text("FONT_BASIC")};
    AbUiFontGetter getter = {&global, font_getter};
    resources->fonts.getters = &getter;
    resources->fonts.getter_count = 1;
    EagleFixture source = {.now = {2026, 10, 20, 10, 0, 30}};
    source.values[AB_UI_EAGLE_SHOW_ELEMENTS] = source.values[AB_UI_EAGLE_ENABLED] =
        source.values[AB_UI_EAGLE_SUBSYSTEM_ENABLED] =
            (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN, .boolean = true};
    AbDataTable empty = {0};
    AbDataEntry global_fields[] = {
        {{.kind = AB_DATA_STRING, .string = span("startedFromEditor")}, {0}},
        {{.kind = AB_DATA_STRING, .string = span("eagleLockedTime")},
         {.kind = AB_DATA_NUMBER, .bits = number(3660).number_bits}},
        {{.kind = AB_DATA_STRING, .string = span("hasIaps")},
         {.kind = AB_DATA_BOOLEAN, .bits = 1}},
        {{.kind = AB_DATA_STRING, .string = span("g_freeEagleUnlockStatus")},
         {.kind = AB_DATA_TABLE, .table = &empty}},
        {{.kind = AB_DATA_STRING, .string = span("levelName")},
         {.kind = AB_DATA_STRING, .string = span("Level1")}}};
    AbDataTable globals = {global_fields, 5, 5, NULL};
    AbUiTextState lines[32];
    char names[32][40];
    AbFontWrappedLine wrapped[8];
    AbSpan labels[8];
    uint32_t codes[64];
    uint8_t bytes[1024];
    AbUiMightyEagleHost host = {.scaling = &fixture.scaling,
                                .fonts = &resources->fonts,
                                .events = events,
                                .event_ids = resources->events.data.globals,
                                .globals = &globals,
                                .basic_font_getter = {.kind = AB_UI_POINTER_FUNCTION,
                                                      .function = (uintptr_t)&getter},
                                .clock = {&source, eagle_clock},
                                .context = &source,
                                .query = eagle_query,
                                .episode_power = eagle_power,
                                .used_time = eagle_used,
                                .timer_storage = {lines, names, 32, wrapped, labels, 8,
                                                  codes, 64, bytes, 1024}};
    AbUiImageButtonArguments arguments = {
        .image.frame.name = text("eagleButton"),
        .activate_on_release = {.kind = AB_UI_POINTER_BOOLEAN, .boolean = true}};
    CHECK(ab_ui_mighty_eagle_button_init(button, &fixture.context, &arguments,
                                         (AbUiPointerValue){0}, host, error) &&
          global.calls == 1 && button->widget.frame->child_count == 1 &&
          button->timer.widget.tree.parent == &button->widget.tree &&
          equal(button->from, "INGAME") && equal(button->timer_hancor, "HCENTER") &&
          equal(button->widget.state.image.image, "INGAME_BUTTON_EAGLE") &&
          equal(button->timer.state.fields.vertical_anchor, "VCENTER"));
    button->widget.frame->fields.x = number(200);
    button->widget.frame->fields.y = number(100);
    CHECK(ab_ui_widget_lifecycle(&button->widget, AB_UI_FRAME_ENTRY, error) &&
          !ab_ui_pointer_value_truthy(&button->widget.state.enabled) &&
          ab_ui_pointer_value_truthy(&button->widget.frame->fields.visible) &&
          ab_ui_widget_lifecycle(&button->widget, AB_UI_FRAME_LAYOUT, error));
    for (size_t index = 0; index < 8; ++index)
        CHECK(button->subscriptions[index]);
    /* Base disabled release starts the real information timer, then the class
     * suppresses the return metadata and self identity. */
    AbUiPointerReply reply;
    CHECK(ab_ui_widget_pointer(&button->widget, span("LPRESS"), (AbVec2){200, 100},
                               &reply, error) &&
          ab_ui_widget_pointer(&button->widget, span("LRELEASE"), (AbVec2){200, 100},
                               &reply, error) &&
          real(reply.values[0]) == -1 && reply.values[1].kind == AB_UI_POINTER_NONE &&
          reply.values[2].kind == AB_UI_POINTER_NONE && real(button->info_timer) == 3);
    CHECK(ab_ui_widget_update(&button->widget, 0, 0, error) && source.used_calls == 1 &&
          !source.clock_calls &&
          !ab_ui_pointer_value_truthy(&button->timer.widget.frame->fields.visible));
    AbDataEntry date_fields[6];
    const char *const date_names[] = {"year", "month",   "day",
                                      "hour", "minutes", "seconds"};
    const float date_values[] = {2026, 10, 20, 10, 0, 0};
    for (size_t index = 0; index < 6; ++index)
        date_fields[index] = (AbDataEntry){
            {.kind = AB_DATA_STRING, .string = span(date_names[index])},
            {.kind = AB_DATA_NUMBER, .bits = number(date_values[index]).number_bits}};
    AbDataTable date = {date_fields, 6, 6, NULL};
    source.used = (AbDataValue){.kind = AB_DATA_TABLE, .table = &date};
    source.values[AB_UI_EAGLE_FREE_ENABLED] = number(0);
    CHECK(ab_ui_widget_update(&button->widget, 3, 3, error) &&
          source.last_free.kind == AB_UI_POINTER_NUMBER &&
          !source.last_free.number_bits && source.clock_calls == 1 &&
          equal(button->timer.state.fields.text, "1:00:30") &&
          real(button->info_timer) == 0 && button->clipping.generation.count == 1 &&
          ab_ui_pointer_value_truthy(&button->timer.widget.frame->fields.visible));
    size_t retained = button->clipping.used_lines;
    CHECK(ab_ui_widget_update(&button->widget, 0.25f, 3.25f, error) &&
          button->info_timer.kind == AB_UI_POINTER_NONE &&
          button->clipping.used_lines == retained && source.clock_calls == 2 &&
          ab_ui_pointer_value_truthy(&button->timer.widget.frame->fields.visible));
    CHECK(ab_ui_widget_update(&button->widget, 0, 3.25f, error) &&
          !ab_ui_pointer_value_truthy(&button->timer.widget.frame->fields.visible) &&
          source.clock_calls == 2);
    size_t sounds = fixture.sounds;
    CHECK(eagle_notify(resources, events, "EID_MIGHTY_EAGLE_AVAILABLE", error) &&
          ab_ui_pointer_value_truthy(&button->widget.state.enabled) &&
          fixture.sounds == sounds + 1);
    CHECK(ab_ui_widget_pointer(&button->widget, span("LPRESS"), (AbVec2){200, 100},
                               &reply, error) &&
          ab_ui_widget_pointer(&button->widget, span("LRELEASE"), (AbVec2){200, 100},
                               &reply, error) &&
          equal(reply.values[0], "ME_BUTTON_CLICKED") &&
          reply.values[2].kind == AB_UI_POINTER_NONE);
    CHECK(eagle_notify(resources, events, "EID_LEVEL_GOALS_CLEARED", error) &&
          button->state == AB_UI_EAGLE_BOUNCE && real(button->scale_timer) == 0 &&
          ab_ui_widget_update(&button->widget, 0.25f, 3.5f, error) &&
          real(button->scale_timer) == 0.25f &&
          real(button->widget.frame->fields.scale_x) == 0.9375f);
    /* Visibility requires literal true for both the provider and argument.
     * Numeric zero remains truthy elsewhere but must not reveal this control. */
    size_t disabled_calls = source.calls[AB_UI_EAGLE_LEVEL_DISABLED];
    source.values[AB_UI_EAGLE_SHOW_ELEMENTS] = number(0);
    CHECK(ab_ui_mighty_eagle_button_visible(
              button,
              (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN, .boolean = true},
              error) &&
          !ab_ui_pointer_value_truthy(&button->widget.frame->fields.visible) &&
          source.calls[AB_UI_EAGLE_LEVEL_DISABLED] == disabled_calls);
    size_t used_calls = source.used_calls;
    CHECK(ab_ui_widget_update(&button->widget, 1, 4.5f, error) &&
          source.used_calls == used_calls);
    source.values[AB_UI_EAGLE_SHOW_ELEMENTS] =
        (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN, .boolean = true};
    CHECK(ab_ui_mighty_eagle_button_visible(button, number(0), error) &&
          !ab_ui_pointer_value_truthy(&button->widget.frame->fields.visible));
    CHECK(ab_ui_mighty_eagle_button_visible(
              button,
              (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN, .boolean = true},
              error) &&
          eagle_notify(resources, events, "EID_LEVEL_COMPLETE_INIT", error) &&
          button->state == AB_UI_EAGLE_DISAPPEAR && real(button->scale_timer) == 1);
    CHECK(ab_ui_widget_pointer(&button->widget, span("LPRESS"), (AbVec2){200, 100},
                               &reply, error) &&
          ab_ui_widget_pointer(&button->widget, span("LRELEASE"), (AbVec2){200, 100},
                               &reply, error) &&
          reply.values[0].kind == AB_UI_POINTER_NONE);
    CHECK(ab_ui_widget_update(&button->widget, 0.25f, 4.75f, error) &&
          real(button->scale_timer) == 0.25f &&
          real(button->widget.frame->fields.scale_x) == 0.3125f &&
          ab_ui_widget_update(&button->widget, 0.125f, 4.875f, error) &&
          button->state == AB_UI_EAGLE_STILL &&
          button->scale_timer.kind == AB_UI_POINTER_NONE &&
          !ab_ui_pointer_value_truthy(&button->widget.frame->fields.visible));
    CHECK(ab_ui_widget_lifecycle(&button->widget, AB_UI_FRAME_EXIT, error));
    for (size_t index = 0; index < 8; ++index)
        CHECK(button->subscriptions[index] == (index == 2));
    CHECK(ab_ui_mighty_eagle_button_detach(button, error));
    global_fields[0].value = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1};
    AbUiPointerValue editor = source.values[AB_UI_EAGLE_CHILDLOCK] =
        (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN, .boolean = true};
    size_t available_calls = source.calls[AB_UI_EAGLE_AVAILABLE];
    CHECK(ab_ui_mighty_eagle_button_visible(button, editor, error) &&
          ab_ui_widget_lifecycle(&button->widget, AB_UI_FRAME_ENTRY, error) &&
          source.calls[AB_UI_EAGLE_AVAILABLE] == available_calls &&
          equal(button->widget.state.return_value, "TOGGLE_POWERUPS_DISABLED") &&
          equal(button->widget.state.image.image, "BUTTON_USE_EAGLE_DISABLED"));
    for (size_t index = 0; index < 8; ++index)
        CHECK(!button->subscriptions[index]);
    AbUiDrawInput input = {.transform = {.position = {3, 7},
                                         .scale = {8, 9},
                                         .angle = 1,
                                         .local_factor = 2},
                           .present = 63};
    CHECK(
        ab_ui_scaling_set_to(&fixture.scaling, &button->widget.frame->fields, error) &&
        ab_ui_widget_draw(&button->widget, &input, error) && fixture.list.count > 0 &&
        ab_ui_widget_lifecycle(&button->widget, AB_UI_FRAME_EXIT, error));
    AbDataEntry purchase = {
        {.kind = AB_DATA_STRING, .string = span("mightyEagleEnabled")},
        {.kind = AB_DATA_BOOLEAN, .bits = 1}};
    AbDataTable purchases = {&purchase, 1, 1, NULL};
    AbClassicLevelRequest request = {
        .episode = &empty,
        .page = &empty,
        .level = &empty,
        .name = {.kind = AB_DATA_STRING, .string = span("Level1")}};
    AbUiEagleServices services = {.progress = {.episodes = &empty,
                                               .local_settings = &empty,
                                               .globals = &globals,
                                               .purchases = &purchases},
                                  .settings = &empty,
                                  .level = &request,
                                  .clock = {&source, eagle_clock}};
    AbUiMightyEagleButton live = {0};
    CHECK(ab_ui_eagle_services_bind(&services, &host, error) &&
          host.globals == services.progress.globals &&
          ab_ui_mighty_eagle_button_init(&live, &fixture.context, &arguments,
                                         (AbUiPointerValue){0}, host, error) &&
          ab_ui_widget_lifecycle(&live.widget, AB_UI_FRAME_ENTRY, error) &&
          ab_ui_pointer_value_truthy(&live.widget.frame->fields.visible) &&
          ab_ui_pointer_value_truthy(&live.widget.state.enabled));
    live.widget.frame->fields.x = number(200);
    live.widget.frame->fields.y = number(100);
    CHECK(ab_ui_widget_lifecycle(&live.widget, AB_UI_FRAME_LAYOUT, error) &&
          ab_ui_widget_update(&live.widget, 0.125f, 5, error) &&
          ab_ui_widget_pointer(&live.widget, span("LPRESS"), (AbVec2){200, 100}, &reply,
                               error) &&
          ab_ui_widget_pointer(&live.widget, span("LRELEASE"), (AbVec2){200, 100},
                               &reply, error) &&
          equal(reply.values[0], "ME_BUTTON_CLICKED") &&
          ab_ui_widget_lifecycle(&live.widget, AB_UI_FRAME_EXIT, error) &&
          ab_ui_mighty_eagle_button_detach(&live, error));
    button->host.query = NULL;
    CHECK(!ab_ui_mighty_eagle_button_visible(button, number(0), NULL) &&
          fixture.context.failed &&
          button->want_visible.kind == AB_UI_POINTER_BOOLEAN &&
          button->want_visible.boolean);
    button->initialized = false;
    return true;
}

static bool mighty_eagle(Resources *resources, AbError *error) {
    AbEventManager *events = NULL;
    AbUiMightyEagleButton button = {0};
    if (!ab_event_manager_open((AbEventManagerLimits){16, 4, 8, 8, 8, 65536},
                               (AbEventManagerHost){0}, &events, error))
        return false;
    bool okay = eagle_class(resources, events, &button, error);
    /* The class fixture's borrowed providers have ended; only the manager is
     * closed here, without invoking source or lifetime callbacks afterward. */
    ab_event_manager_close(events);
    resources->fonts.getters = NULL;
    resources->fonts.getter_count = 0;
    return okay;
}

#endif
