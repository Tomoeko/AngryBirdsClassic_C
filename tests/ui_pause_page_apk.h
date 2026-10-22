#ifndef AB_UI_PAUSE_PAGE_APK_TEST_H
#define AB_UI_PAUSE_PAGE_APK_TEST_H

#include "ab/ui_pause_page.h"

typedef struct {
    AbUiPausePage *page;
    AbDataTable *notices[16];
    size_t notice_count, shows, scaling_calls;
    bool audio, scaling;
} PauseFixture;

static bool pause_audio(void *context, bool *output, AbError *error) {
    PauseFixture *fixture = context;
    (void)error;
    *output = fixture->audio;
    return true;
}

static bool pause_display(void *context, AbUiPointerValue *output, AbError *error) {
    (void)context;
    (void)error;
    *output = text("1-1");
    return true;
}

/* Observe required external common-game services here. This is class-level
 * composition, not a replacement implementation of those game services. */
static bool pause_show_service(void *context, AbError *error) {
    PauseFixture *fixture = context;
    if (!fixture->notice_count ||
        !ab_ui_pointer_value_truthy(
            &fixture->page->widget.frame->fields.pause_page_show))
        return ab_fail(error, "PausePage show service preceded source publication");
    ++fixture->shows;
    return true;
}

static bool pause_scaling_service(void *context, bool enabled, AbError *error) {
    PauseFixture *fixture = context;
    if (enabled == ab_ui_pointer_value_truthy(
                       &fixture->page->widget.frame->fields.pause_page_show))
        return ab_fail(error, "PausePage scaling service preceded source show flag");
    fixture->scaling = enabled;
    ++fixture->scaling_calls;
    return true;
}

static bool pause_notice(void *context, AbDataTable *notice, AbError *error) {
    PauseFixture *fixture = context;
    if (fixture->notice_count == 16)
        return ab_fail(error, "Pause test notice storage exhausted");
    for (size_t index = 0; index < fixture->notice_count; ++index)
        if (fixture->notices[index] == notice)
            return ab_fail(error, "PausePage reused an emitted event table");
    fixture->notices[fixture->notice_count++] = notice;
    return true;
}

static AbDataValue pause_id(Resources *resources, const char *name) {
    AbDataValue key = {.kind = AB_DATA_STRING, .string = span(name)};
    const AbDataValue *value = ab_data_get(resources->events.data.globals, key);
    return value ? *value : (AbDataValue){0};
}

