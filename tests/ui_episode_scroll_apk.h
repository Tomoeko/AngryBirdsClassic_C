#ifndef AB_UI_EPISODE_SCROLL_APK_TEST_H
#define AB_UI_EPISODE_SCROLL_APK_TEST_H

#include "ab/ui_episode_scroll.h"

#include <stdlib.h>

static const AbDataValue *episode_event(PreviewMenu *menu, const char *name) {
    return ab_data_get(
        menu->event_data.data.globals,
        (AbDataValue){.kind = AB_DATA_STRING, .string = menu_span(name)});
}

static bool episode_scroll_widgets(PreviewMenu *menu, AbUiEpisodeButtonHost host,
                                   AbEpisodeProgress *progress, const AbDataTable *ids,
                                   AbMetal *renderer, const float clear[4],
                                   const char *first_path, const char *last_path,
                                   AbError *error) {
    AbUiEpisodeScroll *owner = calloc(1, sizeof(*owner));
    if (!owner)
        return ab_fail(error, "Cannot allocate episode-scroll integration owner");
    /* Controlled layout and saved-state inputs, not application defaults. */
    AbDataEntry features[] = {
        {.key = {.kind = AB_DATA_STRING, .string = menu_span("g_use_new_layout")},
         .value = {.kind = AB_DATA_BOOLEAN, .bits = 1}},
        {.key = {.kind = AB_DATA_STRING,
                 .string =
                     menu_span("g_episode_selection_use_first_card_width_for_all")},
         .value = {.kind = AB_DATA_BOOLEAN, .bits = 1}},
        {.key = {.kind = AB_DATA_STRING,
                 .string = menu_span("g_episode_selection_button_padding")},
         .value = {.kind = AB_DATA_NUMBER, .bits = menu_number(0.04f).number_bits}},
        {.key = {.kind = AB_DATA_STRING,
                 .string = menu_span("g_freeEagleUnlockStatus")},
         .value = {.kind = AB_DATA_TABLE}},
        {.key = {.kind = AB_DATA_STRING, .string = menu_span("g_episodesBackwards")},
         .value = {.kind = AB_DATA_BOOLEAN}},
        {.key = {.kind = AB_DATA_STRING, .string = menu_span("g_registrationEnabled")},
         .value = {.kind = AB_DATA_BOOLEAN}},
        {.key = {.kind = AB_DATA_STRING,
                 .string = menu_span("g_episode_selection_use_scroll_buttons")},
         .value = {.kind = AB_DATA_BOOLEAN, .bits = 1}},
        {.key = {.kind = AB_DATA_STRING, .string = menu_span("g_layoutMargin")},
         .value = {.kind = AB_DATA_TABLE}}};
    AbDataTable empty = {0};
    features[3].value.table = &empty;
    AbDataEntry margin_entry = {
        .key = {.kind = AB_DATA_STRING, .string = menu_span("x")},
        .value = {.kind = AB_DATA_NUMBER}};
    AbDataTable margin = {.entries = &margin_entry, .count = 1, .capacity = 1};
    features[7].value.table = &margin;
    AbDataTable globals = {.entries = features, .count = 8, .capacity = 8};
    const AbDataTable *previous = progress->globals;
    progress->globals = &globals;
    AbImage captured = {0};
    bool okay = false, entered = false;
    float seconds = 0.02f, wheel = 0;
    const AbDataValue *pause = episode_event(menu, "EID_GAME_PAUSED");
    const AbDataValue *extras = episode_event(menu, "EID_EPISODE_EXTRAS_CHANGED");
    const AbDataValue *unlocked = episode_event(menu, "EID_FREE_VERSION_UNLOCKED");
    if (!pause || !extras || !unlocked ||
        !ab_ui_episode_scroll_init(
            owner, &menu->context, NULL,
            (AbUiEpisodeScrollHost){.buttons = host,
                                    .scroll = {.events = menu->events,
                                               .pause_event = *pause,
                                               .input_seconds = &seconds,
                                               .wheel = &wheel},
                                    .episode_ids = ids,
                                    .globals = &globals,
                                    .progress = progress,
                                    .extras_changed = *extras,
                                    .free_version_unlocked = *unlocked},
            error))
        goto release;
    owner->scroll.current_anchor = 1;
    if (!ab_ui_widget_lifecycle(&owner->scroll.widget, AB_UI_FRAME_ENTRY, error))
        goto release;
    entered = true;
    if (!ab_ui_widget_lifecycle(&owner->scroll.widget, AB_UI_FRAME_LAYOUT, error) ||
        owner->scroll.widget.frame->child_count != 14)
        goto release;
    for (size_t index = 0; index < owner->count; ++index) {
        /* Service the real mixer into a discarded buffer; no audio device is
         * opened. Otherwise this accelerated fixture fills its voice storage. */
        int16_t mixed[512];
        for (size_t block = 0; block < 512; ++block) {
            AbAudioMixerView audio = ab_audio_mixer_view(menu->mixer);
            if (!audio.active_count && !audio.pending_count)
                break;
            if (!ab_audio_mixer_mix(menu->mixer, 512, mixed, 512, error))
                goto release;
        }
        AbUiEpisodeButton *card = owner->buttons + index;
        size_t anchor = index + 1 < owner->scroll.anchor_count
                            ? index + 1
                            : owner->scroll.anchor_count;
        if (!ab_ui_scroll_frame_set_current(&owner->scroll, anchor, error))
            goto release;
        /* Each selection normally leaves this menu; reset the real pointer
         * flags before exercising another card in the retained test owner. */
        if (!ab_ui_widget_clear_pointer_flags(&owner->scroll.widget, error))
            goto release;
        AbVec2 point, size, pivot, scale;
        AbUiPointerValue image = card->widget.state.image.image;
        if (image.kind != AB_UI_POINTER_TEXT ||
            !ab_ui_widget_screen_position(&card->widget, &point, error) ||
            !ab_ui_widget_cumulative_scale(&card->widget, &scale, error) ||
            !menu->context.images.bounds(menu->context.images.context, image.text,
                                         &size, error) ||
            !menu->context.images.pivot(menu->context.images.context, image.text,
                                        &pivot, error))
            goto release;
        point.x += (size.x * 0.5f - pivot.x) * scale.x;
        point.y += (size.y * 0.5f - pivot.y) * scale.y;
        menu->context.cursor = point;
        AbUiPointerReply reply;
        menu->context.touch_count = 1;
        if (!ab_ui_widget_pointer(&owner->scroll.widget, menu_span("LPRESS"), point,
                                  &reply, error))
            goto release;
        menu->context.touch_count = 0;
        if (!ab_ui_widget_pointer(&owner->scroll.widget, menu_span("LRELEASE"), point,
                                  &reply, error))
            goto release;
        if (!menu_equal(reply.values[0], "GOTO_LEVEL_SELECTION") ||
            reply.values[2].kind != AB_UI_POINTER_OBJECT ||
            reply.values[2].object != (uintptr_t)&card->widget) {
            ab_fail(error, "Actual scroller card selection differs at %zu", index);
            goto release;
        }
        if (index == 0 || index + 1 == owner->count) {
            ab_draw_list_clear(menu->context.draw_list);
            if (!ab_ui_widget_draw(&owner->scroll.widget, &(AbUiDrawInput){0}, error) ||
                !ab_metal_capture(renderer, menu->context.draw_list, 480, 320, clear,
                                  1024 * 1024, &captured, error))
                goto release;
            const char *path = index == 0 ? first_path : last_path;
            if (path && !save(path, &captured, error))
                goto release;
            ab_image_free(&captured);
        }
    }
    /* Changed live flags pass through the actual EventManager subscription. */
    features[4].value.bits = 1;
    features[5].value.bits = 1;
    AbDataEntry event_entry = {
        .key = {.kind = AB_DATA_STRING, .string = menu_span("id")}, .value = *unlocked};
    AbDataTable event = {.entries = &event_entry, .count = 1, .capacity = 1};
    if (!ab_event_manager_notify(menu->events, &event, error) ||
        !ab_ui_pointer_value_truthy(&owner->buttons[1].locked) ||
        ab_ui_pointer_value_truthy(&owner->buttons[0].locked) ||
        menu_real(owner->buttons[0].widget.frame->fields.x.number_bits) <=
            menu_real(owner->buttons[1].widget.frame->fields.x.number_bits))
        goto release;
    event_entry.value = *extras;
    if (!ab_event_manager_notify(menu->events, &event, error) ||
        owner->scroll.widget.frame->child_count != 14)
        goto release;
    printf("Owned APK episode scroller: 14 selectable cards, %zu anchors, Metal "
           "first/last "
           "pages, reverse layout and live unlock/rebuild events.\n",
           owner->scroll.anchor_count);
    okay = true;
release:
    if (!okay && !error->message[0])
        ab_fail(error, "Episode-scroll integration failed");
    if (entered && !menu->context.failed &&
        !ab_ui_widget_lifecycle(&owner->scroll.widget, AB_UI_FRAME_EXIT, error))
        okay = false;
    if (!ab_ui_episode_scroll_detach(owner, error))
        okay = false;
    menu->render.clip = (AbDrawClip){0};
    menu->context.touch_count = 0;
    progress->globals = previous;
    ab_image_free(&captured);
    free(owner);
    return okay;
}

#endif
