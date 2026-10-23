#ifndef AB_GAME_SLINGSCOPE_H
#define AB_GAME_SLINGSCOPE_H

static bool game_scope_bind(PreviewPowerups *powers, AbError *error) {
    if (powers->scope_bound)
        return true;
    const char *const names[] = {"SLINGSHOT_POWERUP_SCOPE", "PARTICLE_SLINGDOT"};
    const AbSpriteProvider *provider = &powers->game->menu->sprites.provider;
    for (size_t index = 0; index < 2; ++index) {
        AbSpriteAtlasBinding binding = {0};
        uint32_t texture;
        if (!provider->sprite(provider->context, menu_span(names[index]), &binding) ||
            !binding.sprite.region || !binding.image ||
            binding.alpha == AB_SPRITE_ALPHA_UNKNOWN ||
            !provider->texture(provider->context, binding.image, &texture) || !texture)
            return ab_fail(error, "Missing actual Sling Scope image: %s", names[index]);
        AbDrawQuad quad;
        if (!ab_sprite_quad(
                binding.sprite.region, binding.image->width, binding.image->height,
                (AbSpritePlacement){.scale_x = 1, .scale_y = 1}, &quad, error))
            return false;
        powers->scope_images[index].region = binding.sprite.region;
        powers->scope_images[index].texture = texture;
        powers->scope_images[index].alpha = binding.alpha == AB_SPRITE_ALPHA_PRESENT;
        for (size_t vertex = 0; vertex < 4; ++vertex) {
            powers->scope_images[index].uv[2 * vertex] = quad.vertices[vertex].u;
            powers->scope_images[index].uv[2 * vertex + 1] = quad.vertices[vertex].v;
        }
    }
    powers->scope_bound = true;
    return true;
}

static bool game_scope_pivot(PreviewPowerups *powers, AbVec2 *pivot, AbError *error) {
    AbUiImageResourceBoundaries images = powers->game->menu->context.images;
    return images.pivot(images.context, menu_span("SLINGSHOT_POWERUP_SCOPE"), pivot,
                        error);
}

static bool game_scope_reset(void *context, AbError *error) {
    (void)error;
    PreviewPowerups *powers = context;
    powers->scope_activated = false;
    powers->scope_delay = 1.5f;
    return true;
}

static bool game_scope_update(void *context, float seconds, AbError *error) {
    PreviewPowerups *powers = context;
    PreviewGame *game = powers->game;
    AbClassicSlingView view = ab_classic_sling_view(game->playfield.sling);
    AbVec2 velocity = {1, 0}, pivot;
    if (game->shot.dragged && view.band.distance != 0 &&
        !ab_classic_sling_launch_velocity(game->playfield.sling, &velocity, error))
        return false;
    if (!game_scope_pivot(powers, &pivot, error) ||
        !ab_sling_scope_update(&game->menu->scope_state, velocity, seconds, error))
        return false;
    if (powers->scope_activated)
        return true;
    if (powers->scope_delay > 0) {
        powers->scope_delay -= seconds;
        return true;
    }
    AbDataValue resolved;
    uint32_t handle;
    if (!ab_audio_bank_name_value(game->menu->audio, game_text("super_slingshot"),
                                  &resolved, error) ||
        resolved.kind != AB_DATA_STRING ||
        !ab_audio_bank_play_resource(game->menu->audio, resolved.string, 1, false, 0,
                                     &handle, error) ||
        !game_sling_particles(game, "powerupBuff", 1, true, error) ||
        !game_sling_particles(game, "powerupBurst", 40, true, error) ||
        !ab_classic_sling_set_scope(game->playfield.sling, true, error))
        return false;
    powers->scope_activated = true;
    game->menu->scope_state.angle = 0;
    return ab_classic_powerup_manager_deactivate(&powers->manager, 2, true, error);
}

