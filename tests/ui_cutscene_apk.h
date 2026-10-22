#ifndef AB_UI_CUTSCENE_APK_TEST_H
#define AB_UI_CUTSCENE_APK_TEST_H

#include "ab/ui_cutscene.h"

static bool cutscene_save_boundary(void *context, AbError *error) {
    (void)error;
    ++*(size_t *)context;
    return true;
}

static bool cutscene_release(PreviewMenu *menu, AbUiCutscene *owner, AbError *error) {
    AbVec2 point;
    if (!ab_ui_widget_screen_position(&owner->skip, &point, error))
        return false;
    menu->context.cursor = point;
    menu->context.left_button_held = true;
    menu->context.touch_count = 1;
    AbUiPointerReply reply;
    if (!ab_ui_widget_pointer(&owner->widget, menu_span("LPRESS"), point, &reply,
                              error))
        return false;
    menu->context.left_button_held = false;
    menu->context.touch_count = 0;
    return ab_ui_widget_pointer(&owner->widget, menu_span("LRELEASE"), point, &reply,
                                error);
}

static bool cutscene_widgets(PreviewMenu *menu, AbMetal *renderer, const char *path,
                             AbError *error) {
    const char *stage = "prepared CutScene assets";
    const AbDataTable *requirements =
        ab_dynamic_asset_requirements_get(&menu->requirements, menu_span("CutScene"));
    AbTargetData definition = {0};
    AbUiCutscene owner = {0};
    AbUiCutsceneClass shared = {0};
    AbImage captured = {0};
    LevelSelectionEvents completion = {0};
    AbEventListener listener = {.context = &completion,
                                .event_triggered = selection_record};
    AbDataEntry finish_field = {menu_data("id"), menu_event(menu, "EID_CHANGE_LEVEL")};
    AbDataTable finish = {&finish_field, 1, 1, NULL};
    size_t saves = 0;
    bool okay = false, entered = false, subscribed = false;
    bool route_enabled = menu->route_enabled;
    menu->route_enabled = false;
    if (!requirements ||
        !ab_android_assets_manage(menu->planning, requirements, true,
                                  ab_asset_resources_host, menu->resources, error) ||
        !ab_asset_textures_sync(menu->textures,
                                ab_asset_resources_owners(menu->resources), error) ||
        !ab_target_data_open(menu->archive, "assets/data/scripts/cutscenes.lua",
                             (AbSpan){menu->key, sizeof(menu->key)}, 4 * 1024 * 1024,
                             NULL, &definition, error))
        goto release;
    const AbDataTable *commands =
        level_test_child(definition.data.globals, menu_data("pack1_intro"));
    if (!commands || commands->count != 6)
        goto release;
    stage = "source intro construction and entry";
    AbUiCutsceneHost host = {.scaling = &menu->scaling,
                             .shared = &shared,
                             .audio = menu->audio,
                             .mixer = menu->mixer,
                             .settings = menu->settings,
                             .events = menu->events,
                             .end_event = &finish,
                             .commands = commands,
                             .name = menu_span("pack1_intro"),
                             .device = menu_span("android"),
                             .white_texture = 63,
                             .context = &saves,
                             .save_settings = cutscene_save_boundary};
    if (!ab_event_manager_add(menu->events, menu_event(menu, "EID_CHANGE_LEVEL"),
                              &listener, error))
        goto release;
    subscribed = true;
    if (!ab_ui_cutscene_init(&owner, &menu->context, host, error) ||
        !ab_ui_widget_lifecycle(&owner.widget, AB_UI_FRAME_ENTRY, error))
        goto release;
    entered = true;
    if (!ab_ui_widget_lifecycle(&owner.widget, AB_UI_FRAME_LAYOUT, error) ||
        !ab_ui_widget_update(&owner.widget, 0, 0, error) || owner.command_index != 5 ||
        !owner.sprite_present || owner.sound_count != 1)
        goto release;
    stage = "real intro Metal draw";
    ab_draw_list_clear(menu->context.draw_list);
    AbUiDrawInput input = {0};
    const float clear[] = {0, 0, 0, 1};
    if (!ab_ui_widget_draw(&owner.widget, &input, error) ||
        !ab_metal_capture(renderer, menu->context.draw_list, 1024, 768, clear,
                          16 * 1024 * 1024, &captured, error) ||
        (path && !save(path, &captured, error)))
        goto release;
    stage = "actual delayed scroll and midpoint";
    if (!ab_ui_widget_update(&owner.widget, 3.5f, 3.5f, error) ||
        owner.command_index != 6 || !owner.scrolling || owner.scroll.x != 0 ||
        !ab_ui_widget_update(&owner.widget, 4.8f, 8.3f, error) ||
        fabsf(owner.scroll.x - owner.scroll_end.x * 0.5f) > 0.001f)
        goto release;
    stage = "completion marks watched, saves then notifies";
    if (!ab_ui_widget_update(&owner.widget, 8, 16.3f, error) ||
        owner.command_index != 7 || completion.count != 1 ||
        completion.last != &finish || saves != 1 || owner.scrolling ||
        owner.scroll.x != owner.scroll_end.x)
        goto release;
    const AbDataTable *watched = level_test_child(
        ab_settings_statistics_root(menu->settings), menu_data("cutscenesWatched"));
    const AbDataValue *done =
        watched ? ab_data_get(watched, menu_data("pack1_intro")) : NULL;
    if (!done || done->kind != AB_DATA_BOOLEAN || !done->bits)
        goto release;
    stage = "held-entry release and skip without source save";
    if (!ab_ui_widget_lifecycle(&owner.widget, AB_UI_FRAME_EXIT, error))
        goto release;
    entered = false;
    menu->context.left_button_held = true;
    if (!ab_ui_widget_lifecycle(&owner.widget, AB_UI_FRAME_ENTRY, error))
        goto release;
    entered = true;
    if (!ab_ui_widget_lifecycle(&owner.widget, AB_UI_FRAME_LAYOUT, error) ||
        !ab_ui_widget_update(&owner.widget, 0, 0, error) ||
        !cutscene_release(menu, &owner, error) || owner.ldown ||
        completion.count != 1 || !cutscene_release(menu, &owner, error) ||
        completion.count != 2 || saves != 1)
        goto release;
    puts("Actual pack1 intro: six APK command records, prepared story artwork, "
         "source fit/scroll, muted audio cue, watched state, save boundary and skip.");
    okay = true;
release:
    menu->route_enabled = route_enabled;
    if (!okay && !error->message[0])
        ab_fail(error, "CutScene intro integration differs at %s", stage);
    AbError cleanup = {0};
    if (entered && !menu->context.failed)
        (void)ab_ui_widget_lifecycle(&owner.widget, AB_UI_FRAME_EXIT, &cleanup);
    if (subscribed)
        (void)ab_event_manager_remove(
            menu->events, menu_event(menu, "EID_CHANGE_LEVEL"), &listener, &cleanup);
    ab_image_free(&captured);
    ab_target_data_close(&definition);
    return okay;
}

#endif
