#ifndef AB_MENU_ROUTE_PREVIEW_H
#define AB_MENU_ROUTE_PREVIEW_H

static AbDataValue menu_route_number(float value) {
    return (AbDataValue){.kind = AB_DATA_NUMBER,
                         .bits = menu_number(value).number_bits};
}

static bool menu_route_equal(AbDataValue left, AbDataValue right) {
    AbDataEntry entry = {left, {.kind = AB_DATA_BOOLEAN, .bits = 1}};
    AbDataTable singleton = {&entry, 1, 1, NULL};
    return ab_data_get(&singleton, right) != NULL;
}

static bool menu_route_truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL && (value.kind != AB_DATA_BOOLEAN || value.bits);
}

static AbDataValue menu_route_field(const AbDataTable *table, const char *name) {
    const AbDataValue *value = ab_data_get(table, menu_data(name));
    return value ? *value : (AbDataValue){0};
}

static void menu_route_free(PreviewMenu *menu) {
    AbError cleanup = {0};
    ab_classic_loader_close(menu->loader);
    menu->loader = NULL;
    if (menu->levels)
        (void)ab_ui_level_selection_detach(menu->levels, &cleanup);
    free(menu->levels);
    menu->levels = NULL;
    free(menu->hidden_eggs);
    menu->hidden_eggs = NULL;
    free(menu->rmf_levels);
    menu->rmf_levels = NULL;
    menu_pigs_free(menu);
    free(menu->egg_levels);
    menu->egg_levels = NULL;
    free(menu->cutscene);
    menu->cutscene = NULL;
    ab_target_data_close(&menu->cutscene_data);
}

static bool menu_route_start_loader(PreviewMenu *menu, AbClassicLoaderCalls *calls,
                                    AbDataValue completion, AbError *error) {
    if (menu->loader)
        return ab_fail(error, "A preview Loader operation is already active");
    menu->do_loading = menu_event(menu, "EID_DO_LOADING");
    menu->show_loading = menu_event(menu, "EID_SHOW_LOADING_PAGE");
    menu->hide_loading = menu_event(menu, "EID_HIDE_LOADING_PAGE");
    menu->loader_time = menu_route_number(menu->time);
    menu->loader_state = (AbClassicLoaderState){.loading = {.kind = AB_DATA_BOOLEAN},
                                                .frame = menu_route_number(0)};
    if (!ab_classic_loader_open(
            (AbClassicLoaderConfig){.state = &menu->loader_state,
                                    .script_time = &menu->loader_time,
                                    .do_loading = &menu->do_loading,
                                    .show_loading = &menu->show_loading,
                                    .hide_loading = &menu->hide_loading,
                                    .skip_input = &menu->skip_input,
                                    .events = menu->events,
                                    .notifications = 8,
                                    .storage_bytes = 4096},
            &menu->loader, error))
        return false;
    menu->loader_request = (AbClassicLoaderRequest){
        .id = menu->do_loading, .completion_event = completion, .items = calls};
    return ab_classic_loader_event(menu->loader, &menu->loader_request, error);
}

#include "menu_cutscene_preview.h"

static bool menu_route_load(PreviewMenu *menu, AbDataTable *event, AbError *error) {
    AbUiEpisodeSelection *owner = menu->episodes;
    AbDataValue items = menu_route_field(event, "items");
    AbDataValue completion = menu_route_field(event, "completion_event");
    if (event == &menu->intro_loading && items.kind == AB_DATA_TABLE &&
        items.table == &menu->intro_items && completion.kind == AB_DATA_TABLE &&
        completion.table == &menu->intro_play &&
        menu->intro_item.value.kind == AB_DATA_OBJECT &&
        menu->intro_item.value.object == &menu->intro_call)
        return menu_route_start_loader(menu, &menu->intro_calls, completion, error);
    if (!owner || items.kind != AB_DATA_TABLE || items.table != &owner->loading_items ||
        completion.kind != AB_DATA_TABLE || completion.table != &owner->completion ||
        owner->item.value.kind != AB_DATA_OBJECT ||
        owner->item.value.object != &owner->load)
        return ab_fail(error, "Preview Loader requires an owned source request");
    return menu_route_start_loader(menu, &owner->loading_calls, completion, error);
}

