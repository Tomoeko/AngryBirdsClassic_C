#ifndef AB_UI_LEVEL_EGG_APK_H
#define AB_UI_LEVEL_EGG_APK_H

static size_t level_egg_object(AbClassicLevelView level, const char *name) {
    for (size_t index = 0; index < level.bodies->count; ++index)
        if (menu_route_equal(
                menu_route_field(&level.properties->objects[index].physical, "name"),
                menu_data(name)))
            return index;
    return SIZE_MAX;
}

/* Queue actual destruction at the recovered after-physics removal boundary.
 * This tests the level callback and real services, not a native impact trace. */
static bool level_egg_destroy(PreviewGame *game, size_t object, AbError *error) {
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    AbClassicWorldRuntimeView world = ab_classic_world_runtime_view(game->world);
    if (object >= level.bodies->count || object >= world.physical_count ||
        !ab_level_property_set(level.properties, object, menu_span("strength"),
                               menu_route_number(0), error) ||
        !ab_string_map_set(world.dead_blocks, level.bodies->objects[object].name,
                           (uintptr_t)(world.physical_values + object), true, error))
        return false;
    return ab_classic_removals_run(ab_classic_dynamics_view(world.dynamics).removals,
                                   error);
}

static bool level_egg_reward_widgets(const AbZip *zip, PreviewMenu *menu,
                                     AbMetal *renderer, AbDrawList *list,
                                     AbError *error) {
    const char *directory = AB_TEST_STATE_DIRECTORY "/level-egg-reward";
    const char *const paths[] = {
        AB_TEST_STATE_DIRECTORY "/level-egg-reward/settings.lua",
        AB_TEST_STATE_DIRECTORY "/level-egg-reward/settings_local.lua",
        AB_TEST_STATE_DIRECTORY "/level-egg-reward/highscores.lua"};
    for (size_t index = 0; index < 3; ++index)
        if (cc_path_information(paths[index], false, NULL) != CC_PATH_MISSING)
            return ab_fail(error, "Egg fixture requires absent owned save files");
    bool okay = false;
    const char *stage = "fresh registered egg level";
    AbImage image = {0};
    menu_preview_free(menu);
    if (!intro_route_open(zip, menu, renderer, list, directory, error) ||
        !ability_test_episode_level(menu, 2, "LevelP2_64", error))
        goto release;
    AbClassicLevelView level = ab_classic_level_view(menu->game->playfield.level);
    size_t initial_count = level.bodies->count;
    size_t object = level_egg_object(level, "ExtraGoldenEgg_1");
    AbDataValue unlocked;
    if (object == SIZE_MAX)
        goto release;
    stage = "current eagle gate on actual destruction";
    menu->game->eagle_bait_launched = menu->game->enabled;
    if (!level_egg_destroy(menu->game, object, error) ||
        !level.removed_objects[object] || menu->reward ||
        !ab_settings_golden_egg_unlocked(menu->highscores, menu_span("LevelGE_5"),
                                         &unlocked, error) ||
        menu_route_truthy(unlocked))
        goto release;
    game_session_free(menu);
    if (!ability_test_episode_level(menu, 2, "LevelP2_64", error))
        goto release;
    level = ab_classic_level_view(menu->game->playfield.level);
    object = level_egg_object(level, "ExtraGoldenEgg_1");
    stage = "ordinary destruction, achievement, save and Metal reward";
    if (!level_egg_destroy(menu->game, object, error) ||
        !level.removed_objects[object] || !menu->reward ||
        !ab_settings_golden_egg_unlocked(menu->highscores, menu_span("LevelGE_5"),
                                         &unlocked, error) ||
        !menu_route_truthy(unlocked) || menu->game->egg_event.count != 2 ||
        !menu_preview_draw(menu, 0.5f, error))
        goto release;
    float clear[4];
    menu_background_clear(menu, clear);
    if (!ab_metal_capture(renderer, list, 1024, 768, clear, 16 * 1024 * 1024, &image,
                          error))
        goto release;
    stage = "encrypted reload and pre-physics object pruning";
    menu_preview_free(menu);
    if (!intro_route_open(zip, menu, renderer, list, directory, error) ||
        !ab_settings_file_loaded(menu->highscores_file) ||
        !ability_test_episode_level(menu, 2, "LevelP2_64", error))
        goto release;
    level = ab_classic_level_view(menu->game->playfield.level);
    if (level.bodies->count + 1 != initial_count ||
        level_egg_object(level, "ExtraGoldenEgg_1") != SIZE_MAX ||
        !powerup_test_tick(menu, 3, error))
        goto release;
    stage = "retained decorative egg object after unlock";
    game_session_free(menu);
    if (!ab_settings_golden_egg_unlock(menu->highscores, menu_span("LevelGE_2"),
                                       error) ||
        !ability_test_level(menu, "Level34", error))
        goto release;
    level = ab_classic_level_view(menu->game->playfield.level);
    if (level_egg_object(level, "ExtraBeachBall_1") == SIZE_MAX ||
        !powerup_test_tick(menu, 3, error))
        goto release;
    puts("Actual hidden-egg hooks: destruction/eagle gate, unlock/reward/event, "
         "Metal draw, encrypted reload, pre-physics pruning and retained decoration.");
    okay = true;
release:
    if (!okay && !error->message[0])
        ab_fail(error, "Hidden-egg fixture failed at %s", stage);
    ab_image_free(&image);
    menu_preview_free(menu);
    for (size_t index = 0; index < 3; ++index)
        (void)remove(paths[index]);
    return okay && intro_route_open(zip, menu, renderer, list, NULL, error);
}

#endif
