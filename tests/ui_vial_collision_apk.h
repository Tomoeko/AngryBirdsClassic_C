#ifndef AB_UI_VIAL_COLLISION_APK_H
#define AB_UI_VIAL_COLLISION_APK_H

static bool vial_collision_widgets(PreviewMenu *menu, AbError *error) {
    if (!ability_test_episode_level(menu, 10, "BlackBird_6", error))
        return false;
    PreviewGame *game = menu->game;
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    AbClassicWorldRuntimeView world = ab_classic_world_runtime_view(game->world);
    AbDataValue blocks = menu_route_field(level.catalog, "blocks");
    size_t vial = SIZE_MAX, ground = SIZE_MAX;
    for (size_t object = 0; object < level.bodies->count; ++object) {
        if (menu_route_equal(
                (AbDataValue){.kind = AB_DATA_STRING,
                              .string = level.bodies->objects[object].name},
                menu_data("ground")))
            ground = object;
        const AbDataValue *definition = ab_data_get(
            blocks.table, menu_route_field(&level.properties->objects[object].physical,
                                           "definition"));
        if (definition && definition->kind == AB_DATA_TABLE &&
            menu_route_equal(menu_route_field(definition->table, "onCollision"),
                             menu_data("blackBirdVialCollision")))
            vial = object;
    }
    if (vial == SIZE_MAX || ground == SIZE_MAX || !level.goal_count ||
        !level.bird_count)
        return ab_fail(error, "Vial fixture lost its authored actor registries");
    size_t bird = level.bird_order[0], pig = level.goal_order[0];
    AbDataTable *physical = &level.properties->objects[vial].physical;
    AbDataValue hook = menu_data("blackBirdVialCollision");
    AbBirdCollisionNotifyInput contact = {
        .contact = {.first = level.bodies->objects[bird].name,
                    .second = level.bodies->objects[vial].name,
                    .normal = {1, 0}}};
    if (!game_vial_collision(game, hook, vial, ground, NULL, error) ||
        menu_route_field(physical, "previousCollision").kind != AB_DATA_NIL ||
        !ab_classic_bird_collisions_call(world.birds, &contact, error))
        return ab_fail(error, "Vial contact failed its ground/bird identity gates");
    AbDataValue previous = menu_route_field(physical, "previousCollision");
    if (previous.kind != AB_DATA_TABLE ||
        previous.table != &level.properties->objects[bird].physical ||
        !game_vial_collision(game, hook, vial, ground, NULL, error) ||
        menu_route_field(physical, "previousCollision").table != previous.table)
        return ab_fail(error, "Vial contact did not retain the previous bird alias");
    uintptr_t value = 0;
    bool dead = false;
    if (!ab_string_map_get(world.dead_blocks, level.bodies->objects[vial].name, &value,
                           &dead, error) ||
        dead || !game_vial_collision(game, hook, vial, pig, NULL, error) ||
        !ab_string_map_get(world.dead_blocks, level.bodies->objects[vial].name, &value,
                           &dead, error))
        return false;
    previous = menu_route_field(physical, "previousCollision");
    const AbDataValue *queued = (const AbDataValue *)value;
    if (!dead || !queued || queued->kind != AB_DATA_TABLE ||
        queued->table != physical || previous.kind != AB_DATA_TABLE ||
        previous.table != &level.properties->objects[pig].physical)
        return ab_fail(error, "Vial pig contact lost its canonical deferred identity");
    AbVec2 moved = {menu_real(menu_route_field(previous.table, "x").bits) + 1,
                    menu_real(menu_route_field(previous.table, "y").bits)};
    if (!ab_classic_level_set_position(game->playfield.level, pig, moved, error) ||
        menu_real(menu_route_field(previous.table, "x").bits) != moved.x)
        return ab_fail(error, "Vial previousCollision became a detached table copy");
    game_session_free(menu);
    menu->level_request_ready = false;
    puts("Actual vial contacts: live bird-collision dispatch, ignored ground, retained "
         "bird alias, canonical pig "
         "death queue and live previousCollision identity.");
    return true;
}

#endif