static bool menu_route_event(PreviewMenu *menu, AbDataTable *event, AbError *error) {
    if (!menu->route_enabled)
        return true;
    AbDataValue id = menu_route_field(event, "id");
    if (menu_route_equal(id, menu_event(menu, "EID_GOLDEN_EGG_FROM_MENU")))
        return menu_reward_reveal(menu, event, error);
    if (menu_route_equal(id, menu_event(menu, "EID_DO_LOADING")))
        return menu_route_load(menu, event, error);
    if (menu_route_equal(id, menu_event(menu, "EID_CHANGE_SCENE"))) {
        AbDataValue target = menu_route_field(event, "target");
        if (menu_route_equal(target, menu_data("EPISODE_SELECTION")) ||
            (target.kind == AB_DATA_STRING && target.string.size > 16 &&
             !memcmp(target.string.data, "LEVEL_SELECTION_", 16)))
            menu->pending_scene = target;
    } else if (menu_route_equal(id, menu_event(menu, "EID_SHOW_LOADING_PAGE"))) {
        menu->loading_page = true;
    } else if (menu_route_equal(id, menu_event(menu, "EID_HIDE_LOADING_PAGE"))) {
        menu->loading_page = false;
    } else if (menu_route_equal(id, menu_event(menu, "EID_PLAY_INTRO_CUTSCENE"))) {
        if (event != &menu->intro_play)
            return ab_fail(error, "Intro completion requires its retained event");
        menu->pending_intro = event;
    } else if (menu_route_equal(id, menu_event(menu, "EID_LOAD_INTRO_CUTSCENE")) ||
               menu_route_equal(id, menu_event(menu, "EID_CHANGE_LEVEL"))) {
        /* Keep the canonical meta owner alive through the deferred transition. */
        menu->requested_launch = event;
        menu->launch_pending = menu->launch_enabled;
    }
    return true;
}

/* settingsWrapper.shouldUseAgeFilter48481: nil info is false; an existing
 * record filters only the strict Boolean isUnderThirteen=true field. */
static bool menu_level_age_filter(void *context, bool *output, AbError *error) {
    PreviewMenu *menu = context;
    AbDataValue info =
        menu_route_field(ab_settings_statistics_root(menu->settings), "ageGenderInfo");
    if (info.kind == AB_DATA_NIL) {
        *output = false;
        return true;
    }
    if (info.kind != AB_DATA_TABLE)
        return ab_fail(error, "Age filter requires the actual settings record");
    AbDataValue under = menu_route_field(info.table, "isUnderThirteen");
    *output = under.kind == AB_DATA_BOOLEAN && under.bits != 0;
    return true;
}

static bool menu_eggs_assets(void *context, bool loading, AbError *error) {
    PreviewMenu *menu = context;
    AbDataEntry entry = {menu_route_number(1), menu_data("LEVELSELECTION_GOLDEN_EGGS")};
    AbDataTable request = {&entry, 1, 1, NULL};
    return ab_android_assets_manage(menu->planning, &request, loading,
                                    ab_asset_resources_host, menu->resources, error) &&
           ab_asset_textures_sync(menu->textures,
                                  ab_asset_resources_owners(menu->resources), error);
}

static bool menu_route_level(PreviewMenu *menu, AbError *error) {
    AbDataValue episode = menu->episodes->selected_episode;
    AbDataValue record = {0};
    const AbDataValue *found =
        ab_data_get(ab_episode_catalog_episodes(menu->catalog), episode);
    if (found)
        record = *found;
    if (record.kind != AB_DATA_TABLE || !record.table)
        return ab_fail(error, "Missing selected episode for level-screen route");
    AbDataValue subclass = menu_route_field(record.table, "level_selection_subclass");
    bool danger = menu_route_equal(subclass, menu_data("DangerAboveLevelSelection"));
    bool mine = menu_route_equal(subclass, menu_data("MineAndDineLevelSelection"));
    bool rmf = menu_route_equal(subclass, menu_data("RMFLevelSelection"));
    bool pigs = menu_route_equal(subclass, menu_data("BadPiggiesLevelSelection"));
    bool eggs = menu_route_equal(subclass, menu_data("GoldenEggsLevelSelection"));
    if (menu_route_truthy(subclass) && !danger && !mine && !rmf && !pigs && !eggs)
        return ab_fail(error,
                       "Selected episode requires its source level-screen subclass");
    AbDataValue exit = menu_route_field(record.table, "exit_link_override");
    if (!menu_route_truthy(exit))
        exit = menu_data("EPISODE_SELECTION");
    if (!ab_ui_widget_lifecycle(menu_active_root(menu), AB_UI_FRAME_EXIT, error))
        return false;
    if (menu->levels) {
        if (!ab_ui_level_selection_detach(menu->levels, error))
            return false;
        free(menu->levels);
        menu->levels = NULL;
    }
    free(menu->hidden_eggs);
    menu->hidden_eggs = NULL;
    free(menu->rmf_levels);
    menu->rmf_levels = NULL;
    menu_pigs_free(menu);
    free(menu->egg_levels);
    menu->egg_levels = NULL;
    menu->requested_launch = NULL;
    menu->level_request_ready = false;
    menu->level_services = (AbUiLevelServices){
        .statistics = &menu->statistics,
        .local_settings = ab_settings_statistics_root(menu->local_settings)};
    AbUiLevelButtonHost buttons = ab_ui_level_services_host(&menu->level_services);
    buttons.fonts = &menu->fonts;
    buttons.scaling = &menu->scaling;
    menu->level_progress = (AbLevelProgress){.episodes = menu->statistics.episodes,
                                             .highscores = menu->statistics.highscores,
                                             .release_build = true};
    AbUiLevelSelectionHost host = {
        .levels = {.buttons = buttons,
                   .progress = &menu->level_progress,
                   .scroll = {.events = menu->events,
                              .pause_event = menu_event(menu, "EID_GAME_PAUSED"),
                              .input_seconds = &menu->input_seconds,
                              .wheel = &menu->wheel},
                   .event_ids = menu->event_data.data.globals,
                   .initial_time = menu->time,
                   .context = menu,
                   .age_filter = menu_level_age_filter,
                   .golden_eggs = menu->highscores,
                   .arrows = true},
        .preferences = &menu->preferences,
        .episode_progress = &menu->progress,
        .exit_link = exit,
        .script_time = &menu->time,
        .interpolate_background = true};
    menu->levels = calloc(1, sizeof(*menu->levels));
    if (!menu->levels)
        return ab_fail(error, "Cannot allocate level-screen route controls");
    if (!ab_ui_level_selection_init(menu->levels, &menu->context, episode, host, error))
        return false;
    if (danger || mine) {
        menu->hidden_eggs = calloc(1, sizeof(*menu->hidden_eggs));
        if (!menu->hidden_eggs)
            return ab_fail(error, "Cannot allocate selected hidden-egg screen");
        if (!ab_ui_level_hidden_eggs_attach(
                menu->hidden_eggs, menu->levels, menu->highscores,
                danger ? AB_UI_LEVEL_DANGER_ABOVE : AB_UI_LEVEL_MINE_AND_DINE, error))
            return false;
    }
    if (rmf) {
        menu->rmf_levels = calloc(1, sizeof(*menu->rmf_levels));
        if (!menu->rmf_levels ||
            !ab_ui_level_rmf_attach(menu->rmf_levels, menu->levels, 63, error))
            return error && error->message[0]
                       ? false
                       : ab_fail(error, "Cannot allocate RMF level-screen controls");
    }
    if (pigs && !menu_pigs_attach(menu, error))
        return false;
    if (eggs) {
        menu->egg_levels = calloc(1, sizeof(*menu->egg_levels));
        if (!menu->egg_levels ||
            !ab_ui_level_eggs_attach(
                menu->egg_levels, menu->levels,
                (AbUiLevelEggsHost){.highscores = menu->highscores,
                                    .statistics = &menu->statistics,
                                    .context = menu,
                                    .assets = menu_eggs_assets,
                                    .king_pig_enabled = true,
                                    .release_build = true},
                error))
            return error && error->message[0]
                       ? false
                       : ab_fail(error, "Cannot allocate Golden Eggs screen");
    }
    if (!ab_ui_widget_lifecycle(&menu->levels->widget, AB_UI_FRAME_ENTRY, error) ||
        !ab_ui_widget_lifecycle(&menu->levels->widget, AB_UI_FRAME_LAYOUT, error))
        return false;
    menu->level_active = true;
    menu->episode_active = false;
    menu->cutscene_active = false;
    return true;
}

