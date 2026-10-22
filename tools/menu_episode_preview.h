#ifndef AB_MENU_EPISODE_PREVIEW_H
#define AB_MENU_EPISODE_PREVIEW_H

/* Composition of recovered controls with explicit offline Android feature
 * inputs. Optional preview-owned saves restore before preference initialization;
 * full bootstrap and SDK initialization remain separate. */
static AbDataValue menu_data(const char *name) {
    return (AbDataValue){.kind = AB_DATA_STRING, .string = menu_span(name)};
}

static AbDataValue menu_event(PreviewMenu *menu, const char *name) {
    const AbDataValue *value =
        ab_data_get(menu->event_data.data.globals, menu_data(name));
    return value ? *value : (AbDataValue){0};
}

static bool menu_episode_event(void *context, AbDataTable *event, AbError *error) {
    PreviewMenu *menu = context;
    const AbDataValue *target = ab_data_get(event, menu_data("target"));
    if (target && target->kind == AB_DATA_STRING && target->string.size == 22 &&
        !memcmp(target->string.data, "EXIT_EPISODE_SELECTION", 22))
        menu->pending_main = true;
    return menu_route_event(menu, event, error);
}

static bool menu_episode_change(PreviewMenu *menu, bool episodes, AbError *error) {
    AbUiWidget *old = menu_active_root(menu);
    AbUiWidget *next = episodes ? &menu->episodes->widget : &menu->root;
    if (old == next)
        return true;
    if (!ab_ui_widget_lifecycle(old, AB_UI_FRAME_EXIT, error))
        return false;
    menu->episode_active = episodes;
    menu->level_active = false;
    menu->cutscene_active = false;
    if (!ab_ui_widget_clear_pointer_flags(next, error) ||
        !ab_ui_widget_lifecycle(next, AB_UI_FRAME_ENTRY, error) ||
        !ab_ui_widget_lifecycle(next, AB_UI_FRAME_LAYOUT, error))
        return false;
    return !menu->background_enabled ||
           ab_ui_menu_birds_build(&menu->birds, menu->settings, &menu->statistics,
                                  ab_episode_catalog_feather_ids(menu->catalog),
                                  menu->screen, menu_span("android"), false, error);
}

static void menu_episode_free(PreviewMenu *menu) {
    AbError error = {0};
    if (menu->episodes)
        (void)ab_ui_episode_selection_detach(menu->episodes, &error);
    free(menu->episodes);
    menu->episodes = NULL;
    if (menu->navigation.event_triggered)
        (void)ab_event_manager_remove(menu->events,
                                      menu_event(menu, "EID_CHANGE_SCENE"),
                                      &menu->navigation, &error);
    const char *const observed[] = {
        "EID_DO_LOADING",          "EID_LOAD_INTRO_CUTSCENE", "EID_PLAY_INTRO_CUTSCENE",
        "EID_CHANGE_LEVEL",        "EID_SHOW_LOADING_PAGE",   "EID_HIDE_LOADING_PAGE",
        "EID_GOLDEN_EGG_FROM_MENU"};
    for (size_t index = 0; index < sizeof(observed) / sizeof(observed[0]); ++index)
        if (menu->navigation.event_triggered)
            (void)ab_event_manager_remove(menu->events,
                                          menu_event(menu, observed[index]),
                                          &menu->navigation, &error);
    for (size_t index = 0; index < menu->uploaded_fonts; ++index)
        (void)menu->texture_host.remove(menu->texture_host.context,
                                        61 + (uint32_t)index, &error);
    for (size_t index = 0; index < 2; ++index)
        ab_font_asset_close(menu->font_assets + index);
    ab_settings_statistics_close(menu->local_settings);
    ab_settings_statistics_close(menu->settings);
    ab_settings_statistics_close(menu->highscores);
    ab_settings_file_close(menu->local_file);
    ab_settings_file_close(menu->settings_file);
    ab_settings_file_close(menu->highscores_file);
    ab_episode_catalog_close(menu->catalog);
    ab_target_data_close(&menu->star_limits);
}

