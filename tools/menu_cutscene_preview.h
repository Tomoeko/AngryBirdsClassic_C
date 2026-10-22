#ifndef AB_MENU_CUTSCENE_PREVIEW_H
#define AB_MENU_CUTSCENE_PREVIEW_H

/* GameluaMenuListener.eventTriggered3611 instructions 621..704: retain the
 * selected meta, queue one Loader callable, then construct CutScene with a
 * distinct EID_CHANGE_LEVEL completion event. This prepared dynamic-resource
 * host covers pack1_intro; legacy global resource release and other cutscene
 * command/override domains remain separate. */
static bool menu_cutscene_assets(void *context, AbError *error) {
    PreviewMenu *menu = context;
    const AbDataTable *requirements =
        ab_dynamic_asset_requirements_get(&menu->requirements, menu_span("CutScene"));
    if (!requirements ||
        !ab_android_assets_manage(menu->planning, requirements, true,
                                  ab_asset_resources_host, menu->resources, error) ||
        !ab_asset_textures_sync(menu->textures,
                                ab_asset_resources_owners(menu->resources), error))
        return false;
    return menu->cutscene_data.data.globals ||
           ab_target_data_open(menu->archive, "assets/data/scripts/cutscenes.lua",
                               (AbSpan){menu->key, sizeof(menu->key)}, 4 * 1024 * 1024,
                               NULL, &menu->cutscene_data, error);
}

static bool menu_cutscene_begin(PreviewMenu *menu, AbDataTable *event, AbError *error) {
    AbDataValue cutscene = menu_route_field(event, "cutscene");
    AbDataValue data = menu_route_field(event, "data");
    if (!menu->level_active || data.kind != AB_DATA_TABLE || !data.table ||
        !menu_route_equal(cutscene, menu_data("pack1_intro")))
        return ab_fail(error, "Intro route requires the recovered pack1 profile");
    AbDataValue episode = menu_route_field(data.table, "episode");
    const AbDataValue *record =
        ab_data_get(ab_episode_catalog_episodes(menu->catalog), episode);
    if (!record || record->kind != AB_DATA_TABLE ||
        menu_route_truthy(
            menu_route_field(record->table, "cutscene_asset_group_override")))
        return ab_fail(error, "Intro asset override requires its source binding");
    menu->intro_play_fields[0] =
        (AbDataEntry){menu_data("id"), menu_event(menu, "EID_PLAY_INTRO_CUTSCENE")};
    menu->intro_play_fields[1] = (AbDataEntry){menu_data("cutscene"), cutscene};
    menu->intro_play_fields[2] = (AbDataEntry){menu_data("data"), data};
    menu->intro_play = (AbDataTable){menu->intro_play_fields, 3, 3, NULL};
    menu->intro_call =
        (AbClassicLoaderCall){.context = menu, .call = menu_cutscene_assets};
    menu->intro_calls_storage[0] = &menu->intro_call;
    menu->intro_calls_storage[1] = NULL;
    menu->intro_calls = (AbClassicLoaderCalls){menu->intro_calls_storage, 2};
    menu->intro_item = (AbDataEntry){
        menu_route_number(1), {.kind = AB_DATA_OBJECT, .object = &menu->intro_call}};
    menu->intro_items = (AbDataTable){&menu->intro_item, 1, 1, NULL};
    menu->intro_loading_fields[0] =
        (AbDataEntry){menu_data("id"), menu_event(menu, "EID_DO_LOADING")};
    menu->intro_loading_fields[1] = (AbDataEntry){
        menu_data("items"), {.kind = AB_DATA_TABLE, .table = &menu->intro_items}};
    menu->intro_loading_fields[2] =
        (AbDataEntry){menu_data("completion_event"),
                      {.kind = AB_DATA_TABLE, .table = &menu->intro_play}};
    menu->intro_loading = (AbDataTable){menu->intro_loading_fields, 3, 3, NULL};
    return ab_event_manager_notify(menu->events, &menu->intro_loading, error);
}

static bool menu_cutscene_enter(PreviewMenu *menu, AbError *error) {
    AbDataValue name = menu_route_field(&menu->intro_play, "cutscene");
    AbDataValue data = menu_route_field(&menu->intro_play, "data");
    const AbDataValue *commands = ab_data_get(menu->cutscene_data.data.globals, name);
    if (!commands || commands->kind != AB_DATA_TABLE || !commands->table ||
        name.kind != AB_DATA_STRING || data.kind != AB_DATA_TABLE ||
        !menu->white_uploaded)
        return ab_fail(error, "Intro completion requires prepared data and resources");
    if (!ab_ui_widget_lifecycle(menu_active_root(menu), AB_UI_FRAME_EXIT, error))
        return false;
    free(menu->cutscene);
    menu->cutscene = calloc(1, sizeof(*menu->cutscene));
    if (!menu->cutscene)
        return ab_fail(error, "Cannot allocate the intro root");
    menu->intro_end_fields[0] =
        (AbDataEntry){menu_data("id"), menu_event(menu, "EID_CHANGE_LEVEL")};
    menu->intro_end_fields[1] = (AbDataEntry){menu_data("data"), data};
    menu->intro_end = (AbDataTable){menu->intro_end_fields, 2, 2, NULL};
    AbUiCutsceneHost host = {.scaling = &menu->scaling,
                             .shared = &menu->cutscene_class,
                             .audio = menu->audio,
                             .mixer = menu->mixer,
                             .settings = menu->settings,
                             .events = menu->events,
                             .end_event = &menu->intro_end,
                             .commands = commands->table,
                             .name = name.string,
                             .device = menu_span("android"),
                             .white_texture = 63,
                             .context = menu,
                             .save_settings = menu_cutscene_save};
    if (!ab_ui_cutscene_init(menu->cutscene, &menu->context, host, error) ||
        !ab_ui_widget_lifecycle(&menu->cutscene->widget, AB_UI_FRAME_ENTRY, error) ||
        !ab_ui_widget_lifecycle(&menu->cutscene->widget, AB_UI_FRAME_LAYOUT, error))
        return false;
    menu->cutscene_active = true;
    menu->level_active = menu->episode_active = false;
    return true;
}

#endif
