#ifndef AB_UI_TERRAIN_APK_TEST_H
#define AB_UI_TERRAIN_APK_TEST_H

static bool terrain_widgets(PreviewMenu *menu, AbMetal *renderer, AbError *error) {
    if (!ability_test_level(menu, "Level4", error))
        return false;
    for (size_t frame = 0; frame < 2; ++frame)
        if (!menu_preview_draw(menu, 1.0f / 60, error))
            return false;
    PreviewGame *game = menu->game;
    size_t masks = 0;
    const AbDrawList *list = menu->context.draw_list;
    for (size_t index = 0; index < list->count; ++index) {
        const AbDrawCommand *command = list->commands + index;
        if (!command->mask.texture)
            continue;
        if (command->texture != game->terrain.texture ||
            command->texture == command->mask.texture ||
            command->blend != AB_BLEND_STRAIGHT)
            return ab_fail(error,
                           "Level 1-7 terrain omitted its independent image/mask");
        ++masks;
    }
    if (!masks)
        return ab_fail(error, "Level 1-7 has no visible actual terrain masks");
    AbImage image = {0};
    float clear[4] = {0, 0, 0, 1};
    bool okay = ab_metal_capture(renderer, list, (uint32_t)menu->screen.x,
                                 (uint32_t)menu->screen.y, clear, 16 * 1024 * 1024,
                                 &image, error);
    const char *path = getenv("AB_TERRAIN_CAPTURE");
    if (okay && path)
        okay = save(path, &image, error);
    ab_image_free(&image);
    if (okay)
        printf("Level 1-7: %zu visible terrain masks with the actual theme texture, "
               "independent UVs and Metal output.\n",
               masks);
    game_session_free(menu);
    menu->level_request_ready = false;
    return okay;
}

#endif
