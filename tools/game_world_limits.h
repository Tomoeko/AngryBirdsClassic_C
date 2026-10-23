#ifndef AB_GAME_WORLD_LIMITS_H
#define AB_GAME_WORLD_LIMITS_H

/* Museum's authored-object extrema and CameraSystem.updateLevelLimits11334.
 * The extrema retain prior camera expansions through later desktop resizes. */
static bool game_world_limits(PreviewGame *game, AbError *error) {
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    if (!game->world_edges_initialized) {
        game->world_left_edge = 100000;
        game->world_right_edge = -100000;
        for (size_t object = 0; object < level.bodies->count; ++object) {
            const AbLevelObject *body = level.bodies->objects + object;
            /* Museum expands authored bounds before constructing its 2000-unit
             * ground. That generated body's placement is also non-NULL here. */
            if (!body->placement ||
                (body->name.size == 6 && !memcmp(body->name.data, "ground", 6)))
                continue;
            const AbDataTable *physical = &level.properties->objects[object].physical;
            AbDataValue x = menu_route_field(physical, "x");
            AbDataValue extent = menu_route_field(physical, "width");
            if (extent.kind == AB_DATA_NIL)
                extent = menu_route_field(physical, "radius");
            if (x.kind != AB_DATA_NUMBER || extent.kind != AB_DATA_NUMBER)
                return ab_fail(error, "Missing authored world-edge geometry");
            float left = menu_real(x.bits) - menu_real(extent.bits);
            float right = menu_real(x.bits) + menu_real(extent.bits);
            if (!isfinite(left) || !isfinite(right))
                return ab_fail(error, "Nonfinite authored world-edge geometry");
            if (left < game->world_left_edge)
                game->world_left_edge = left;
            if (right > game->world_right_edge)
                game->world_right_edge = right;
        }
        game->world_edges_initialized = true;
    }
    AbCameraSystemView camera = ab_classic_camera_view(game->playfield.camera).system;
    float left = 10000000, right = -1000000;
    for (size_t index = 0; index < camera.limits->snap_count; ++index) {
        const AbCameraSnap *snap = camera.limits->snaps + index;
        AbCameraViewport extents;
        if (!ab_camera_viewport(
                &(AbCameraViewportInput){.center = snap->position,
                                         .pixel_size = camera.pixel_size,
                                         .zoom = snap->zoom},
                &extents))
            return ab_fail(error, "Invalid camera snap world extents");
        if (extents.top_left.x < left)
            left = extents.top_left.x;
        if (extents.bottom_right.x > right)
            right = extents.bottom_right.x;
    }
    float padding = (camera.pixel_size.x * 0.2f) / camera.mover->current_zoom;
    left = (left - padding) * 0.05f;
    right = (right + padding) * 0.05f;
    if (!isfinite(left) || !isfinite(right))
        return ab_fail(error, "Invalid expanded camera world extents");
    if (left < game->world_left_edge)
        game->world_left_edge = left;
    if (right > game->world_right_edge)
        game->world_right_edge = right;
    if (game->world_left_edge < -2147483648.0f ||
        game->world_right_edge >= 2147483648.0f ||
        game->world_left_edge > game->world_right_edge)
        return ab_fail(error, "World X limits exceed the native integer domain");
    if (!game->world)
        return true;
    AbClassicDynamicsView dynamics =
        ab_classic_dynamics_view(ab_classic_world_runtime_view(game->world).dynamics);
    return ab_classic_simulation_set_limits(dynamics.simulation,
                                            (int32_t)game->world_left_edge,
                                            (int32_t)game->world_right_edge, error);
}

#endif
