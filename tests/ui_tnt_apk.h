#ifndef AB_UI_TNT_APK_TEST_H
#define AB_UI_TNT_APK_TEST_H

static bool tnt_explosion_widgets(PreviewMenu *menu, AbMetal *renderer,
                                  AbError *error) {
    if (!ability_test_level(menu, "Level66", error))
        return false;
    PreviewGame *game = menu->game;
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    AbClassicWorldRuntimeView world = ab_classic_world_runtime_view(game->world);
    size_t object = 0;
    while (
        object < level.bodies->count &&
        !menu_route_equal(
            menu_route_field(&level.properties->objects[object].physical, "definition"),
            game_text("ExtraBlockTNT")))
        ++object;
    if (object == level.bodies->count)
        return ab_fail(error, "Authored TNT fixture lost its explosive block");
    if (!ab_level_property_set(level.properties, object, menu_span("strength"),
                               menu_route_number(0), error) ||
        !ab_string_map_set(world.dead_blocks, level.bodies->objects[object].name,
                           (uintptr_t)(world.physical_values + object), true, error) ||
        !ab_classic_removals_run(ab_classic_dynamics_view(world.dynamics).removals,
                                 error))
        return false;
    AbParticles *particles = ab_classic_particles_engine(game->particles);
    const AbParticle *burst = NULL;
    for (size_t index = 0; index < ab_particles_count(particles, false); ++index) {
        const AbParticle *particle = ab_particles_at(particles, false, index);
        if (menu_route_equal(
                (AbDataValue){.kind = AB_DATA_STRING, .string = particle->sprite},
                game_text("EXPLOSION"))) {
            burst = particle;
            break;
        }
    }
    if (!burst || !burst->sprite_handle)
        return ab_fail(error,
                       "TNT fire burst is absent or has no loaded EXPLOSION texture");
    if (burst->scale != 2.5f || burst->lifetime != .25f)
        return ab_fail(error, "TNT fire burst lost its source scale/lifetime");
    AbCameraSystem *camera = ab_classic_camera_system(game->playfield.camera);
    AbCameraSystemView view = ab_camera_system_view(camera);
    AbCameraMove move = {.duration = 0, .duration_present = true};
    if (!ab_camera_system_stop_movement(camera) ||
        !ab_camera_system_goto_snap(camera, view.limits->snap_count - 1, true, true,
                                    &move, error) ||
        !ab_classic_camera_update(game->playfield.camera, 0, &(AbCameraSystemInput){0},
                                  error))
        return false;
    ab_draw_list_clear(menu->context.draw_list);
    if (!game_service(game, &(AbGameTailCall){.operation = AB_GAME_TAIL_DRAW}, error))
        return false;
    AbImage image = {0};
    float clear[] = {0, 0, 0, 1};
    bool okay = ab_metal_capture(renderer, menu->context.draw_list,
                                 (uint32_t)game->screen.x, (uint32_t)game->screen.y,
                                 clear, 16 * 1024 * 1024, &image, error);
    const char *path = getenv("AB_TNT_CAPTURE");
    if (okay && path)
        okay = save(path, &image, error);
    ab_image_free(&image);
    if (okay)
        puts("Actual TNT destruction retains the EXPLOSION fire texture, source "
             "scale/lifetime and Metal draw.");
    game_session_free(menu);
    menu->level_request_ready = false;
    return okay;
}

#endif
