#ifndef AB_UI_ROCKET_APK_H
#define AB_UI_ROCKET_APK_H

static bool rocket_set(AbDataTable *table, const char *key, float value,
                       AbError *error) {
    AbObjectPropertyAccess access = {
        .physics_object = {.kind = AB_DATA_TABLE, .table = table}};
    return ab_object_property_set(&access, menu_span(key), menu_route_number(value),
                                  error);
}

static bool rocket_widgets(PreviewMenu *menu, AbMetal *renderer, AbError *error) {
    if (!ability_test_episode_level(menu, 10, "BlackBird_43", error))
        return false;
    PreviewGame *game = menu->game;
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    AbClassicWorldRuntimeView world = ab_classic_world_runtime_view(game->world);
    size_t object = 0;
    while (
        object < level.bodies->count &&
        !menu_route_equal(
            menu_route_field(&level.properties->objects[object].physical, "definition"),
            menu_data("ROCKET_CANISTER")))
        ++object;
    if (object == level.bodies->count || !game->rockets)
        return ab_fail(error, "Rocket fixture lacks its actual canister");
    AbBlockCollisionNotice notice = {.force = nextafterf(10, -INFINITY)};
    AbDataValue result;
    AbDataTable *physical = &level.properties->objects[object].physical;
    size_t ground = level.bodies->count - 1;
    if (!game_rocket_block_collision(game, object, ground, &notice, &result, error) ||
        menu_route_field(physical, "rocketInfo").kind != AB_DATA_NIL)
        return ab_fail(error, "Rocket started below its inclusive force threshold");
    notice.force = 10;
    if (!game_rocket_block_collision(game, object, ground, &notice, &result, error))
        return false;
    AbDataValue info = menu_route_field(physical, "rocketInfo");
    if (info.kind != AB_DATA_TABLE || !rocket_set(info.table, "timer", .5f, error) ||
        !game_rocket_block_collision(game, object, ground, &notice, &result, error) ||
        !menu_route_equal(menu_route_field(info.table, "timer"),
                          menu_route_number(.5f)))
        return ab_fail(error, "Rocket activation lost its stable non-resetting state");
    float interval = 0, duration = 0;
    if (!game_shock_read(info.table, "particleSpawnInterval", &interval, error) ||
        !game_shock_read(info.table, "propelDuration", &duration, error))
        return false;
    size_t particles =
        ab_particles_count(ab_classic_particles_engine(game->particles), false);
    AbForceQueue *queue =
        ab_classic_simulation_view(ab_classic_dynamics_view(world.dynamics).simulation)
            .queue;
    size_t forces = queue->count;
    if (!game_rocket_update(game->rockets, interval, 0, error) ||
        !game_rocket_update(game->rockets, 0, 0, error) ||
        ab_particles_count(ab_classic_particles_engine(game->particles), false) !=
            particles ||
        queue->count != forces + 2 ||
        !game_rocket_update(game->rockets, nextafterf(interval, INFINITY) - interval, 0,
                            error) ||
        ab_particles_count(ab_classic_particles_engine(game->particles), false) <=
            particles)
        return ab_fail(error, "Rocket particle strict boundary/queued thrust failed");
    if (!physics_replay_focus(menu, object, error) ||
        !physics_replay_capture(menu, renderer, error) ||
        !game_eagle_test_capture(menu, renderer, "AB_ROCKET_CAPTURE", error) ||
        !rocket_set(info.table, "timer", duration, error) ||
        !game_rocket_update(game->rockets, .01f, 0, error))
        return false;
    uintptr_t value;
    bool present;
    AbSpan name = level.bodies->objects[object].name;
    if (!ab_string_map_get(game->rockets->active, name, &value, &present, error) ||
        !present || !game_rocket_update(game->rockets, 0, 0, error) ||
        !ab_string_map_get(game->rockets->active, name, &value, &present, error) ||
        present ||
        !ab_string_map_get(world.dead_blocks, name, &value, &present, error) ||
        !present)
        return ab_fail(error, "Rocket expired on the wrong side of its timer crossing");
    for (size_t frame = 0; frame < 180; ++frame)
        if (!game_eagle_test_frame(menu, error) ||
            (frame % 60 == 0 && !physics_replay_capture(menu, renderer, error)))
            return false;
    if (!level.removed_objects[object] || level.world->failed ||
        menu_route_field(physical, "rocketInfo").table != info.table)
        return ab_fail(error,
                       "Rocket removal lost retained state or continuing physics");
    game_session_free(menu);
    menu->level_request_ready = false;
    puts("Actual rocket canister: inclusive collision threshold, non-resetting state, "
         "strict particle interval, queued thrust, expiry/real removal, Metal and "
         "physics.");
    return true;
}

#endif