static bool game_scope_sprite(PreviewPowerups *powers, size_t image, AbVec2 position,
                              AbSlingshotDrawState state, float alpha, AbDrawList *list,
                              AbError *error) {
    const AbSpriteRegion *region = powers->scope_images[image].region;
    AbImageRectangle rectangle = {
        .x = position.x - (float)(int32_t)((float)region->pivot_x + 0.5f),
        .y = position.y - (float)(int32_t)((float)region->pivot_y + 0.5f),
        .width = region->width,
        .height = region->height};
    AbDrawCommand command;
    bool visible;
    AbSpriteRenderState render = {.translation_x = state.translation.x,
                                  .translation_y = state.translation.y,
                                  .scale_x = state.scale.x,
                                  .scale_y = state.scale.y,
                                  .angle_radians = state.angle,
                                  .rotation_origin_x = state.pivot.x,
                                  .rotation_origin_y = state.pivot.y};
    return ab_image_rectangle_command(
               rectangle, render, (uint32_t)powers->game->screen.x,
               (uint32_t)powers->game->screen.y, powers->scope_images[image].uv,
               (AbImageRectangleMaterial){powers->scope_images[image].texture,
                                          powers->scope_images[image].alpha, alpha},
               &command, &visible, error) &&
           (!visible || ab_draw_list_push(list, &command, error));
}

static bool game_scope_draw(void *context, float zoom, AbDrawList *list,
                            AbError *error) {
    PreviewPowerups *powers = context;
    if (!powers->scope_activated)
        return true;
    PreviewGame *game = powers->game;
    AbCameraViewport viewport;
    AbVec2 pivot;
    if (!game_scope_bind(powers, error) || !game_scope_pivot(powers, &pivot, error))
        return false;
    if (!ab_camera_system_query_viewport(
            ab_classic_camera_system(game->playfield.camera), &viewport))
        return ab_fail(error, "Sling Scope requires its current camera/images");
    AbSlingshotDrawState render = {
        .translation = {-viewport.rendering.top_left.x, -viewport.rendering.top_left.y},
        .scale = {zoom, zoom},
        .angle = game->menu->scope_state.angle,
        .pivot = pivot};
    AbVec2 start = ab_classic_sling_start_pose(game->playfield.sling).level_start;
    AbVec2 accessory = {start.x * 20 - 60, start.y * 20 - 93};
    if (!game_scope_sprite(powers, 0, accessory, render, 1, list, error))
        return false;
    AbClassicSlingView view = ab_classic_sling_view(game->playfield.sling);
    if (!view.mouse.drag_started || !game->shot.current_bird_name ||
        !game->shot.dragged || view.band.distance == 0 || view.band.distance <= 2)
        return true;
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    AbDataValue definition;
    if (!ab_object_definition_lookup(&level.properties->lookup,
                                     menu_span(game->shot.dragged->name), &definition,
                                     error) ||
        definition.kind != AB_DATA_TABLE)
        return ab_fail(error, "Sling Scope requires the dragged bird definition");
    float force = 0;
    AbDataValue raw_force = menu_route_field(definition.table, "flyVerticalForce");
    AbDataValue acceleration =
        menu_route_field(definition.table, "flyVerticalAcceleration");
    if (menu_route_truthy(raw_force) && !game_powerup_number(raw_force, &force, error))
        return false;
    float mass = game->shot.dragged->mass;
    if (menu_route_truthy(acceleration)) {
        if (!game_powerup_number(acceleration, &force, error))
            return false;
        force *= mass;
    }
    AbSlingScopePlan plan;
    AbSlingScopeInput input = {.velocity = game->menu->scope_state.velocity,
                               .position = view.band.rubber_band,
                               .timer = game->menu->scope_state.timer,
                               .vertical_force = force,
                               .mass = mass,
                               .gravity_y = level.world->definition.gravity.y,
                               .maximum_translation =
                                   level.world->definition.maximum_translation,
                               .physics_to_world = 20,
                               .length = 11,
                               .alpha_factor = 1.2f,
                               .body_present = true,
                               .alpha_decay = true,
                               .render = render};
    if (!ab_sling_scope_plan(&input, &plan, error))
        return false;
    for (size_t dot = 0; dot < plan.count; ++dot)
        if (!game_scope_sprite(powers, 1, plan.dots[dot].position,
                               plan.dots[dot].render, plan.dots[dot].alpha, list,
                               error))
            return false;
    return ab_classic_sling_play_scope_sound(game->playfield.sling, error);
}

#endif
