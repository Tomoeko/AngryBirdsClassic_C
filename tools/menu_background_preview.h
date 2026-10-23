#ifndef AB_MENU_BACKGROUND_PREVIEW_H
#define AB_MENU_BACKGROUND_PREVIEW_H

/* The actual menu theme's layer/camera/bird component. Effects and
 * source menu-root service placements are separate integrations. This preview
 * retains the requested effects theme explicitly rather than claiming an
 * effects subsystem has been installed. Resource work runs only at open. */
static void menu_background_free(PreviewMenu *menu) {
    AbError error = {0};
    ab_theme_layers_close(menu->theme_layers);
    ab_target_data_close(&menu->theme_data);
    if (menu->white_uploaded)
        (void)menu->texture_host.remove(menu->texture_host.context, 63, &error);
}

static bool menu_theme_call(void *context, AbMainMenuThemeCall operation, AbSpan theme,
                            const float *numbers, size_t count, AbError *error) {
    PreviewMenu *menu = context;
    if (operation == AB_MAIN_MENU_CAMERA_ZOOM && count == 1) {
        menu->menu_camera.zoom = (AbVec2){numbers[0], numbers[0]};
        return true;
    }
    if (operation == AB_MAIN_MENU_CAMERA_TOP_LEFT && count == 2) {
        menu->menu_camera.top_left = (AbVec2){numbers[0], numbers[1]};
        return true;
    }
    if (operation == AB_MAIN_MENU_SELECT_EFFECTS && !count) {
        menu->requested_effects = theme;
        return true;
    }
    if (operation == AB_MAIN_MENU_SELECT_THEME && !count &&
        theme.size == menu->theme_name.size &&
        !memcmp(theme.data, menu->theme_name.data, theme.size))
        return true;
    return ab_fail(error, "Unsupported prepared menu-theme component operation");
}

static bool menu_background_layout(PreviewMenu *menu, AbError *error) {
    AbMainMenuResources resources = {.themes = menu->theme_data.data.globals,
                                     .settings = &menu->menu_settings,
                                     .screen_height = menu->screen.y,
                                     .scroll = menu->menu_scroll,
                                     .context = menu,
                                     .call = menu_theme_call};
    return ab_main_menu_resources_layout_provider(&menu->menu_theme, &resources,
                                                  &menu->sprites.provider,
                                                  &menu->camera_top, error);
}

static const AbDataTable *menu_theme_graphics(PreviewMenu *menu, AbSpan name,
                                              const AbDataTable **record) {
    const AbDataValue *value =
        ab_data_get(menu->theme_data.data.globals,
                    (AbDataValue){.kind = AB_DATA_STRING, .string = name});
    if (!value || value->kind != AB_DATA_TABLE || !value->table)
        return NULL;
    *record = value->table;
    value = ab_data_get(*record, menu_data("graphicSetName"));
    return value && value->kind == AB_DATA_TABLE ? value->table : NULL;
}

/* Resource transitions run in Loader's callable, outside ordinary menu draw.
 * Cached theme bindings must be discarded before releasing their source group. */
static bool menu_theme_select(PreviewMenu *menu, AbSpan name, AbError *error) {
    if (menu->theme_layers && name.size == menu->theme_name.size &&
        !memcmp(name.data, menu->theme_name.data, name.size))
        return true;
    const AbDataTable *record = NULL, *old_record = NULL;
    const AbDataTable *graphics = menu_theme_graphics(menu, name, &record);
    if (!graphics)
        return ab_fail(error, "Missing selected Android menu-theme resource recipe");
    /* Level theme strings expire with that level. The prepared theme catalog
     * owns the same key for the whole menu lifetime, including result/reload
     * transitions and settings observers after the level has been released. */
    const AbDataTable *themes = menu->theme_data.data.globals;
    size_t key = 0;
    for (; key < themes->count; ++key) {
        AbDataValue candidate = themes->entries[key].key;
        if (candidate.kind == AB_DATA_STRING && candidate.string.size == name.size &&
            !memcmp(candidate.string.data, name.data, name.size)) {
            name = candidate.string;
            break;
        }
    }
    if (key == themes->count)
        return ab_fail(error, "Missing stable theme catalog key");
    const AbDataTable *old_graphics =
        menu->theme_layers ? menu_theme_graphics(menu, menu->theme_name, &old_record)
                           : NULL;
    ab_theme_layers_close(menu->theme_layers);
    menu->theme_layers = NULL;
    menu->theme_draw = (AbThemeDraw){0};
    if ((old_graphics &&
         !ab_android_assets_manage(menu->planning, old_graphics, false,
                                   ab_asset_resources_host, menu->resources, error)) ||
        !ab_android_assets_manage(menu->planning, graphics, true,
                                  ab_asset_resources_host, menu->resources, error) ||
        !ab_asset_textures_sync(menu->textures,
                                ab_asset_resources_owners(menu->resources), error) ||
        !ab_theme_layers_open_provider(record, &menu->sprites.provider, 1024 * 1024,
                                       &menu->theme_layers, error))
        return false;
    menu->theme_name = name;
    const AbDataValue *song = ab_data_get(record, menu_data("menuMusic"));
    menu->main_song = song && song->kind != AB_DATA_NIL &&
                              (song->kind != AB_DATA_BOOLEAN || song->bits)
                          ? *song
                          : menu_data("title_theme");
    if (!ab_main_menu_settings_set_theme(
            &menu->menu_settings,
            (AbUiPointerValue){.kind = AB_UI_POINTER_TEXT, .text = name}) ||
        !menu_background_layout(menu, error))
        return false;
    return ab_theme_draw_initialize(
        (AbThemeDrawConfig){menu->theme_layers, &menu->sprites.provider,
                            &menu->theme_render, &menu->background_argb, 63, 1024},
        &menu->theme_draw, error);
}

