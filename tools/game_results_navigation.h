#ifndef AB_GAME_RESULTS_NAVIGATION_H
#define AB_GAME_RESULTS_NAVIGATION_H

static AbDataValue result_at(const AbDataTable *table, size_t index) {
    const AbDataValue *value = ab_data_get(table, menu_route_number((float)index));
    return value ? *value : (AbDataValue){0};
}

static bool result_next(PreviewMenu *menu, AbClassicLevelRequest *output, bool *present,
                        AbError *error) {
    const AbDataTable *episodes = ab_episode_catalog_episodes(menu->catalog);
    const AbDataTable *ids = ab_episode_catalog_ids(menu->catalog);
    AbClassicLevelRequest *current = &menu->level_request;
    size_t episode_index = 1;
    AbDataValue episode_id;
    while ((episode_id = result_at(ids, episode_index)).kind != AB_DATA_NIL &&
           !menu_route_equal(episode_id, current->episode_id))
        ++episode_index;
    bool hidden = episode_id.kind == AB_DATA_NIL;
    if (hidden && !menu_route_truthy(menu_route_field(current->episode, "hidden")))
        return ab_fail(error, "Current episode is absent from the level iterator");
    if (hidden)
        episode_id = current->episode_id;
    size_t page_index = (size_t)menu_real(current->page_number.bits);
    size_t level_index = (size_t)menu_real(current->level_index.bits) + 1;
    for (;;) {
        const AbDataValue *episode_value = ab_data_get(episodes, episode_id);
        const AbDataTable *episode =
            episode_value && episode_value->kind == AB_DATA_TABLE ? episode_value->table
                                                                  : NULL;
        AbDataValue pages = menu_route_field(episode, "pages");
        AbDataValue page = pages.kind == AB_DATA_TABLE
                               ? result_at(pages.table, page_index)
                               : (AbDataValue){0};
        if (page.kind == AB_DATA_NIL) {
            if (hidden ||
                (episode_id = result_at(ids, ++episode_index)).kind == AB_DATA_NIL) {
                *present = false;
                return true;
            }
            page_index = level_index = 1;
            continue;
        }
        if (page.kind != AB_DATA_TABLE)
            return ab_fail(error, "Invalid next-level page metadata");
        AbDataValue levels = menu_route_field(page.table, "levels");
        AbDataValue level = levels.kind == AB_DATA_TABLE
                                ? result_at(levels.table, level_index)
                                : (AbDataValue){0};
        if (level.kind == AB_DATA_NIL) {
            ++page_index;
            level_index = 1;
            continue;
        }
        if (level.kind != AB_DATA_TABLE)
            return ab_fail(error, "Invalid next-level record");
        AbDataEntry fields[] = {
            {menu_data("episode"), episode_id},
            {menu_data("page"), menu_route_number((float)page_index)},
            {menu_data("level"), menu_route_number((float)level_index)},
            {menu_data("levelName"), menu_route_field(level.table, "name")}};
        AbDataTable metadata = {fields, 4, 4, NULL};
        *present = ab_classic_level_request_resolve(episodes, &metadata, output, error);
        return *present;
    }
}

/* Apply only after the pointer traversal has returned. New world construction
 * occurs at the next application draw boundary, with canonical catalog data. */
static bool game_results_route(PreviewMenu *menu, AbError *error) {
    PreviewGame *game = menu->game;
    if (!game)
        return true;
    AbUiPointerValue action =
        game->results ? game->results->pending_action : game->navigation_action;
    if (action.kind == AB_UI_POINTER_NONE)
        return true;
    bool from_pause = game->results == NULL;
    if (game->results)
        game->results->pending_action = (AbUiPointerValue){0};
    else
        game->navigation_action = (AbUiPointerValue){0};
    bool back = menu_equal(action, "RETURN_TO_LEVEL_SELECTION");
    bool restart = menu_equal(action, "RESTART_LEVEL");
    bool next = menu_equal(action, "NEXT_LEVEL");
    if (!back && !restart && !next)
        return ab_fail(error, "Unsupported reached result navigation action");
    AbClassicLevelRequest request = menu->level_request;
    bool present = true;
    if (next) {
        if (menu_route_truthy(menu_route_field(request.level, "goto_credits")))
            return ab_fail(error, "Selected result requires its credits scene");
        if (menu_route_truthy(
                menu_route_field(request.level, "return_to_episode_selection")))
            return ab_fail(error,
                           "Selected result requires its episode-end transition");
        if (!result_next(menu, &request, &present, error))
            return false;
        if (!present)
            return ab_fail(error, "Selected result has no next campaign level");
    }
    if (!ab_audio_bank_stop(menu->audio, menu_span("score_count"), error) ||
        !menu_preview_flush(menu, error))
        return false;
    if (back) {
        if (menu_route_truthy(menu_route_field(request.episode, "custom_return")) ||
            menu_route_truthy(menu_route_field(request.episode, "return_to_shop_page")))
            return ab_fail(error, "Selected result requires its custom return scene");
        AbDataEntry fields[] = {
            {menu_data("id"), menu_event(menu, "EID_LEAVE_GAME")},
            {menu_data("reason"),
             menu_data(from_pause ? "PAUSE_MENU_BUTTON" : "LEVELEND_MENU_BUTTON")}};
        AbDataTable notice = {fields, 2, 2, NULL};
        if (!ab_event_manager_notify(menu->events, &notice, error))
            return false;
    }
    game_session_free(menu);
    menu->requested_launch = NULL;
    menu->level_request_ready = false;
    if (back) {
        menu->cutscene_active = menu->episode_active = false;
        menu->level_active = true;
        menu->entered = true;
        return menu->levels &&
               ab_ui_widget_clear_pointer_flags(&menu->levels->widget, error) &&
               ab_ui_widget_lifecycle(&menu->levels->widget, AB_UI_FRAME_ENTRY,
                                      error) &&
               ab_ui_widget_lifecycle(&menu->levels->widget, AB_UI_FRAME_LAYOUT, error);
    }
    menu->level_request = request;
    menu->level_request_ready = true;
    menu->level_restart = restart;
    return true;
}

#endif
