#ifndef AB_UI_MENU_RESOLUTION_APK_TEST_H
#define AB_UI_MENU_RESOLUTION_APK_TEST_H

static bool high_font_metadata(PreviewMenu *menu, AbError *error) {
    const char *const paths[] = {"assets/data/fonts/1024x768/FONT_BASIC.dat",
                                 "assets/data/fonts/1024x768/FONT_LS_SMALL.dat"};
    for (size_t index = 0; index < 2; ++index) {
        const AbZipEntry *entry = ab_zip_find(menu->archive, paths[index]);
        AbBuffer expected = {0};
        if (!entry ||
            !ab_zip_read(menu->archive, (size_t)(entry - menu->archive->entries),
                         1024 * 1024, &expected, error))
            return false;
        const AbBuffer *loaded = &menu->font_assets[index].metadata;
        bool same = loaded->size == expected.size &&
                    !memcmp(loaded->data, expected.data, expected.size);
        ab_buffer_free(&expected);
        if (!same)
            return ab_fail(error, "Highest-resolution font metadata differs");
    }
    return true;
}

static bool menu_resolution_widgets(const AbZip *zip, PreviewMenu *menu,
                                    AbMetal *renderer, AbDrawList *list,
                                    AbError *error) {
    menu_preview_free(menu);
    menu->highest_resources = true;
    menu->screen = (AbVec2){1024, 768};
    if (!menu_preview_open(zip, (AbAssetTextureHost){renderer, upload, remove_texture},
                           list, menu, error) ||
        !menu_background_open(menu, error) || !high_font_metadata(menu, error))
        return false;
    AbSpriteAtlasBinding play;
    const AbSpriteProvider *sprites = &menu->sprites.provider;
    if (!sprites->sprite(sprites->context, menu_span("BUTTON_TOONS_PLAY"), &play))
        return ab_fail(error, "Missing highest-resolution Play binding");
    const char prefix[] = "assets/data/images/1024x768/";
    if (play.sprite.metadata_path.size < sizeof(prefix) - 1 ||
        memcmp(play.sprite.metadata_path.data, prefix, sizeof(prefix) - 1))
        return ab_fail(error, "Menu selected a lower-resolution sprite profile");
    size_t textures = ab_asset_textures_count(menu->textures);
    const AbImage *image = play.image;
    const AbVec2 sizes[] = {{1024, 768}, {1920, 1080}, {2560, 1080},
                            {320, 240},  {768, 1024},  {1024, 768}};
    menu->navigation_enabled = true;
    for (size_t index = 0; index < sizeof(sizes) / sizeof(sizes[0]); ++index) {
        AbVec2 size = sizes[index];
        if (!menu_preview_resize(menu, size, error) ||
            !menu_preview_draw(menu, 0, error))
            return false;
        float expected = size.x / 1024 < size.y / 768 ? size.x / 1024 : size.y / 768;
        if (menu->scaling.default_profile.x != 1024 ||
            menu->scaling.default_profile.y != 768 ||
            menu->scaling.default_scaling != expected ||
            menu_real(menu->play.frame->fields.x.number_bits) != size.x * 0.5f ||
            (float)menu->context.render.viewport_width != size.x ||
            (float)menu->context.render.viewport_height != size.y ||
            !sprites->sprite(sprites->context, menu_span("BUTTON_TOONS_PLAY"), &play) ||
            play.image != image || ab_asset_textures_count(menu->textures) != textures)
            return ab_fail(error, "Menu resizing changed its resource profile/storage");
        if (!route_enter(menu, error) || !menu_preview_draw(menu, 0, error) ||
            menu->episodes->cards.count != 14 ||
            !route_click(menu, &menu->episodes->back, "GOTO_MAIN_MENU", error))
            return false;
        if (menu->episode_active)
            return ab_fail(error, "Native-size Back did not restore the main menu");
    }
    printf("Highest APK menu assets/fonts retained across 4:3, 16:9, ultrawide, "
           "portrait and small-window layouts; Play and Back route at each size.\n");
    return true;
}

#endif
