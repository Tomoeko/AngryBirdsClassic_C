#ifndef AB_UI_SHOCKWAVE_APK_H
#define AB_UI_SHOCKWAVE_APK_H

static bool shockwave_event(void *context, AbDataTable *event, AbError *error) {
    (void)error;
    if (!menu_route_equal(menu_route_field(event, "id"),
                          menu_data("EID_BIRD_SPECIALTY_TRIGGERED")))
        return false;
    ++*(size_t *)context;
    return true;
}

static bool shockwave_widgets(PreviewMenu *menu, AbMetal *renderer, AbError *error) {
    bool launch_enabled = menu->launch_enabled;
    menu->launch_enabled = true;
    for (size_t delayed = 0; delayed < 2; ++delayed) {
        if (!ability_test_episode_level(menu, 10, "BlackBird_1", error))
            return false;
        PreviewGame *game = menu->game;
        size_t notices = 0;
        AbEventListener listener = {menu_span("ShockwaveFixture"), &notices,
                                    shockwave_event};
        if (!ab_event_manager_add(menu->events,
                                  menu_event(menu, "EID_BIRD_SPECIALTY_TRIGGERED"),
                                  &listener, error))
            return false;
        AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
        size_t phase = 0, flight = 0, activated_frame = SIZE_MAX, bird = SIZE_MAX;
        bool damaged = false, electrocuted = false, finished = false;
        bool queued_force = false, slowed = false;
        for (size_t frame = 0; frame < 1200; ++frame) {
            bool camera_ready =
                ab_classic_camera_view(game->playfield.camera).system.input_enabled;
            AbUiPointerReply reply;
            if (frame > 240 && phase < 3 && game->shot.bird_ready && camera_ready) {
                AbVec2 target = ab_classic_sling_view(game->playfield.sling).band.rest;
                if (phase) {
                    target.x -= 4;
                    target.y += 3;
                }
                AbCameraViewport viewport;
                AbVec2 point;
                if (!ab_camera_system_query_viewport(
                        ab_classic_camera_system(game->playfield.camera), &viewport) ||
                    !ab_coordinate_point(&viewport.rendering, AB_SPACE_PHYSICS,
                                         AB_SPACE_SCREEN, target, &point) ||
                    !menu_preview_pointer(menu,
                                          menu_span(phase == 0   ? "LPRESS"
                                                    : phase == 1 ? "LHOLD"
                                                                 : "LRELEASE"),
                                          point, &reply, error))
                    return false;
                ++phase;
            } else if (phase == 3) {
                ++flight;
                if (flight == 10) {
                    for (size_t object = 0; object < level.bodies->count; ++object)
                        if (game->flying.kind == AB_DATA_TABLE &&
                            game->flying.table ==
                                &level.properties->objects[object].physical)
                            bird = object;
                    if (bird == SIZE_MAX || !game->shockwaves || !level.goal_count)
                        return ab_fail(error,
                                       "Short Fuse fixture lost its flying actor");
                    size_t pig = level.goal_order[0];
                    AbDataTable *physical = &level.properties->objects[pig].physical;
                    AbVec2 point = {menu_real(menu_route_field(physical, "x").bits) - 1,
                                    menu_real(menu_route_field(physical, "y").bits) -
                                        (delayed ? 10 : 1)};
                    if (!ab_classic_shots_set_position(game->playfield.shots, bird,
                                                       point, error) ||
                        !ab_body_velocity_set(
                            &level.world->storage
                                 .bodies[level.frame->objects[bird].body]
                                 .motion,
                            (AbVec2){0}))
                        return false;
                    if (delayed) {
                        /* Controlled real named-contact boundary, then ordinary
                         * shared object updates run the source delay/fuse/hook. */
                        if (!game_shock_collision(game, &bird, NULL, error) ||
                            !ab_level_property_set(level.properties, bird,
                                                   menu_span("collision"),
                                                   game->enabled, error))
                            return false;
                    } else if (!menu_preview_pointer(menu, menu_span("LPRESS"),
                                                     (AbVec2){700, 250}, &reply, error))
                        return false;
                } else if (flight == 11 && !delayed &&
                           !menu_preview_pointer(menu, menu_span("LRELEASE"),
                                                 (AbVec2){700, 250}, &reply, error))
                    return false;
            }
            if (!game_eagle_test_frame(menu, error))
                return false;
            if (bird != SIZE_MAX && game->shockwaves->objects[bird].used) {
                PreviewShockObject *wave = game->shockwaves->objects + bird;
                AbClassicWorldRuntimeView world =
                    ab_classic_world_runtime_view(game->world);
                AbForceQueue *queue =
                    ab_classic_simulation_view(
                        ab_classic_dynamics_view(world.dynamics).simulation)
                        .queue;
                slowed |= game->speed == .6f;
                for (size_t index = 0; index < queue->count; ++index) {
                    AbQueuedForce force = queue->entries[index];
                    AbVec2 expected = {
                        (force.point.x - wave->origin.x) * wave->wave.force,
                        (force.point.y - wave->origin.y) * wave->wave.force};
                    if (force.force.x != expected.x || force.force.y != expected.y)
                        return ab_fail(
                            error,
                            "Shockwave changed its captured script force values");
                    queued_force = true;
                }
                if (activated_frame == SIZE_MAX) {
                    activated_frame = frame;
                    if (!level.removed_objects[bird] ||
                        !menu_route_truthy(game->prevent_ending) || game->speed != .8f)
                        return ab_fail(
                            error, "Short Fuse hook lost its actual activation state");
                }
                for (size_t object = 0; object < level.bodies->count; ++object) {
                    AbDataTable *physical = &level.properties->objects[object].physical;
                    damaged |= menu_route_field(physical, "damagedByShockwave").kind !=
                               AB_DATA_NIL;
                    electrocuted |=
                        menu_route_field(physical, "electrocute").kind == AB_DATA_TABLE;
                }
                if (frame == activated_frame + 40 &&
                    !game_eagle_test_capture(menu, renderer,
                                             delayed ? "AB_SHOCKWAVE_DELAY_CAPTURE"
                                                     : "AB_SHOCKWAVE_CAPTURE",
                                             error))
                    return false;
                if (frame > activated_frame + 240) {
                    const AbClassicGameUpdateFunction *update = NULL;
                    if (!ab_classic_game_updates_lookup(
                            game->updates, menu_span("updateBlackBirdShockwaves"),
                            &update, error) ||
                        update || game->speed != 1 ||
                        menu_route_truthy(game->prevent_ending) || !damaged ||
                        /* Only the manual case targets a pig at detonation.
                         * The delayed origin follows the falling bird. */
                        (!delayed && !electrocuted) || !queued_force || !slowed ||
                        level.world->failed)
                        return ab_fail(
                            error,
                            "Short Fuse route %zu cleanup: update=%u speed=%g "
                            "prevent=%u damage=%u electro=%u force=%u slow=%u "
                            "world=%u",
                            delayed, update != NULL, game->speed,
                            menu_route_truthy(game->prevent_ending), damaged,
                            electrocuted, queued_force, slowed, level.world->failed);
                    finished = true;
                    break;
                }
            }
        }
        if (!finished || notices != (delayed ? 1 : 2))
            return ab_fail(error,
                           "Short Fuse fixture did not complete its specialty route");
        if (!ab_event_manager_remove(menu->events,
                                     menu_event(menu, "EID_BIRD_SPECIALTY_TRIGGERED"),
                                     &listener, error))
            return false;
        game_session_free(menu);
        menu->level_request_ready = false;
    }
    menu->launch_enabled = launch_enabled;
    puts("Short Fuse: real mouse launch/manual shockwave, named-contact delayed fuse, "
         "lightning drawing, scaled physics, damage/score, pig electrocution and "
         "cleanup.");
    return true;
}

#endif
