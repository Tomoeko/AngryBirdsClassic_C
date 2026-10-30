#ifndef AB_GAME_LEVEL_REWARDS_H
#define AB_GAME_LEVEL_REWARDS_H

/* level_scripts.lua4663..6010 and game_settings_wrapper.lua1018..1308,
 * 2349/2450. These are level hooks, separate from physical removal/scoring. */
static const char *game_boomerang_setting(PreviewGame *game) {
    if (menu_route_equal(game->level_name, game_text("LevelP3_271")))
        return "boomerangBirdAchieved";
    if (menu_route_equal(game->level_name, game_text("LevelP4_426")))
        return "boomerangBirdAchieved2";
    return NULL;
}

static bool game_reward_notify(PreviewGame *game, const AbDataEntry *fields,
                               size_t count, AbError *error) {
    if (game->reward_events_used == 2 || count > 3)
        return ab_fail(error, "Level reward notification storage exhausted");
    size_t slot = game->reward_events_used++;
    memcpy(game->reward_fields[slot], fields, count * sizeof(*fields));
    game->reward_events[slot] =
        (AbDataTable){game->reward_fields[slot], count, 3, NULL};
    return ab_event_manager_notify(game->menu->events, game->reward_events + slot,
                                   error);
}

static bool game_boomerang_prepare(PreviewGame *game, const AbLuaData *level,
                                   AbError *error) {
    const char *key = game_boomerang_setting(game);
    return !key ||
           !menu_route_truthy(menu_route_field(
               ab_settings_statistics_root(game->menu->settings), key)) ||
           ab_level_remove_loaded_object(level, menu_span("ExtraBoomerangBird_1"),
                                         error);
}

static bool game_boomerang_complete(void *context, AbError *error) {
    PreviewGame *game = context;
    const char *key = game_boomerang_setting(game);
    if (!key)
        return ab_fail(error, "Unknown boomerang completion level");
    if (!menu_route_truthy(
            menu_route_field(ab_settings_statistics_root(game->menu->settings), key)))
        game->show_boomerang_popup = true;
    return true;
}

static bool game_boomerang_ending(void *context, AbError *error) {
    PreviewGame *game = context;
    const char *key = game_boomerang_setting(game);
    if (!key)
        return ab_fail(error, "Unknown boomerang ending level");
    if (!game->show_boomerang_popup)
        return true;
    game->show_boomerang_popup = false;
    game->finish.complete.ending_timer += 3.6f;
    if (!isfinite(game->finish.complete.ending_timer))
        return ab_fail(error, "Boomerang reward ending timer overflow");
    AbDataEntry popup = {game_text("id"),
                         menu_event(game->menu, "EID_BOOMERANG_BIRD_POPUP")};
    AbDataEntry shown = {game_text("id"),
                         menu_event(game->menu, "EID_BOOMERANG_BIRD_POPUP_SHOWN")};
    AbSpan path = menu_span(key);
    return game_reward_notify(game, &popup, 1, error) &&
           ab_settings_statistics_write(game->menu->settings, &path, 1, game->enabled,
                                        error) &&
           game_reward_notify(game, &shown, 1, error);
}

static bool game_cake_removed(PreviewGame *game, const AbDataTable *physical,
                              AbError *error) {
    if (!menu_route_equal(menu_route_field(physical, "name"),
                          game_text("SPECIAL_CAKE_1_1")) ||
        menu_route_truthy(game->eagle_bait_launched))
        return true;
    const AbDataTable *settings = ab_settings_statistics_root(game->menu->settings);
    if (!menu_route_truthy(menu_route_field(settings, "cakeCollected"))) {
        AbSpan key = menu_span("cakeCollected");
        AbDataEntry popup[] = {
            {game_text("id"), menu_event(game->menu, "EID_REWARD_POPUP")},
            {game_text("sprite"), game_text("GOLDEN_EGG_SPECIAL_CAKE")},
            {game_text("sound"), game_text("star_collect")}};
        if (!ab_settings_statistics_write(game->menu->settings, &key, 1, game->enabled,
                                          error) ||
            !game_reward_notify(game, popup, 3, error))
            return false;
    }
    AbDataEntry event = {game_text("id"), menu_event(game->menu, "EID_CAKE_COLLECTED")};
    return game_reward_notify(game, &event, 1, error);
}

#endif
