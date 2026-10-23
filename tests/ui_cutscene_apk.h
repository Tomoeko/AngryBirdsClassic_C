#ifndef AB_UI_CUTSCENE_APK_TEST_H
#define AB_UI_CUTSCENE_APK_TEST_H

#include "ab/ui_cutscene.h"
#include "ab/hash.h"

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
                             .content_scaling = &menu->cutscene_scaling,
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

static bool cutscene_catalog_widgets_at(PreviewMenu *menu, AbMetal *renderer,
                                        AbVec2 screen, bool require_pan,
                                        AbError *error) {
    if (!menu_preview_resize(menu, screen, error))
        return false;
    for (size_t pack = 1; pack <= 9; ++pack) {
        char name[32];
        snprintf(name, sizeof(name), "pack%zu_intro", pack);
        AbDataValue selected;
        if (!menu_cutscene_selection(menu, menu_data(name), (AbDataValue){0}, &selected,
                                     error) ||
            !menu_cutscene_assets(menu, error) ||
            !ab_audio_mixer_mix(menu->mixer, 0, NULL, 0, error))
            return false;
        const AbDataTable *commands =
            level_test_child(menu->cutscene_data.data.globals, selected);
        if (!commands)
            return ab_fail(error, "Catalog intro has no actual commands");
        const AbDataValue *end =
            ab_data_get(commands, menu_route_number((float)commands->count));
        AbDataValue deadline = end && end->kind == AB_DATA_TABLE
                                   ? menu_route_field(end->table, "time")
                                   : (AbDataValue){0};
        if (deadline.kind != AB_DATA_NUMBER)
            return ab_fail(error, "Catalog intro has no actual end time");
        /* Retain the original persistent story key, not this test's name buffer. */
        for (size_t entry = 0; entry < menu->cutscene_data.data.globals->count; ++entry)
            if (menu_route_equal(menu->cutscene_data.data.globals->entries[entry].key,
                                 selected))
                selected = menu->cutscene_data.data.globals->entries[entry].key;
        size_t saves = 0;
        AbUiCutscene owner = {0};
        AbUiCutsceneClass shared = {0};
        AbUiCutsceneHost host = {.scaling = &menu->scaling,
                                 .content_scaling = &menu->cutscene_scaling,
                                 .shared = &shared,
                                 .audio = menu->audio,
                                 .mixer = menu->mixer,
                                 .settings = menu->settings,
                                 .events = menu->events,
                                 .commands = commands,
                                 .name = selected.string,
                                 .device = menu_span("android"),
                                 .white_texture = 63,
                                 .context = &saves,
                                 .save_settings = cutscene_save_boundary};
        if (!ab_ui_cutscene_init(&owner, &menu->context, host, error) ||
            !ab_ui_widget_lifecycle(&owner.widget, AB_UI_FRAME_ENTRY, error))
            return false;
        bool okay = true;
        AbVec2 previous = {0};
        float movement = 0;
        bool scroll_command = false;
        uint32_t first_pixels = 0;
        bool pixels_changed = false;
        size_t frames = (size_t)ceilf(menu_real(deadline.bits) * 30) + 2;
        for (size_t frame = 0; okay && frame < frames; ++frame) {
            okay =
                ab_ui_widget_update(&owner.widget, 1.0f / 30, (float)frame / 30, error);
            if (frame) {
                movement += fabsf(owner.scroll.x - previous.x) +
                            fabsf(owner.scroll.y - previous.y);
                scroll_command = scroll_command || owner.scrolling;
            }
            previous = owner.scroll;
            if (okay && (frame == 0 || frame == frames / 2 || frame + 1 == frames)) {
                ab_draw_list_clear(menu->context.draw_list);
                AbUiDrawInput input = {0};
                AbImage image = {0};
                const float clear[] = {0, 0, 0, 1};
                okay = ab_ui_widget_draw(&owner.widget, &input, error) &&
                       ab_metal_capture(renderer, menu->context.draw_list,
                                        (uint32_t)screen.x, (uint32_t)screen.y, clear,
                                        64 * 1024 * 1024, &image, error);
                if (okay) {
                    uint32_t pixels =
                        ab_crc32((AbSpan){image.rgba.data, image.rgba.size});
                    if (!frame)
                        first_pixels = pixels;
                    else
                        pixels_changed = pixels_changed || pixels != first_pixels;
                    const char *prefix = getenv("AB_COMIC_PAN_CAPTURE_PREFIX");
                    if (prefix && require_pan && pack == 1 && screen.x == 1920) {
                        char path[1024];
                        int length =
                            snprintf(path, sizeof(path), "%s-%zu.ppm", prefix, frame);
                        okay = length > 0 && (size_t)length < sizeof(path) &&
                               save(path, &image, error);
                    }
                    if (okay && require_pan && pack == 1) {
                        size_t last = ((size_t)image.width - 1) * 4;
                        if (memcmp(image.rgba.data, image.rgba.data + 16, 3) ||
                            memcmp(image.rgba.data + last, image.rgba.data + last - 16,
                                   3))
                            okay =
                                ab_fail(error, "Comic edge leaves a background pixel");
                        if (frame + 1 == frames)
                            for (size_t row = 0; okay && row < image.height; ++row) {
                                const uint8_t *edge =
                                    image.rgba.data + row * image.width * 4 + last;
                                if (!(edge[0] || edge[1] || edge[2]) &&
                                    (edge[-16] || edge[-15] || edge[-14]))
                                    okay = ab_fail(
                                        error,
                                        "Comic right edge is uncovered at row %zu",
                                        row);
                            }
                    }
                }
                ab_image_free(&image);
            }
        }
        if (okay && require_pan && scroll_command && (movement < 1 || !pixels_changed))
            okay = ab_fail(error,
                           "Pack%zu comic did not pan at %.0fx%.0f: scale %.4f, "
                           "reference %.0fx%.0f, class %.4f, no_scroll %u",
                           pack, screen.x, screen.y, owner.scale, owner.reference.x,
                           owner.reference.y, shared.reference_scale,
                           (unsigned)owner.no_scroll);
        AbError cleanup = {0};
        bool exited = ab_ui_widget_lifecycle(&owner.widget, AB_UI_FRAME_EXIT, &cleanup);
        if (!okay || !exited || saves != 1)
            return error && error->message[0]
                       ? false
                       : ab_fail(error, "Catalog pack%zu intro did not finish", pack);
    }
    puts("All nine source pack intros play to their actual end times and render on "
         "Metal, including vertical fit, initial scroll and direction restrictions.");
    return true;
}

static bool cutscene_catalog_widgets(PreviewMenu *menu, AbMetal *renderer,
                                     AbError *error) {
    return cutscene_catalog_widgets_at(menu, renderer, (AbVec2){1024, 768}, false,
                                       error);
}

static bool cutscene_catalog_pan_widgets(const AbZip *zip, PreviewMenu *menu,
                                         AbMetal *renderer, AbDrawList *list,
                                         AbError *error) {
    menu_preview_free(menu);
    menu->highest_resources = true;
    menu->screen = (AbVec2){1920, 1080};
    bool okay =
        menu_preview_open(zip, (AbAssetTextureHost){renderer, upload, remove_texture},
                          list, menu, error) &&
        menu_background_open(menu, error) &&
        cutscene_catalog_widgets_at(menu, renderer, menu->screen, true, error) &&
        cutscene_catalog_widgets_at(menu, renderer, (AbVec2){2560, 1440}, true,
                                    error) &&
        cutscene_catalog_widgets_at(menu, renderer, (AbVec2){3840, 2160}, true,
                                    error) &&
        cutscene_catalog_widgets_at(menu, renderer, (AbVec2){1919, 1079}, true, error);
    menu_preview_free(menu);
    return okay;
}

#endif