static bool menu_load_theme(void *context, AbSpan theme, AbError *error) {
    PreviewMenu *menu = context;
    return menu->background_enabled
               ? menu_theme_select(menu, theme, error)
               : ab_fail(error, "Episode Loader requires prepared theme services");
}

static bool menu_background_open(PreviewMenu *menu, AbError *error) {
    if (!menu->episodes && !menu_episode_open(menu, error))
        return false;
    AbSpan key = {menu->key, sizeof(menu->key)};
    if (!ab_target_data_open(menu->archive, "assets/data/scripts/themes.lua", key,
                             4 * 1024 * 1024, NULL, &menu->theme_data, error))
        return false;
    const AbDataValue *settings =
        ab_data_get(menu->theme_data.data.globals, menu_data("settings"));
    const AbDataValue *name =
        settings && settings->kind == AB_DATA_TABLE
            ? ab_data_get(settings->table, menu_data("defaultTheme"))
            : NULL;
    AbUiPointerValue fallback =
        name && name->kind == AB_DATA_STRING
            ? (AbUiPointerValue){.kind = AB_UI_POINTER_TEXT, .text = name->string}
            : (AbUiPointerValue){0};
    AbUiPointerValue chosen;
    if (!ab_main_menu_settings_theme(&menu->menu_settings, fallback, NULL, NULL,
                                     &chosen, error) ||
        chosen.kind != AB_UI_POINTER_TEXT)
        return ab_fail(error, "Unsupported saved menu-theme selection");
    uint8_t pixels[] = {255, 255, 255, 255};
    const AbImage white = {1, 1, {pixels, sizeof(pixels)}};
    if (!menu->texture_host.upload(
            menu->texture_host.context, 63,
            (AbSpriteAtlasImage){.image = &white, .alpha = AB_SPRITE_ALPHA_OPAQUE},
            error))
        return false;
    menu->white_uploaded = true;
    if (!menu_theme_select(menu, chosen.text, error))
        return false;
    if (!ab_ui_menu_birds_init(&menu->birds, &menu->random, error) ||
        !ab_ui_menu_birds_build(&menu->birds, menu->settings, &menu->statistics,
                                ab_episode_catalog_feather_ids(menu->catalog),
                                menu->screen, menu_span("android"), false, error))
        return false;
    menu->background_enabled = true;
    return menu_audio_initialize(menu, error);
}

static bool menu_background_draw(PreviewMenu *menu, float seconds, AbError *error) {
    if (!menu_audio_music(menu, error))
        return false;
    menu->menu_scroll = menu->menu_scroll + 50 * seconds;
    menu->menu_camera.top_left = (AbVec2){menu->menu_scroll, menu->camera_top};
    if (!ab_ui_menu_birds_update(&menu->birds, &menu->context, menu->screen, seconds,
                                 error))
        return false;
    AbThemeDrawInput input = {.camera = menu->menu_camera,
                              .width = (uint32_t)menu->screen.x,
                              .height = (uint32_t)menu->screen.y,
                              .time = menu->time};
    menu->theme_render =
        (AbThemeLayerRenderState){menu->render.geometry, menu->render.alpha};
    if (!ab_theme_draw_background(&menu->theme_draw, input, menu->context.draw_list,
                                  error))
        return false;
    menu->render.geometry = menu->theme_render.geometry;
    menu->render.alpha = menu->theme_render.alpha;
    if (!ab_ui_menu_birds_draw(&menu->birds, &menu->context, &menu->sprites.provider,
                               menu->screen, error))
        return false;
    menu->theme_render =
        (AbThemeLayerRenderState){menu->render.geometry, menu->render.alpha};
    bool okay = ab_theme_draw_foreground(&menu->theme_draw, input,
                                         menu->context.draw_list, error);
    menu->render.geometry = menu->theme_render.geometry;
    menu->render.alpha = menu->theme_render.alpha;
    return okay;
}

static void menu_background_clear(const PreviewMenu *menu, float clear[4]) {
    if (menu->cutscene_active) {
        clear[0] = clear[1] = clear[2] = 0;
        clear[3] = 1;
        return;
    }
    const float reciprocal = 1.0f / 255.0f;
    clear[0] = (float)((menu->background_argb >> 16) & 255) * reciprocal;
    clear[1] = (float)((menu->background_argb >> 8) & 255) * reciprocal;
    clear[2] = (float)(menu->background_argb & 255) * reciprocal;
    clear[3] = 1;
}

#endif
