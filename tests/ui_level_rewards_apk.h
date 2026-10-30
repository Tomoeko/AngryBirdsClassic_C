#ifndef AB_UI_LEVEL_REWARDS_APK_H
#define AB_UI_LEVEL_REWARDS_APK_H

static bool level_reward_boomerang(PreviewMenu *menu, AbMetal *renderer,
                                   AbError *error) {
    const char *const names[] = {"LevelP3_271", "LevelP4_426"};
    const char *const keys[] = {"boomerangBirdAchieved", "boomerangBirdAchieved2"};
    for (size_t index = 0; index < 2; ++index) {
        AbSpan key = menu_span(keys[index]);
        if (!ab_settings_statistics_write(menu->settings, &key, 1,
                                          (AbDataValue){.kind = AB_DATA_BOOLEAN},
                                          error) ||
            !ability_test_episode_level(menu, 3 + index, names[index], error))
            return false;
        PreviewGame *game = menu->game;
        AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
        if (level_egg_object(level, "ExtraBoomerangBird_1") == SIZE_MAX)
            return ab_fail(error, "Fresh boomerang reward object was pruned");
        for (size_t frame = 0; frame < 180; ++frame)
            if (!game_eagle_test_frame(menu, error))
                return false;
        AbClassicGameRuntimeView runtime = ab_classic_game_runtime_view(game->game);
        /* Invoke the actual completion owner, then place the independently
         * covered bonus counter at its finished boundary. This isolates the
         * registered reward hooks without manufacturing a native win trace. */
        if (!ab_classic_level_finish_complete(runtime.finish, error) ||
            !game->show_boomerang_popup)
            return ab_fail(error, "Completion owner missed its boomerang hook");
        game->finish.complete.remaining_bird_index = (float)level.bird_count + 1;
        game->finish.complete.bonus_timer = -1;
        if (!ab_classic_level_ending_update(runtime.ending, 0, error))
            return false;
        if (game->show_boomerang_popup || game->reward_events_used != 2 ||
            game->finish.complete.ending_timer != 4.6f ||
            !menu_route_truthy(menu_route_field(
                ab_settings_statistics_root(menu->settings), keys[index])))
            return ab_fail(error,
                           "Boomerang ending missed its reward/event/timer writes");
        if (!game_eagle_test_frame(menu, error))
            return false;
        if (!menu->reward ||
            !menu_equal(menu->reward->egg.state.image.image, "BIRD_BOOMERANG"))
            return ab_fail(error, "Boomerang event did not create its actual image");
        if (!hidden_egg_reward_frames(menu, renderer, error))
            return false;
        game_session_free(menu);
        menu->level_request_ready = false;
        if (!ability_test_episode_level(menu, 3 + index, names[index], error))
            return false;
        game = menu->game;
        level = ab_classic_level_view(game->playfield.level);
        if (level_egg_object(level, "ExtraBoomerangBird_1") != SIZE_MAX ||
            !ab_classic_level_finish_complete(
                ab_classic_game_runtime_view(game->game).finish, error) ||
            game->show_boomerang_popup || game->reward_events_used)
            return ab_fail(error, "Achieved boomerang reward repeated on replay");
        game_session_free(menu);
        menu->level_request_ready = false;
    }
    return true;
}

static bool level_reward_cake(PreviewMenu *menu, AbMetal *renderer, AbError *error) {
    AbSpan key = menu_span("cakeCollected");
    if (!ab_settings_statistics_write(menu->settings, &key, 1,
                                      (AbDataValue){.kind = AB_DATA_BOOLEAN}, error))
        return false;
    for (size_t pass = 0; pass < 3; ++pass) {
        if (!ability_test_episode_level(menu, 7, "LevelH900", error))
            return false;
        PreviewGame *game = menu->game;
        AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
        size_t cake = level_egg_object(level, "SPECIAL_CAKE_1_1");
        game->eagle_bait_launched = pass == 0 ? game->enabled : game->disabled;
        if (cake == SIZE_MAX || !level_egg_destroy(game, cake, error) ||
            !game_eagle_test_frame(menu, error))
            return false;
        bool collected = menu_route_truthy(menu_route_field(
            ab_settings_statistics_root(menu->settings), "cakeCollected"));
        if (pass == 0) {
            if (collected || menu->reward || game->reward_events_used)
                return ab_fail(error, "Eagle collected the gated birthday cake");
        } else if (pass == 1) {
            if (!collected || !menu->reward || game->reward_events_used != 2 ||
                !menu_equal(menu->reward->egg.state.image.image,
                            "GOLDEN_EGG_SPECIAL_CAKE") ||
                !hidden_egg_reward_frames(menu, renderer, error))
                return ab_fail(error, "Birthday cake reward lost its actual process");
        } else if (!collected || menu->reward || game->reward_events_used != 1) {
            return ab_fail(error, "Already collected cake repeated its popup");
        }
        game_session_free(menu);
        menu->level_request_ready = false;
    }
    return true;
}

static bool registered_level_reward_widgets(PreviewMenu *menu, AbMetal *renderer,
                                            AbError *error) {
    bool routing = menu->route_enabled;
    menu->route_enabled = true;
    if (!level_reward_boomerang(menu, renderer, error) ||
        !level_reward_cake(menu, renderer, error))
        return false;
    menu->route_enabled = routing;
    puts("Registered reward levels: boomerang completion/ending hooks, first reward, "
         "replay pruning, cake destruction/Eagle gate, real event/settings owners "
         "and Metal popup frames.");
    return true;
}

#endif
