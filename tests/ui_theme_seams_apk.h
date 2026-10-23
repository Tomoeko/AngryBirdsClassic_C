#ifndef AB_UI_THEME_SEAMS_APK_TEST_H
#define AB_UI_THEME_SEAMS_APK_TEST_H

static bool theme_seam_quad(PreviewMenu *menu, const char *name, AbDrawQuad *output,
                            AbError *error) {
    AbSpriteAtlasBinding binding;
    const AbSpriteProvider *provider = &menu->sprites.provider;
    uint32_t texture;
    AbDrawQuad atlas;
    if (!provider->sprite(provider->context, menu_span(name), &binding) ||
        !provider->texture(provider->context, binding.image, &texture) ||
        !ab_sprite_quad(binding.sprite.region, binding.image->width,
                        binding.image->height,
                        (AbSpritePlacement){.scale_x = 1, .scale_y = 1}, &atlas, error))
        return ab_fail(error, "Missing theme seam fixture sprite: %s", name);
    const AbDrawList *list = menu->context.draw_list;
    for (size_t draw = 0; draw < list->count; ++draw) {
        const AbDrawCommand *command = list->commands + draw;
        bool match = command->texture == texture;
        for (size_t vertex = 0; match && vertex < 4; ++vertex)
            match = command->quad.vertices[vertex].u == atlas.vertices[vertex].u &&
                    command->quad.vertices[vertex].v == atlas.vertices[vertex].v;
        if (match) {
            *output = command->quad;
            return true;
        }
    }
    return ab_fail(error, "Theme seam fixture sprite was not drawn: %s", name);
}

static bool theme_seam_widgets(PreviewMenu *menu, AbMetal *renderer, AbError *error) {
    const char *const pairs[][2] = {
        {"BACKGROUND_4_LAYER_2_ITEM_1", "BACKGROUND_4_LAYER_2_ITEM_3"},
        {"BACKGROUND_4_LAYER_2_ITEM_2", "BACKGROUND_4_LAYER_2_ITEM_3"},
        {"BACKGROUND_4_LAYER_3_ITEM_4", "BACKGROUND_4_LAYER_3_ITEM_2"}};
    const AbVec2 sizes[] = {{1024, 768}, {1920, 1080}, {2560, 1440}, {768, 1024}};
    AbVec2 previous = menu->screen;
    if (!menu_theme_select(menu, menu_span("theme4"), error))
        return false;
    for (size_t size = 0; size < sizeof(sizes) / sizeof(sizes[0]); ++size) {
        if (!menu_preview_resize(menu, sizes[size], error))
            return false;
        ab_draw_list_clear(menu->context.draw_list);
        if (!menu_background_draw(menu, 0, error))
            return false;
        for (size_t pair = 0; pair < sizeof(pairs) / sizeof(pairs[0]); ++pair) {
            AbDrawQuad upper, lower;
            if (!theme_seam_quad(menu, pairs[pair][0], &upper, error) ||
                !theme_seam_quad(menu, pairs[pair][1], &lower, error))
                return false;
            float gap =
                fabsf(upper.vertices[2].y - lower.vertices[0].y) * sizes[size].y * 0.5f;
            if (gap > 0.001f)
                return ab_fail(error, "Theme attachment leaves a %g pixel slice: %s",
                               (double)gap, pairs[pair][0]);
        }
        float clear[4];
        menu_background_clear(menu, clear);
        AbImage image = {0};
        bool okay = ab_metal_capture(renderer, menu->context.draw_list,
                                     (uint32_t)sizes[size].x, (uint32_t)sizes[size].y,
                                     clear, 20 * 1024 * 1024, &image, error);
        const char *path = getenv("AB_THEME_SEAM_CAPTURE");
        if (okay && path && size == 2)
            okay = save(path, &image, error);
        ab_image_free(&image);
        if (!okay)
            return false;
    }
    if (!menu_theme_select(menu, menu_span("theme1"), error) ||
        !menu_preview_resize(menu, previous, error))
        return false;
    puts("Theme 4 plant, leaf and bush attachments stay joined in Metal at 4:3, "
         "16:9, Retina and portrait sizes.");
    return true;
}

#endif