static bool menu_episode_open(PreviewMenu *menu, AbError *error) {
    const AbZip *archive = menu->archive;
    AbSpan key = {menu->key, sizeof(menu->key)};
    const AbDataTable *requirements = ab_dynamic_asset_requirements_get(
        &menu->requirements, menu_span("LevelSelectionRoot"));
    if (!requirements ||
        !ab_android_assets_manage(menu->planning, requirements, true,
                                  ab_asset_resources_host, menu->resources, error) ||
        !ab_asset_textures_sync(menu->textures,
                                ab_asset_resources_owners(menu->resources), error) ||
        ab_asset_textures_count(menu->textures) >= 61 ||
        !ab_episode_catalog_open(
            archive, key,
            (AbEpisodeCatalogConfig){.device_model = menu_span("android"),
                                     .disable_external_links = true},
            2 * 1024 * 1024, &menu->catalog, error) ||
        !ab_episode_catalog_annotate_levels(menu->catalog, error) ||
        !ab_target_data_open(archive, "assets/data/scripts/starLimits.lua", key,
                             3 * 1024 * 1024, NULL, &menu->star_limits, error))
        return false;
    const char *const low[] = {"assets/data/fonts/480x320/FONT_BASIC.dat",
                               "assets/data/fonts/480x320/FONT_LS_SMALL.dat"};
    const char *const high[] = {"assets/data/fonts/1024x768/FONT_BASIC.dat",
                                "assets/data/fonts/1024x768/FONT_LS_SMALL.dat"};
    const char *const *paths = menu->highest_resources ? high : low;
    const char *const names[] = {"FONT_BASIC", "FONT_LS_SMALL"};
    for (size_t index = 0; index < 2; ++index) {
        AbFontAsset *asset = menu->font_assets + index;
        if (!ab_font_asset_open(archive, paths[index], 8 * 1024 * 1024, 2 * 1024 * 1024,
                                asset, error) ||
            !menu->texture_host.upload(
                menu->texture_host.context, 61 + (uint32_t)index,
                (AbSpriteAtlasImage){.image = &asset->image,
                                     .alpha = asset->has_alpha
                                                  ? AB_SPRITE_ALPHA_PRESENT
                                                  : AB_SPRITE_ALPHA_OPAQUE},
                error))
            return false;
        ++menu->uploaded_fonts;
        menu->font_bindings[index] = (AbUiFontBinding){
            menu_span(names[index]), &asset->font, 61 + (uint32_t)index,
            asset->image.width, asset->image.height};
    }
    menu->fonts = (AbUiFontResources){.bindings = menu->font_bindings,
                                      .count = 2,
                                      .strings = &menu->text_resources,
                                      .codes = menu->codes,
                                      .glyphs = menu->glyphs,
                                      .capacity = 128,
                                      .screen_width = menu->screen.x};
    if (!menu_settings_open(menu, error) ||
        !ab_episode_preferences_init(&menu->preferences, menu->local_settings,
                                     ab_episode_catalog_episodes(menu->catalog), error))
        return false;
    menu->margin_fields[0] = (AbDataEntry){menu_data("x"), {.kind = AB_DATA_NUMBER}};
    menu->margin_fields[1] = (AbDataEntry){menu_data("y"), {.kind = AB_DATA_NUMBER}};
    menu->margin = (AbDataTable){menu->margin_fields, 2, 2, NULL};
    const char *const flags[] = {"g_use_new_layout",
                                 "g_episode_selection_use_first_card_width_for_all",
                                 "g_episode_selection_use_scroll_buttons"};
    for (size_t index = 0; index < 3; ++index)
        menu->features[index] = (AbDataEntry){menu_data(flags[index]),
                                              {.kind = AB_DATA_BOOLEAN, .bits = 1}};
    menu->features[3] =
        (AbDataEntry){menu_data("g_episode_selection_button_padding"),
                      {.kind = AB_DATA_NUMBER, .bits = menu_number(0.04f).number_bits}};
    menu->features[4] = (AbDataEntry){menu_data("g_freeEagleUnlockStatus"),
                                      {.kind = AB_DATA_TABLE, .table = &menu->empty}};
    menu->features[5] = (AbDataEntry){menu_data("g_layoutMargin"),
                                      {.kind = AB_DATA_TABLE, .table = &menu->margin}};
    menu->episode_globals = (AbDataTable){menu->features, 6, 8, NULL};
    const AbDataTable *episodes = ab_episode_catalog_episodes(menu->catalog);
    menu->statistics = (AbEpisodeStatistics){
        episodes, ab_episode_catalog_ids(menu->catalog),
        ab_settings_statistics_root(menu->highscores), menu->star_limits.data.globals};
    menu->progress =
        (AbEpisodeProgress){episodes, ab_settings_statistics_root(menu->local_settings),
                            &menu->episode_globals, &menu->empty};
    menu->episode_services = (AbUiEpisodeServices){&menu->statistics, &menu->progress};
    AbUiEpisodeButtonHost buttons =
        ab_ui_episode_services_host(&menu->episode_services);
    buttons.fonts = &menu->fonts;
    buttons.scaling = &menu->scaling;
    buttons.random = &menu->random;
    buttons.ugc_class = &menu->ugc;
    buttons.settings = ab_settings_statistics_root(menu->settings);
    menu->episodes = calloc(1, sizeof(*menu->episodes));
    if (!menu->episodes)
        return ab_fail(error, "Cannot allocate episode preview controls");
    AbUiEpisodeScrollHost cards = {
        .buttons = buttons,
        .scroll = {.events = menu->events,
                   .pause_event = menu_event(menu, "EID_GAME_PAUSED"),
                   .input_seconds = &menu->input_seconds,
                   .wheel = &menu->wheel},
        .episode_ids = ab_episode_catalog_ids(menu->catalog),
        .globals = &menu->episode_globals,
        .progress = &menu->progress,
        .extras_changed = menu_event(menu, "EID_EPISODE_EXTRAS_CHANGED"),
        .free_version_unlocked = menu_event(menu, "EID_FREE_VERSION_UNLOCKED")};
    if (!ab_ui_episode_selection_init(
            menu->episodes, &menu->context,
            (AbUiEpisodeSelectionHost){.cards = cards,
                                       .preferences = &menu->preferences,
                                       .event_ids = menu->event_data.data.globals,
                                       .context = menu,
                                       .load_theme = menu_load_theme},
            error))
        return false;
    menu->navigation =
        (AbEventListener){.context = menu, .event_triggered = menu_episode_event};
    const char *const observed[] = {
        "EID_CHANGE_SCENE",      "EID_DO_LOADING",          "EID_LOAD_INTRO_CUTSCENE",
        "EID_CHANGE_LEVEL",      "EID_PLAY_INTRO_CUTSCENE", "EID_SHOW_LOADING_PAGE",
        "EID_HIDE_LOADING_PAGE", "EID_GOLDEN_EGG_FROM_MENU"};
    for (size_t index = 0; index < sizeof(observed) / sizeof(observed[0]); ++index)
        if (!ab_event_manager_add(menu->events, menu_event(menu, observed[index]),
                                  &menu->navigation, error))
            return false;
    return true;
}

#endif
