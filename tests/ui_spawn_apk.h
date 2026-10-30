#ifndef AB_UI_SPAWN_APK_H
#define AB_UI_SPAWN_APK_H

static bool spawn_widgets(PreviewMenu *menu, AbMetal *renderer, AbError *error) {
    const char *const scenes[] = {"Level1896", "Level1899", "Level1905", "Level932"};
    for (size_t scene = 0; scene < sizeof(scenes) / sizeof(*scenes); ++scene) {
        if (!ability_test_episode_level(menu, 7, scenes[scene], error))
            return false;
        PreviewGame *game = menu->game;
        AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
        AbClassicWorldRuntimeView world = ab_classic_world_runtime_view(game->world);
        size_t initial = level.bodies->count, cakes = 0;
        for (size_t object = 0; object < initial; ++object) {
            if (!menu_route_equal(
                    menu_route_field(&level.properties->objects[object].physical,
                                     "definition"),
                    menu_data("Spawning_cake")))
                continue;
            ++cakes;
            if (!ab_string_map_set(
                    world.dead_blocks, level.bodies->objects[object].name,
                    (uintptr_t)(world.physical_values + object), true, error))
                return false;
        }
        if (!cakes || game->spawns.capacity != cakes * 10)
            return ab_fail(error, "Actual cake destruction lost its spawn reservation");
        for (size_t frame = 0; frame < 180 && game->spawns.used < cakes * 10; ++frame)
            if (!game_eagle_test_frame(menu, error))
                return false;
        if (game->spawns.used != cakes * 10 ||
            level.bodies->count != initial + cakes * 10 ||
            ab_classic_world_runtime_view(game->world).physical_count !=
                level.bodies->count ||
            game->explosions.physical_count != level.bodies->count)
            return ab_fail(error,
                           "Cake destruction omitted a generated physical piece");
        for (size_t object = initial; object < level.bodies->count; ++object) {
            const AbDataTable *physical = &level.properties->objects[object].physical;
            AbDataValue script = menu_route_field(physical, "gameObjectScript");
            if (!menu_route_equal(menu_route_field(physical, "definition"),
                                  menu_data("CakeCream")) ||
                menu_real(menu_route_field(physical, "radius").bits) != 1.3f ||
                script.kind != AB_DATA_TABLE ||
                script.table != &level.properties->objects[object].script ||
                level.world->storage
                        .fixtures[level.bodies->objects[object].physics.fixture]
                        .shape.kind != AB_SHAPE_CIRCLE)
                return ab_fail(error,
                               "Generated cake piece lost its live factory state");
        }
        if (!physics_replay_capture(menu, renderer, error))
            return false;
        for (size_t frame = 0; frame < 270; ++frame)
            if (!game_eagle_test_frame(menu, error))
                return false;
        if (level.world->failed)
            return ab_fail(error,
                           "Generated cake circles failed the physics continuation");
        game_session_free(menu);
        menu->level_request_ready = false;
    }
    puts(
        "Four actual birthday cake scenes: all death-spawn circles, canonical "
        "scripts, physics/removal registrations, live continuation and Metal drawing.");
    return true;
}

#endif
