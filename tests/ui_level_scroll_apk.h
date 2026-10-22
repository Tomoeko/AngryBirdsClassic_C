#ifndef AB_UI_LEVEL_SCROLL_APK_TEST_H
#define AB_UI_LEVEL_SCROLL_APK_TEST_H

#include "ab/ui_level_scroll.h"

static bool level_scroll_click(PreviewMenu *menu, AbUiLevelScroll *owner,
                               AbUiLevelButton *button, AbError *error) {
    AbVec2 point;
    if (!ab_ui_widget_screen_position(&button->widget, &point, error))
        return false;
    menu->context.cursor = point;
    menu->context.left_button_held = true;
    menu->context.touch_count = 1;
    AbUiPointerReply reply;
    if (!ab_ui_widget_pointer(&owner->scroll.widget, menu_span("LPRESS"), point, &reply,
                              error))
        return false;
    menu->context.left_button_held = false;
    menu->context.touch_count = 0;
    return ab_ui_widget_pointer(&owner->scroll.widget, menu_span("LRELEASE"), point,
                                &reply, error) &&
           menu_equal(reply.values[0], "GOTO_LEVEL") &&
           reply.values[1].kind == AB_UI_POINTER_OBJECT &&
           reply.values[1].object == (uintptr_t)&button->meta;
}

static bool level_scroller_widgets(PreviewMenu *menu, AbMetal *renderer,
                                   const char *path, AbError *error) {
    AbUiLevelScroll *owner = calloc(1, sizeof(*owner));
    if (!owner)
        return ab_fail(error, "Cannot allocate actual level-scroller test storage");
    bool okay = false, entered = false;
    const char *stage = "construct";
    AbImage captured = {0};
    AbDataEntry score_fields[] = {
        {menu_data("score"), level_test_number(1)},
        {menu_data("completed"), {.kind = AB_DATA_BOOLEAN, .bits = 1}}};
    AbDataTable score = {score_fields, 2, 2, NULL};
    AbDataEntry saved = {menu_data("Level1"), {.kind = AB_DATA_TABLE, .table = &score}};
    AbDataTable highscores = {&saved, 0, 1, NULL};
    AbEpisodeStatistics statistics = menu->statistics;
    statistics.highscores = &highscores;
    AbUiLevelServices services = {
        .statistics = &statistics,
        .local_settings = ab_settings_statistics_root(menu->local_settings)};
    AbUiLevelButtonHost buttons = ab_ui_level_services_host(&services);
    buttons.fonts = &menu->fonts;
    buttons.scaling = &menu->scaling;
    AbLevelProgress progress = {.episodes = statistics.episodes,
                                .highscores = &highscores,
                                .release_build = true};
    AbUiLevelScrollHost host = {
        .buttons = buttons,
        .progress = &progress,
        .scroll = {.events = menu->events,
                   .pause_event = menu_event(menu, "EID_GAME_PAUSED"),
                   .input_seconds = &menu->input_seconds,
                   .wheel = &menu->wheel},
        .event_ids = menu->event_data.data.globals,
        .initial_time = menu->time,
        .arrows = true};
    if (!ab_ui_level_scroll_init(owner, &menu->context, level_test_number(1), host,
                                 error))
        goto release;
    AbVec2 anchors[] = {{0, 0}, {1024, 0}, {2048, 0}};
    if (!ab_ui_scroll_frame_set_anchors(&owner->scroll, anchors, 3, error) ||
        !ab_ui_widget_lifecycle(&owner->scroll.widget, AB_UI_FRAME_ENTRY, error))
        goto release;
    entered = true;
    stage = "layout";
    if (!ab_ui_widget_lifecycle(&owner->scroll.widget, AB_UI_FRAME_LAYOUT, error) ||
        owner->count != 63 || owner->page_count != 3 || owner->text_count != 1 ||
        owner->image_count != 21)
        goto release;
    stage = "move to final page";
    if (!ab_ui_scroll_frame_set_target(&owner->scroll, 3, error))
        goto release;
    for (size_t frame = 0; frame < 64; ++frame)
        if (!ab_ui_widget_update(&owner->scroll.widget, 0.1f,
                                 menu->time + (float)(frame + 1) * 0.1f, error))
            goto release;
    if (owner->scroll.current_anchor != 3 || owner->image_count != 21 ||
        owner->text_count ||
        menu_real(owner->scroll.widget.scroll[0].number_bits) != 2048)
        goto release;
    ab_draw_list_clear(menu->context.draw_list);
    AbUiDrawInput input = {0};
    const float clear[] = {41.0f / 255, 118.0f / 255, 142.0f / 255, 1};
    stage = "Metal final-page draw";
    if (!ab_ui_widget_draw(&owner->scroll.widget, &input, error) ||
        !ab_metal_capture(renderer, menu->context.draw_list, 1024, 768, clear,
                          16 * 1024 * 1024, &captured, error) ||
        (path && !save(path, &captured, error)))
        goto release;
    stage = "live score sync";
    highscores.count = 1;
    AbDataEntry sync_field = {menu_data("id"), menu_event(menu, "EID_SYNC_NEW_DATA")};
    AbDataTable sync = {&sync_field, 1, 1, NULL};
    if (!ab_event_manager_notify(menu->events, &sync, error) ||
        !ab_ui_pointer_value_truthy(&owner->buttons[1].widget.state.enabled) ||
        !menu_equal(owner->buttons[0].stars.state.image.image, "LS_STARS_1") ||
        !ab_ui_scroll_frame_set_current(&owner->scroll, 1, error) ||
        !level_scroll_click(menu, owner, owner->buttons + 1, error))
        goto release;
    stage = "exit";
    if (!ab_ui_widget_lifecycle(&owner->scroll.widget, AB_UI_FRAME_EXIT, error))
        goto release;
    entered = false;
    puts("Actual LevelScrollFrame: 63 owned buttons, three page anchors, final-page "
         "Metal draw, visible image/text caches, live score sync and level click.");
    okay = true;
release:
    if (!okay && !error->message[0])
        ab_fail(error, "Actual LevelScrollFrame integration differs at %s", stage);
    AbError cleanup = {0};
    if (entered && !menu->context.failed)
        (void)ab_ui_widget_lifecycle(&owner->scroll.widget, AB_UI_FRAME_EXIT, &cleanup);
    (void)ab_ui_level_scroll_detach(owner, &cleanup);
    ab_image_free(&captured);
    free(owner);
    return okay;
}

#endif