static bool pause_page_profile(Resources *resources, unsigned profile, AbError *error) {
    Fixture fixture;
    CHECK(initialize(&fixture, resources, false, false, false, false, error));
    AbEventManager *events = NULL;
    AbAudioMixer *mixer = NULL;
    bool okay = false;
    AbUiPausePage page = {0};
    PauseFixture observer = {.page = &page};
    AbUiPauseNotice notices[64];
    AbEventTimerEntry timers[4];
    AbUiTextState lines[32];
    char names[32][40];
    AbFontWrappedLine wrapped[8];
    AbSpan labels[8];
    uint32_t codes[64];
    uint8_t bytes[1024];
    AbDataValue time = {0}, tutorial = {0}, music = {0}, skip = {0};
    AbClassicAudioRampState ramp = {0};
    AbVec2 margin = {0};
    AbUiPausePageEnvironment source = {.powerups_enabled = true,
                                       .purchase_elements = true,
                                       .use_scaling = true,
                                       .width_ratio = 0.2f,
                                       .device_model = span("android"),
                                       .menu_font = text("FONT_MENU")};
    source.shop_demo = profile == 2;
    source.mattel_active = profile == 3;
    source.childlock_on = profile == 1;
    source.channel_available = source.channel_enabled = profile == 1;
    if (profile == 1)
        source.device_model = span("osx");
    AbEventListener listener = {.name = span("pause-test"),
                                .context = &observer,
                                .event_triggered = pause_notice};
    const char *const observed[] = {
        "EID_PAUSE_PAGE_SHOWN", "EID_PAUSE_PAGE_EXTENDED", "EID_PAUSE_PAGE_CLOSED",
        "EID_PAUSE_PAGE_HIDE_COMPLETED", "EID_PAUSE_CLICKED"};
    if (!ab_event_manager_open((AbEventManagerLimits){20, 4, 16, 16, 8, 65536},
                               (AbEventManagerHost){0}, &events, error) ||
        !ab_audio_mixer_open((AbAudioMixerLimits){4, 16, 1, 65536}, &mixer, error))
        goto release;
    for (size_t index = 0; index < 5; ++index)
        if (!ab_event_manager_add(events, pause_id(resources, observed[index]),
                                  &listener, error))
            goto release;
    AbUiPausePageHost host = {.scaling = &fixture.scaling,
                              .fonts = &resources->fonts,
                              .events = events,
                              .event_ids = resources->events.data.globals,
                              .screen = &fixture.screen,
                              .margin = &margin,
                              .environment = &source,
                              .time = &time,
                              .tutorial_active = &tutorial,
                              .current_music = &music,
                              .skip_input = &skip,
                              .mixer = mixer,
                              .ramp = &ramp,
                              .solid_texture = 60,
                              .notices = notices,
                              .notice_capacity = 64,
                              .timers = timers,
                              .timer_capacity = 4,
                              .label_storage = {lines, names, 32, wrapped, labels, 8,
                                                codes, 64, bytes, sizeof(bytes)},
                              .context = &observer,
                              .audio_enabled = pause_audio,
                              .level_display = pause_display,
                              .show_pause_menu = pause_show_service,
                              .scaling_enabled = pause_scaling_service};
    if (!ab_ui_pause_page_init(&page, &fixture.context, host, error) ||
        !ab_ui_widget_lifecycle(&page.widget, AB_UI_FRAME_ENTRY, error) ||
        !ab_ui_widget_lifecycle(&page.widget, AB_UI_FRAME_LAYOUT, error))
        goto release;
    bool shop = profile < 2, toons = profile == 1;
    if (page.shop_present != shop || page.toons_present != toons ||
        page.widget.frame->child_count != 1 || page.dummy.frame->child_count != 5 ||
        page.middle.frame->child_count != 3u + shop + toons ||
        ab_ui_pointer_value_truthy(
            &page.controls[AB_UI_PAUSE_CHECKPOINT].frame->fields.visible) ||
        !equal(page.number.state.fields.text, "1-1") ||
        real(ab_ui_toggle_button_get_state(&page.sounds)) != 2 ||
        !page.number_clipping.generation.clipped || !page.registered[1] ||
        page.registered[0] != (profile == 1)) {
        ab_fail(error, "Actual PausePage constructed graph/entry state differs");
        goto release;
    }
    if (!ab_ui_pause_page_show(&page, error) || observer.shows != 1 ||
        observer.scaling || observer.scaling_calls != 1 ||
        !ab_ui_widget_update(&page.widget, 0.1f, 0.1f, error) ||
        page.extend_state != 0.5f ||
        !ab_ui_widget_update(&page.widget, 0.1f, 0.2f, error) ||
        page.extend_state != 1 || observer.notice_count != 2)
        goto release;
    AbUiWidget *resume = page.controls + AB_UI_PAUSE_RESUME;
    AbVec2 point, scale;
    if (!ab_ui_widget_screen_position(resume, &point, error) ||
        !ab_ui_widget_cumulative_scale(resume, &scale, error))
        goto release;
    point.x += (real(resume->frame->fields.w) * 0.5f -
                real(resume->state.image.resource_pivot_x)) *
               scale.x;
    point.y += (real(resume->frame->fields.h) * 0.5f -
                real(resume->state.image.resource_pivot_y)) *
               scale.y;
    fixture.context.cursor = point;
    fixture.context.left_button_held = true;
    fixture.context.touch_count = 1;
    AbUiPointerReply reply;
    if (!ab_ui_widget_pointer(&page.widget, span("LPRESS"), point, &reply, error))
        goto release;
    fixture.context.left_button_held = false;
    fixture.context.touch_count = 0;
    if (!ab_ui_widget_pointer(&page.widget, span("LRELEASE"), point, &reply, error) ||
        !equal(reply.values[0], "BACK_TO_GAME") ||
        reply.values[2].object != (uintptr_t)resume || fixture.sounds != 1)
        goto release;
    fixture.list.count = 0;
    AbUiDrawInput input = {0};
    if (!ab_ui_widget_draw(&page.widget, &input, error) || fixture.list.count < 8 ||
        fixture.commands[0].texture != 60 || fixture.commands[1].texture != 60)
        goto release;
    tutorial = (AbDataValue){.kind = AB_DATA_BOOLEAN};
    fixture.list.count = 0;
    if (!ab_ui_widget_draw(&page.widget, &input, error) || fixture.list.count)
        goto release;
    tutorial = (AbDataValue){0};
    AbDataEntry connecting_id = {{.kind = AB_DATA_STRING, .string = span("id")},
                                 pause_id(resources, "EID_CHANNEL_CONNECTING_STARTED")};
    AbDataTable connecting = {&connecting_id, 1, 1, NULL};
    if (!ab_event_manager_notify(events, &connecting, error) || !page.block_input ||
        !ab_ui_widget_pointer(&page.widget, span("LPRESS"), (AbVec2){0}, &reply,
                              error) ||
        !equal(reply.values[0], "BLOCK"))
        goto release;
    connecting_id.value = pause_id(resources, "EID_CHANNEL_CLOSED");
    if (!ab_event_manager_notify(events, &connecting, error) || page.block_input ||
        !ab_ui_pause_page_hide(&page, error) || !observer.scaling ||
        skip.kind != AB_DATA_BOOLEAN || !skip.bits ||
        !ab_ui_widget_update(&page.widget, 0.2f, 0.4f, error) ||
        page.extend_state != 0 ||
        ab_ui_pointer_value_truthy(&page.widget.frame->fields.visible) ||
        observer.notice_count != 4)
        goto release;
    if (profile == 1) {
        observer.audio = true;
        AbDataEntry audio_fields[] = {
            {{.kind = AB_DATA_STRING, .string = span("id")},
             pause_id(resources, "EID_AUDIO_STATE_CHANGED")},
            {{.kind = AB_DATA_STRING, .string = span("from")},
             {.kind = AB_DATA_STRING, .string = span("KEYBOARD_COMMAND")}}};
        AbDataTable audio_event = {audio_fields, 2, 2, NULL};
        if (!ab_event_manager_notify(events, &audio_event, error) ||
            real(ab_ui_toggle_button_get_state(&page.sounds)) != 1)
            goto release;
    }
    observer.audio = true;
    source.no_audio_on_pause = true;
    if (!ab_ui_pause_page_show(&page, error) ||
        ramp.music_length.bits != number(-0.5f).number_bits ||
        ramp.music_volume.bits != number(1).number_bits ||
        !ab_ui_pause_page_hide(&page, error) ||
        ramp.music_length.bits != number(0.5f).number_bits ||
        !ab_ui_widget_lifecycle(&page.widget, AB_UI_FRAME_EXIT, error))
        goto release;
    if (profile == 0) {
        size_t before = observer.notice_count, scaling_calls = observer.scaling_calls;
        page.host.show_pause_menu = NULL;
        if (ab_ui_pause_page_show(&page, NULL) || !fixture.context.failed ||
            !ab_ui_pointer_value_truthy(&page.widget.frame->fields.pause_page_show) ||
            observer.notice_count != before + 1 ||
            observer.scaling_calls != scaling_calls)
            goto release;
    }
    okay = true;
release:
    if (!ab_ui_pause_page_detach(&page, error))
        okay = false;
    ab_audio_mixer_close(mixer);
    ab_event_manager_close(events);
    return okay;
}

static bool pause_page(Resources *resources, AbError *error) {
    for (unsigned profile = 0; profile < 4; ++profile)
        if (!pause_page_profile(resources, profile, error)) {
            fprintf(stderr, "PausePage profile %u failed\n", profile);
            return false;
        }
    return true;
}

#endif
