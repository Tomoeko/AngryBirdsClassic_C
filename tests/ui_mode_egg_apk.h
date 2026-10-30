#ifndef AB_UI_MODE_EGG_APK_H
#define AB_UI_MODE_EGG_APK_H

static bool mode_egg_widgets(PreviewMenu *menu, AbMetal *renderer, AbError *error) {
    const char *const names[] = {"EggDefender_classic_8", "BlackBird_15"};
    const char *const eggs[] = {"LevelGE_29", "LevelGE_31"};
    const char *const objects[] = {"BP_BIG_ENGINE_1", "ExtraGoldenEgg_1"};
    menu->route_enabled = true;
    for (size_t index = 0; index < 2; ++index) {
        AbDataValue unlocked;
        AbSpan path[] = {menu_span("openGoldenEggLevels"), menu_span(eggs[index]),
                         menu_span("unlocked")};
        if (!ab_settings_golden_egg_unlocked(menu->highscores, path[1], &unlocked,
                                             error) ||
            !ab_settings_statistics_write(menu->highscores, path, 3,
                                          (AbDataValue){.kind = AB_DATA_BOOLEAN},
                                          error) ||
            !ability_test_episode_level(menu, 9 + index, names[index], error))
            return false;
        PreviewGame *game = menu->game;
        AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
        size_t object = level_egg_object(level, objects[index]);
        if (object == SIZE_MAX || !game->egg_script)
            return ab_fail(error, "Mode extra egg did not retain its fresh object");
        game->eagle_bait_launched = game->enabled;
        if (!level_egg_destroy(game, object, error))
            return false;
        if (!level.removed_objects[object] ||
            !ab_settings_golden_egg_unlocked(menu->highscores, path[1], &unlocked,
                                             error) ||
            menu_route_truthy(unlocked) || menu->reward)
            return ab_fail(
                error,
                "Mode egg %s Eagle gate failed: removed=%u unlocked=%u reward=%u",
                names[index], level.removed_objects[object],
                menu_route_truthy(unlocked), menu->reward != NULL);
        for (size_t frame = 0; frame < 6; ++frame)
            if (!game_eagle_test_frame(menu, error))
                return false;
        game_session_free(menu);
        menu->level_request_ready = false;
        if (!ability_test_episode_level(menu, 9 + index, names[index], error))
            return false;
        game = menu->game;
        level = ab_classic_level_view(game->playfield.level);
        object = level_egg_object(level, objects[index]);
        if (object == SIZE_MAX || !level_egg_destroy(game, object, error) ||
            !ab_settings_golden_egg_unlocked(menu->highscores, path[1], &unlocked,
                                             error) ||
            !menu_route_truthy(unlocked) || !menu->reward ||
            !hidden_egg_reward_frames(menu, renderer, error))
            return ab_fail(error, "Mode egg's actual award/reward path failed");
        game_session_free(menu);
        menu->level_request_ready = false;
        if (!ability_test_episode_level(menu, 9 + index, names[index], error))
            return false;
        game = menu->game;
        level = ab_classic_level_view(game->playfield.level);
        if (level_egg_object(level, objects[index]) != SIZE_MAX)
            return ab_fail(error,
                           "Unlocked mode egg was not pruned before construction");
        for (size_t frame = 0; frame < 180; ++frame)
            if (!game_eagle_test_frame(menu, error))
                return false;
        game_session_free(menu);
        menu->level_request_ready = false;
    }
    puts("Actual EggDefender 8/Short Fuse 15 extra hooks: fresh construction, Eagle "
         "physical cleanup without award, destruction reward, replay pruning and Metal "
         "frames.");
    return true;
}

#endif
