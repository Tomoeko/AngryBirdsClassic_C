#ifndef AB_UI_AIM_PATHS_APK_H
#define AB_UI_AIM_PATHS_APK_H

static bool aim_sprite_commands(PreviewMenu *menu, const char *name, size_t *count,
                                AbDrawQuad *first, AbError *error) {
    AbLuaAssetSpriteBinding binding;
    if (!ab_lua_asset_sprite_bind(&menu->sprites.provider, menu_span(name), &binding,
                                  error) ||
        !binding.region)
        return ab_fail(error, "Aiming fixture has no loaded image: %s", name);
    const AbDrawList *list = menu->context.draw_list;
    *count = 0;
    for (size_t index = 0; index < list->count; ++index) {
        const AbDrawCommand *command = list->commands + index;
        if (command->mask.texture || command->texture != binding.texture ||
            command->coordinates != AB_DRAW_CLIP ||
            command->color[3] * command->opacity <= 0)
            continue;
        bool match = true;
        float left = INFINITY, top = INFINITY, right = -INFINITY, bottom = -INFINITY;
        for (size_t vertex = 0; vertex < 4; ++vertex) {
            const AbDrawVertex *point = command->quad.vertices + vertex;
            match = match && point->u == binding.uv[vertex * 2] &&
                    point->v == binding.uv[vertex * 2 + 1];
            left = fminf(left, point->x);
            right = fmaxf(right, point->x);
            top = fminf(top, point->y);
            bottom = fmaxf(bottom, point->y);
        }
        if (match && left < 1 && right > -1 && top < 1 && bottom > -1) {
            if (!*count && first)
                *first = command->quad;
            ++*count;
        }
    }
    return true;
}

static bool aim_capture(PreviewMenu *menu, AbMetal *renderer, const char *variable,
                        AbError *error) {
    AbImage image = {0};
    float clear[4];
    menu_background_clear(menu, clear);
    bool okay = ab_metal_capture(renderer, menu->context.draw_list,
                                 (uint32_t)menu->screen.x, (uint32_t)menu->screen.y,
                                 clear, 16 * 1024 * 1024, &image, error);
    const char *path = getenv(variable);
    if (okay && path)
        okay = save(path, &image, error);
    ab_image_free(&image);
    return okay;
}

static bool aim_scope_visible(PreviewMenu *menu, AbMetal *renderer, unsigned profile,
                              AbError *error) {
    size_t count;
    AbDrawQuad before, after;
    if (!aim_sprite_commands(menu, "PARTICLE_SLINGDOT", &count, &before, error) ||
        count < 2)
        return ab_fail(error, "Sling Scope has no visible predicted path in profile %u",
                       profile);
    if (!powerup_test_tick(menu, 3, error) ||
        !aim_sprite_commands(menu, "PARTICLE_SLINGDOT", &count, &after, error) ||
        count < 2 || memcmp(&before, &after, sizeof(before)) == 0)
        return ab_fail(error, "Sling Scope did not animate its visible path");
    return profile != 2 ||
           aim_capture(menu, renderer, "AB_LEVEL2_SCOPE_CAPTURE", error);
}

static bool aim_flight_visible(PreviewMenu *menu, AbMetal *renderer, unsigned profile,
                               AbError *error) {
    bool seen = false;
    size_t most = 0;
    for (size_t frame = 0; frame < 180; ++frame) {
        if (!powerup_test_tick(menu, 1, error))
            return false;
        size_t count = 0;
        const char *const names[] = {"TRAIL_WHITE_1", "TRAIL_WHITE_2", "TRAIL_WHITE_3"};
        for (size_t index = 0; index < 3; ++index) {
            size_t current;
            if (!aim_sprite_commands(menu, names[index], &current, NULL, error))
                return false;
            count += current;
        }
        if (count > most)
            most = count;
        if (!seen && count >= 3) {
            if (profile == 2 &&
                !aim_capture(menu, renderer, "AB_LEVEL2_TRAIL_CAPTURE", error))
                return false;
            seen = true;
        }
    }
    if (!seen)
        return ab_fail(
            error, "Launched bird omitted its visible white trail (%zu dots)", most);
    printf(
        "Aiming profile %u: animated Scope prediction and %zu visible flight dots.\n",
        profile, most);
    return true;
}

#endif