/* Apply scene changes after pointer/update traversal and Loader callbacks return.
 * Event payloads remain owned by the old scene until all consumers finish. */
static bool menu_route_apply(PreviewMenu *menu, AbError *error) {
    if (!menu->route_enabled)
        return true;
    if (menu->pending_intro) {
        menu->pending_intro = NULL;
        return menu_cutscene_enter(menu, error);
    }
    if (menu->launch_pending) {
        menu->launch_pending = false;
        AbDataValue id = menu_route_field(menu->requested_launch, "id");
        if (menu_route_equal(id, menu_event(menu, "EID_LOAD_INTRO_CUTSCENE")))
            return menu_cutscene_begin(menu, menu->requested_launch, error);
        if (menu_route_equal(id, menu_event(menu, "EID_CHANGE_LEVEL"))) {
            AbDataValue data = menu_route_field(menu->requested_launch, "data");
            if (data.kind != AB_DATA_TABLE ||
                !ab_classic_level_request_resolve(
                    ab_episode_catalog_episodes(menu->catalog), data.table,
                    &menu->level_request, error))
                return error && error->message[0]
                           ? false
                           : ab_fail(error,
                                     "Level preparation requires canonical data");
            menu->level_request_ready = true;
        }
    }
    if (menu->pending_scene.kind == AB_DATA_NIL)
        return true;
    AbDataValue target = menu->pending_scene;
    menu->pending_scene = (AbDataValue){0};
    if (menu_route_equal(target, menu_data("EPISODE_SELECTION")))
        return menu_episode_change(menu, true, error);
    AbDataValue expected = menu_route_field(&menu->episodes->completion, "target");
    return menu_route_equal(target, expected)
               ? menu_route_level(menu, error)
               : ab_fail(error,
                         "Level scene does not match the retained Loader request");
}

static bool menu_route_update(PreviewMenu *menu, AbError *error) {
    if (!menu->route_enabled)
        return true;
    menu->skip_input = (AbDataValue){.kind = AB_DATA_BOOLEAN};
    menu->loader_time = menu_route_number(menu->time);
    if (menu->loader) {
        if (!ab_classic_loader_update(menu->loader, error))
            return false;
        if (!menu_route_truthy(menu->loader_state.loading)) {
            /* Only this preview consumes the transient show/hide notices. The
             * operation has returned; release them before the next request. */
            ab_classic_loader_close(menu->loader);
            menu->loader = NULL;
        }
    }
    return menu_route_apply(menu, error);
}

#endif
