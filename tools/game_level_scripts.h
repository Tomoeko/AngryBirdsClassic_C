#ifndef AB_GAME_LEVEL_SCRIPTS_H
#define AB_GAME_LEVEL_SCRIPTS_H

static AbDataValue game_script_index(const AbDataTable *table, size_t index) {
    const AbDataValue *value = ab_data_get(table, menu_route_number((float)index));
    return value ? *value : (AbDataValue){0};
}

/* Statistics.calculateOpenGoldenEggLevels5649 visits the actual episode/page
 * catalog and queries unlock state, including each query's nil-row creation. */
static bool game_open_egg_count(PreviewMenu *menu, float *count, AbError *error) {
    const AbDataTable *ids = ab_episode_catalog_ids(menu->catalog);
    const AbDataTable *episodes = ab_episode_catalog_episodes(menu->catalog);
    float total = 0;
    for (size_t index = 1; index <= ids->count; ++index) {
        const AbDataValue *episode =
            ab_data_get(episodes, game_script_index(ids, index));
        if (!episode || episode->kind != AB_DATA_TABLE)
            return ab_fail(error, "Golden-egg statistics has no catalog episode");
        AbDataValue pages = menu_route_field(episode->table, "pages");
        if (pages.kind != AB_DATA_TABLE || !pages.table)
            return ab_fail(error, "Golden-egg statistics has no catalog pages");
        for (size_t page = 1; page <= pages.table->count; ++page) {
            AbDataValue current = game_script_index(pages.table, page);
            if (current.kind != AB_DATA_TABLE || !current.table)
                return ab_fail(error, "Golden-egg statistics has a malformed page");
            if (!menu_route_truthy(
                    menu_route_field(current.table, "calculate_as_golden_egg_levels")))
                continue;
            AbDataValue levels = menu_route_field(current.table, "levels");
            if (levels.kind != AB_DATA_TABLE || !levels.table)
                return ab_fail(error, "Golden-egg statistics has no level list");
            for (size_t level = 1; level <= levels.table->count; ++level) {
                AbDataValue row = game_script_index(levels.table, level);
                AbDataValue name = row.kind == AB_DATA_TABLE
                                       ? menu_route_field(row.table, "name")
                                       : (AbDataValue){0};
                AbDataValue unlocked;
                if (name.kind != AB_DATA_STRING ||
                    !ab_settings_golden_egg_unlocked(menu->highscores, name.string,
                                                     &unlocked, error))
                    return error && error->message[0]
                               ? false
                               : ab_fail(error,
                                         "Golden-egg statistic has no level name");
                if (menu_route_truthy(unlocked))
                    total += 1;
            }
        }
    }
    *count = total;
    return true;
}

/* Ordinary goldenEggAchieved65593. All retained payloads share the game's
 * lifetime. The caller's actual special mode is absent in this ordinary route. */
static bool game_golden_egg(void *context, AbSpan name, AbError *error) {
    PreviewGame *game = context;
    PreviewMenu *menu = game->menu;
    AbDataValue unlocked;
    if (!ab_settings_golden_egg_unlocked(menu->highscores, name, &unlocked, error))
        return false;
    if (menu_route_truthy(unlocked))
        return true;
    if (!ab_settings_golden_egg_unlock(menu->highscores, name, error) ||
        !ab_settings_statistics_create_table(menu->highscores, &name, 1, 4, error))
        return false;
    const char *const fields[] = {"completed", "birds", "score", "lowScore"};
    for (size_t index = 0; index < 4; ++index) {
        AbSpan path[] = {name, menu_span(fields[index])};
        AbDataValue value =
            index ? menu_route_number(0) : (AbDataValue){.kind = AB_DATA_BOOLEAN};
        if (!ab_settings_statistics_write(menu->highscores, path, 2, value, error))
            return false;
    }
    if (!menu_preview_flush(menu, error) || !menu_reward_show(menu, error))
        return false;
    float count = 0;
    if (!game_open_egg_count(menu, &count, error))
        return false;
    game->egg_count =
        (AbDataEntry){game_text("openedLevelsAmount"), menu_route_number(count)};
    game->egg_data = (AbDataTable){&game->egg_count, 1, 1, NULL};
    game->egg_event_fields[0] =
        (AbDataEntry){game_text("id"), menu_event(menu, "EID_GOLDEN_EGG_GAINED")};
    game->egg_event_fields[1] = (AbDataEntry){
        game_text("data"), {.kind = AB_DATA_TABLE, .table = &game->egg_data}};
    game->egg_event = (AbDataTable){game->egg_event_fields, 2, 2, NULL};
    return ab_event_manager_notify(menu->events, &game->egg_event, error);
}

static bool game_level5_update(void *context, float seconds, float time,
                               AbError *error) {
    PreviewGame *game = context;
    (void)seconds;
    (void)time;
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    AbDataValue click = game->menu->root_clicks.double_click;
    return ab_classic_level5_update(
        &game->level5,
        (AbClassicLevel5Input){.world = &level.properties->world,
                               .pressed = &game->pressed,
                               .cursor_physics = game->input.cursor_physics,
                               .double_click =
                                   click.kind == AB_DATA_BOOLEAN && click.bits},
        game_golden_egg, game, error);
}

#endif
