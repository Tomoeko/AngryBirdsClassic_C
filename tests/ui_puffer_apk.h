#ifndef AB_UI_PUFFER_APK_H
#define AB_UI_PUFFER_APK_H

static bool puffer_launch(PreviewMenu *menu, AbError *error) {
    for (size_t phase = 0; phase < 3; ++phase) {
        PreviewGame *game = menu->game;
        AbVec2 target = ab_classic_sling_view(game->playfield.sling).band.rest;
        if (phase) {
            target.x -= 4;
            target.y += 3;
        }
        AbCameraViewport viewport;
        AbVec2 point;
        AbUiPointerReply reply;
        if (!ab_camera_system_query_viewport(
                ab_classic_camera_system(game->playfield.camera), &viewport) ||
            !ab_coordinate_point(&viewport.rendering, AB_SPACE_PHYSICS, AB_SPACE_SCREEN,
                                 target, &point) ||
            !menu_preview_pointer(menu,
                                  menu_span(phase == 0   ? "LPRESS"
                                            : phase == 1 ? "LHOLD"
                                                         : "LRELEASE"),
                                  point, &reply, error) ||
            !game_eagle_test_frame(menu, error))
            return false;
    }
    return true;
}

static bool puffer_widgets(PreviewMenu *menu, AbMetal *renderer, AbError *error) {
    for (size_t delayed = 0; delayed < 2; ++delayed) {
        if (!ability_test_episode_level(menu, 7, "Level925", error))
            return false;
        PreviewGame *game = menu->game;
        AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
        for (size_t frame = 0; frame < 360; ++frame)
            if (!game_eagle_test_frame(menu, error))
                return false;
        if (!game->shot.bird_ready || !game->puffers)
            return ab_fail(error, "Puffer fixture did not reach its actual held bird");
        size_t bird = level.bird_order[0];
        AbDataTable *physical = &level.properties->objects[bird].physical;
        if (!menu_route_equal(menu_route_field(physical, "definition"),
                              game_text("GlobeBird")))
            return ab_fail(error, "Actual birthday level has a different leading bird");
        size_t body = level.bodies->objects[bird].physics.body;
        float density;
        if (!game_puffer_read(physical, "density", &density, error) ||
            !puffer_launch(menu, error))
            return false;
        size_t notices = 0;
        AbEventListener listener = {menu_span("PufferFixture"), &notices,
                                    shockwave_event};
        if (!ab_event_manager_add(menu->events,
                                  menu_event(menu, "EID_BIRD_SPECIALTY_TRIGGERED"),
                                  &listener, error))
            return false;
        bool inflated = false, deflating = false, retired = false, delayed_seen = false;
        float largest = 0;
        for (size_t frame = 0; frame < 600 && !retired; ++frame) {
            if (!delayed && (frame == 10 || frame == 11)) {
                AbUiPointerReply reply;
                if (!menu_preview_pointer(
                        menu, menu_span(frame == 10 ? "LPRESS" : "LRELEASE"),
                        (AbVec2){700, 250}, &reply, error))
                    return false;
            }
            if (!game_eagle_test_frame(menu, error))
                return false;
            bool pending;
            if (!game_puffer_has(game->puffers->objects + bird, GAME_PUFFER_DELAY,
                                 &pending, error))
                return false;
            delayed_seen |= pending;
            if (menu_route_truthy(menu_route_field(physical, "isPuffing")) &&
                !level.removed_objects[bird]) {
                float radius = level.bodies->objects[bird].physics.dimensions.radius;
                if (!inflated) {
                    inflated = true;
                    largest = radius;
                    const AbWorldFixture *fixture =
                        level.world->storage.fixtures +
                        level.bodies->objects[bird].physics.fixture;
                    if (!(radius > 5) || fixture->density != .1f ||
                        menu_real(menu_route_field(physical, "density").bits) !=
                            density ||
                        level.bodies->objects[bird].physics.body != body ||
                        (!delayed && !game_eagle_test_capture(
                                         menu, renderer, "AB_PUFFER_CAPTURE", error)))
                        return ab_fail(
                            error, "Inflation changed the wrong native/table fields");
                }
                if (menu_route_field(physical, "deathTimer").kind != AB_DATA_NIL &&
                    radius < largest)
                    deflating = true;
            }
            retired = level.removed_objects[bird];
        }
        if (!inflated || !deflating || !retired || notices != 1 ||
            (delayed && !delayed_seen) ||
            !menu_route_truthy(menu_route_field(physical, "deflated")) ||
            ab_world_has_body(level.world, body))
            return ab_fail(error,
                           "Puffer route %zu failed: inflate=%u shrink=%u retire=%u "
                           "delay=%u notices=%zu",
                           delayed, inflated, deflating, retired, delayed_seen,
                           notices);
        if (!ab_event_manager_remove(menu->events,
                                     menu_event(menu, "EID_BIRD_SPECIALTY_TRIGGERED"),
                                     &listener, error))
            return false;
        game_session_free(menu);
        menu->level_request_ready = false;
    }
    puts("Actual orange bird: mouse launch/manual activation, real first-contact "
         "delay, inflation/material/fixture ownership, deflation/random direction, "
         "bird removal and Metal drawing.");
    return true;
}

#endif
