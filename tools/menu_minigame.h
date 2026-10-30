#ifndef AB_MENU_MINIGAME_H
#define AB_MENU_MINIGAME_H

#include "ab/settings_golden_eggs.h"
#include "ab/hash.h"

#include <stdio.h>

typedef struct SoundboardScene SoundboardScene;
typedef struct KeyboardScene KeyboardScene;
typedef struct SequencerScene SequencerScene;
typedef struct RadioScene RadioScene;
typedef struct AccordionScene AccordionScene;

struct PreviewMinigame {
    PreviewMenu *menu;
    AbDataValue name;
    const char *location;
    const char *groups[3];
    size_t group_count;
    AbUiWidget widget, background, back, audio;
    AbUiFrameNode *frames[35];
    AbUiDrawChild draw[35];
    AbUiPointerChild pointer[35];
    AbDataTable loading, completion, items, golden, popup, gained, star_data;
    AbDataEntry loading_fields[3], completion_fields[2], item, golden_fields[2];
    AbDataEntry popup_fields[2], gained_fields[2], star_fields[2];
    AbClassicLoaderCall load, *call_items[2];
    AbClassicLoaderCalls calls;
    bool (*construct)(PreviewMinigame *owner, AbError *error);
    void (*dispose)(PreviewMinigame *owner);
    bool star_collected, active, entered;
};

static bool soundboard_new(PreviewMenu *menu, AbError *error);
static bool keyboard_new(PreviewMenu *menu, AbError *error);
static bool sequencer_new(PreviewMenu *menu, AbError *error);
static bool radio_new(PreviewMenu *menu, AbError *error);
static bool accordion_new(PreviewMenu *menu, AbError *error);

static bool minigame_script(PreviewMinigame *owner, const char *path,
                            const char *expected, AbTargetData *data, AbError *error) {
    PreviewMenu *menu = owner->menu;
    uint8_t digest[32];
    char hash[65];
    if (!ab_target_packed_read(menu->archive, path, (AbSpan){menu->key, 32},
                               1024 * 1024, &data->bytes, error) ||
        !ab_sha256((AbSpan){data->bytes.data, data->bytes.size}, digest, error))
        return false;
    ab_digest_hex(digest, hash);
    if (strcmp(hash, expected))
        return ab_fail(error, "Unexpected pinned Golden Egg script: %s", path);
    return ab_lua_read((AbSpan){data->bytes.data, data->bytes.size}, 1024 * 1024,
                       &data->chunk, error);
}

static bool minigame_image(PreviewMinigame *owner, AbUiWidget *widget, const char *name,
                           const char *image, AbError *error) {
    AbUiImageArguments arguments = {.frame.name = menu_text(name),
                                    .image = menu_text(image)};
    return ab_ui_widget_init(widget, &owner->menu->context, AB_UI_WIDGET_IMAGE,
                             (AbUiWidgetChildren){0}, NULL, &arguments, NULL, error) &&
           ab_ui_widget_add(&owner->widget, widget, error);
}

static bool minigame_set_image(PreviewMinigame *owner, AbUiWidget *widget,
                               const char *image, AbError *error) {
    return ab_ui_image_state_set_image(&widget->state.image, menu_text(image),
                                       &owner->menu->context.images, error);
}

static AbDataValue minigame_at(const AbDataTable *table, size_t index) {
    const AbDataValue *value = ab_data_get(table, menu_route_number((float)index));
    return value ? *value : (AbDataValue){0};
}

static bool minigame_number(const AbDataTable *table, const char *key, float *output,
                            AbError *error) {
    AbDataValue value = menu_route_field(table, key);
    if (value.kind != AB_DATA_NUMBER || !isfinite(menu_real(value.bits)))
        return ab_fail(error, "Invalid Golden Egg numeric field: %s", key);
    *output = menu_real(value.bits);
    return true;
}

static bool menu_minigame_name(AbDataValue name) {
    return menu_route_equal(name, menu_data("SOUNDBOARD1")) ||
           menu_route_equal(name, menu_data("KEYBOARD")) ||
           menu_route_equal(name, menu_data("SEQUENCER")) ||
           menu_route_equal(name, menu_data("RADIO")) ||
           menu_route_equal(name, menu_data("ACCORDION"));
}

static bool minigame_manage_assets(PreviewMinigame *owner, bool load, AbError *error) {
    if (!owner->group_count || owner->group_count > 3)
        return ab_fail(error, "Invalid Golden Egg resource groups");
    AbDataEntry entries[3];
    for (size_t index = 0; index < owner->group_count; ++index)
        entries[index] = (AbDataEntry){menu_route_number((float)(index + 1)),
                                       menu_data(owner->groups[index])};
    AbDataTable groups = {entries, owner->group_count, owner->group_count, NULL};
    PreviewMenu *menu = owner->menu;
    return ab_android_assets_manage(menu->planning, &groups, load,
                                    ab_asset_resources_host, menu->resources, error) &&
           ab_asset_textures_sync(menu->textures,
                                  ab_asset_resources_owners(menu->resources), error);
}

static bool minigame_assets(void *context, AbError *error) {
    return minigame_manage_assets(context, true, error);
}

static bool minigame_collect(PreviewMinigame *owner, AbError *error) {
    PreviewMenu *menu = owner->menu;
    owner->star_collected = true;
    AbSpan name = owner->name.string;
    const AbDataTable *scores = ab_settings_statistics_root(menu->highscores);
    const AbDataValue *found = ab_data_get(scores, owner->name);
    AbDataValue prior = found ? *found : (AbDataValue){0};
    if (menu_route_truthy(prior) && prior.kind != AB_DATA_TABLE)
        return ab_fail(error, "Golden Egg highscore is not a table");
    bool first = !menu_route_truthy(prior) ||
                 !menu_route_truthy(menu_route_field(prior.table, "completed"));
    owner->popup_fields[0] =
        (AbDataEntry){menu_data("id"), menu_event(menu, "EID_STAR_POPUP")};
    owner->popup_fields[1] = (AbDataEntry){menu_data("first_time"),
                                           {.kind = AB_DATA_BOOLEAN, .bits = first}};
    owner->popup = (AbDataTable){owner->popup_fields, 2, 2, NULL};
    if (!ab_event_manager_notify(menu->events, &owner->popup, error))
        return false;
    AbSpan path[] = {name, menu_span("completed")};
    if (prior.kind == AB_DATA_NIL) {
        if (!ab_settings_statistics_create_table(menu->highscores, path, 1, 4, error))
            return false;
        const char *const fields[] = {"completed", "birds", "score", "lowScore"};
        for (size_t index = 0; index < 4; ++index) {
            path[1] = menu_span(fields[index]);
            AbDataValue value = index
                                    ? menu_route_number(0)
                                    : (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1};
            if (!ab_settings_statistics_write(menu->highscores, path, 2, value, error))
                return false;
        }
    } else if (prior.kind != AB_DATA_TABLE ||
               !ab_settings_statistics_write(
                   menu->highscores, path, 2,
                   (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1}, error)) {
        return ab_fail(error, "Invalid existing Golden Egg highscore row");
    }
    if (!menu_preview_flush(menu, error))
        return false;
    float stars, available;
    if (!ab_episode_statistics_stars(&menu->statistics, menu_data("G"), false, &stars,
                                     &available, error))
        return false;
    owner->star_fields[0] =
        (AbDataEntry){menu_data("starsGained"), menu_route_number(stars)};
    owner->star_fields[1] = (AbDataEntry){menu_data("levelName"), owner->name};
    owner->star_data = (AbDataTable){owner->star_fields, 2, 2, NULL};
    owner->gained_fields[0] =
        (AbDataEntry){menu_data("id"), menu_event(menu, "EID_GOLDEN_EGG_STAR_GAINED")};
    owner->gained_fields[1] = (AbDataEntry){
        menu_data("data"), {.kind = AB_DATA_TABLE, .table = &owner->star_data}};
    owner->gained = (AbDataTable){owner->gained_fields, 2, 2, NULL};
    return ab_event_manager_notify(menu->events, &owner->gained, error);
}

static bool minigame_layout(PreviewMinigame *owner, AbError *error) {
    PreviewMenu *menu = owner->menu;
    AbVec2 bounds, margin = {0};
    if (!menu->context.images.bounds(menu->context.images.context,
                                     menu_span("GOLDEN_EGG_BG_1"), &bounds, error) ||
        bounds.x <= 0 || bounds.y <= 0)
        return ab_fail(error, "Missing actual soundboard background bounds");
    AbUiFrameFields *background = &owner->background.frame->fields;
    background->x = menu_number(menu->screen.x * .5f);
    background->y = menu_number(menu->screen.y * .5f);
    background->scale_x = menu_number(menu->screen.x / bounds.x);
    background->scale_y = menu_number(menu->screen.y / bounds.y);
    AbDataValue x = menu_route_field(&menu->margin, "x");
    AbDataValue y = menu_route_field(&menu->margin, "y");
    if (x.kind != AB_DATA_NUMBER || y.kind != AB_DATA_NUMBER)
        return ab_fail(error, "Soundboard requires actual layout margins");
    margin = (AbVec2){menu_real(x.bits), menu_real(y.bits)};
    if (!ab_ui_scaling_sprite(&menu->scaling, &menu->context.images,
                              menu_span("ARROW_LEFT"), false, &bounds, error))
        return false;
    owner->back.frame->fields.x = menu_number(bounds.x * .5f + margin.x);
    owner->back.frame->fields.y =
        menu_number(menu->screen.y - bounds.y * .5f - margin.y);
    if (!ab_ui_scaling_set_to(&menu->scaling, &owner->back.frame->fields, error) ||
        !ab_ui_scaling_sprite(&menu->scaling, &menu->context.images,
                              menu_span("MENU_SFX"), false, &bounds, error))
        return false;
    owner->audio.frame->fields.x = menu_number(bounds.x * .5f + margin.x);
    owner->audio.frame->fields.y = menu_number(bounds.y * .5f + margin.y);
    if (!ab_ui_scaling_set_to(&menu->scaling, &owner->audio.frame->fields, error))
        return false;
    return true;
}

static bool minigame_lifecycle(PreviewMinigame *owner, AbUiWidget *widget,
                               AbUiFrameMethod method, AbError *error) {
    if (!ab_ui_widget_lifecycle_base(widget, method, error))
        return false;
    if (method == AB_UI_FRAME_ENTRY) {
        owner->entered = true;
        owner->star_collected = false;
        AbDataValue song = owner->menu->main_song;
        return song.kind == AB_DATA_NIL ||
               (song.kind == AB_DATA_STRING &&
                ab_audio_bank_stop(owner->menu->audio, song.string, error));
    }
    if (method != AB_UI_FRAME_EXIT)
        return true;
    owner->entered = false;
    return minigame_manage_assets(owner, false, error);
}

static bool minigame_controls(PreviewMinigame *owner, AbUiPointerReply *reply,
                              bool *handled, AbError *error) {
    *handled = true;
    if (menu_equal(reply->values[0], "EXIT_SOUNDBOARD"))
        return menu_minigame_leave(owner->menu, error);
    if (!menu_equal(reply->values[0], "TOGGLE_SOUND")) {
        *handled = false;
        return true;
    }
    bool enabled;
    return menu_audio_change(owner->menu, owner->location, error) &&
           menu_audio_enabled(owner->menu, &enabled, error) &&
           ab_ui_image_button_state_set_image(
               &owner->audio.state,
               menu_text(enabled ? "MENU_SFX" : "BUTTON_MENU_SFX_OFF"),
               (AbUiPointerValue){0}, &owner->menu->context.images, error);
}

static bool minigame_widgets(PreviewMinigame *owner, const char *title,
                             AbError *error) {
    AbUiWidgetContext *context = &owner->menu->context;
    AbUiFrameFields frame = {.name = menu_text(title)};
    if (!ab_ui_widget_init(
            &owner->widget, context, AB_UI_WIDGET_FRAME,
            (AbUiWidgetChildren){owner->frames, owner->draw, owner->pointer, 35},
            &frame, NULL, NULL, error) ||
        !ab_ui_widget_init(&owner->background, context, AB_UI_WIDGET_IMAGE,
                           (AbUiWidgetChildren){0}, NULL,
                           &(AbUiImageArguments){.frame.name = menu_text("background"),
                                                 .image = menu_text("GOLDEN_EGG_BG_1")},
                           NULL, error) ||
        !ab_ui_widget_add(&owner->widget, &owner->background, error))
        return false;
    return true;
}

static bool minigame_widget_controls(PreviewMinigame *owner, AbError *error) {
    AbUiWidgetContext *context = &owner->menu->context;
    AbUiImageButtonArguments back = {
        .image = {.frame.name = menu_text("back_button"),
                  .image = menu_text("ARROW_LEFT")},
        .return_value = menu_text("EXIT_SOUNDBOARD"),
        .activate_on_release = {.kind = AB_UI_POINTER_BOOLEAN, .boolean = true}};
    bool enabled;
    if (!menu_audio_enabled(owner->menu, &enabled, error))
        return false;
    AbUiImageButtonArguments audio = {
        .image = {.frame.name = menu_text("audio_button"),
                  .image = menu_text(enabled ? "MENU_SFX" : "BUTTON_MENU_SFX_OFF")},
        .return_value = menu_text("TOGGLE_SOUND"),
        .activate_on_release = {.kind = AB_UI_POINTER_BOOLEAN, .boolean = true}};
    if (!ab_ui_widget_init(&owner->back, context, AB_UI_WIDGET_IMAGE_BUTTON,
                           (AbUiWidgetChildren){0}, NULL, NULL, &back, error) ||
        !ab_ui_widget_add(&owner->widget, &owner->back, error) ||
        !ab_ui_widget_init(&owner->audio, context, AB_UI_WIDGET_IMAGE_BUTTON,
                           (AbUiWidgetChildren){0}, NULL, NULL, &audio, error) ||
        !ab_ui_widget_add(&owner->widget, &owner->audio, error))
        return false;
    return true;
}

static AbUiWidget *menu_minigame_root(PreviewMenu *menu) {
    return menu->minigame && menu->minigame->active ? &menu->minigame->widget : NULL;
}

static bool menu_minigame_is_loading(PreviewMenu *menu, AbDataTable *event) {
    return menu->minigame && event == &menu->minigame->loading;
}

static bool menu_minigame_start_loader(PreviewMenu *menu, AbError *error) {
    PreviewMinigame *owner = menu->minigame;
    return owner && menu_route_start_loader(menu, &owner->calls,
                                            (AbDataValue){.kind = AB_DATA_TABLE,
                                                          .table = &owner->completion},
                                            error);
}

static bool menu_minigame_is_target(PreviewMenu *menu, AbDataValue target) {
    return menu->minigame && target.kind == AB_DATA_OBJECT &&
           target.object == &menu->minigame->widget;
}

static bool menu_minigame_enter(PreviewMenu *menu, AbError *error) {
    PreviewMinigame *owner = menu->minigame;
    if (!owner || owner->active ||
        !ab_ui_widget_lifecycle(menu_active_root(menu), AB_UI_FRAME_EXIT, error))
        return ab_fail(error, "Invalid Golden Egg scene transition");
    owner->active = true;
    menu->level_active = menu->episode_active = menu->cutscene_active = false;
    return ab_ui_widget_lifecycle(&owner->widget, AB_UI_FRAME_ENTRY, error) &&
           ab_ui_widget_lifecycle(&owner->widget, AB_UI_FRAME_LAYOUT, error);
}

static bool menu_minigame_open(PreviewMenu *menu, AbError *error) {
    if (menu->minigame || menu->game || !menu_minigame_name(menu->level_request.name))
        return ab_fail(error, "Invalid Golden Egg preparation request");
    if ((!menu->episodes && !menu_episode_open(menu, error)) ||
        !menu_highscores_open(menu, error))
        return false;
    bool soundboard =
        menu_route_equal(menu->level_request.name, menu_data("SOUNDBOARD1"));
    bool keyboard = menu_route_equal(menu->level_request.name, menu_data("KEYBOARD"));
    bool sequencer = menu_route_equal(menu->level_request.name, menu_data("SEQUENCER"));
    bool radio = menu_route_equal(menu->level_request.name, menu_data("RADIO"));
    if (!(soundboard  ? soundboard_new(menu, error)
          : keyboard  ? keyboard_new(menu, error)
          : sequencer ? sequencer_new(menu, error)
          : radio     ? radio_new(menu, error)
                      : accordion_new(menu, error)))
        return false;
    PreviewMinigame *owner = menu->minigame;
    menu->episodes->selected_episode = menu->level_request.episode_id;
    menu->level_request_ready = false;
    AbDataValue row;
    AbSpan path[] = {menu_span("openGoldenEggLevels"), owner->name.string,
                     menu_span("opened")};
    if (!ab_settings_golden_egg(menu->highscores, path[1], &row, error) ||
        !ab_settings_statistics_write(menu->highscores, path, 3,
                                      (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1},
                                      error))
        return false;
    owner->golden_fields[0] =
        (AbDataEntry){menu_data("id"), menu_event(menu, "EID_GE_LEVEL_STARTED")};
    owner->golden_fields[1] = (AbDataEntry){menu_data("levelName"), owner->name};
    owner->golden = (AbDataTable){owner->golden_fields, 2, 2, NULL};
    if (!ab_event_manager_notify(menu->events, &owner->golden, error) ||
        !owner->construct(owner, error))
        return false;
    owner->load = (AbClassicLoaderCall){owner, minigame_assets};
    owner->call_items[0] = &owner->load;
    owner->calls = (AbClassicLoaderCalls){owner->call_items, 2};
    owner->item = (AbDataEntry){menu_route_number(1),
                                {.kind = AB_DATA_OBJECT, .object = &owner->load}};
    owner->items = (AbDataTable){&owner->item, 1, 1, NULL};
    owner->completion_fields[0] =
        (AbDataEntry){menu_data("id"), menu_event(menu, "EID_CHANGE_SCENE")};
    owner->completion_fields[1] = (AbDataEntry){
        menu_data("target"), {.kind = AB_DATA_OBJECT, .object = &owner->widget}};
    owner->completion = (AbDataTable){owner->completion_fields, 2, 2, NULL};
    owner->loading_fields[0] =
        (AbDataEntry){menu_data("id"), menu_event(menu, "EID_DO_LOADING")};
    owner->loading_fields[1] = (AbDataEntry){
        menu_data("items"), {.kind = AB_DATA_TABLE, .table = &owner->items}};
    owner->loading_fields[2] =
        (AbDataEntry){menu_data("completion_event"),
                      {.kind = AB_DATA_TABLE, .table = &owner->completion}};
    owner->loading = (AbDataTable){owner->loading_fields, 3, 3, NULL};
    return ab_event_manager_notify(menu->events, &owner->loading, error);
}

static bool menu_minigame_close(PreviewMenu *menu, AbError *error) {
    PreviewMinigame *owner = menu->minigame;
    if (!owner)
        return true;
    bool okay = !owner->entered || menu->context.failed ||
                ab_ui_widget_lifecycle(&owner->widget, AB_UI_FRAME_EXIT, error);
    owner->dispose(owner);
    menu->minigame = NULL;
    return okay;
}

static bool menu_minigame_leave(PreviewMenu *menu, AbError *error) {
    if (menu->reward || menu_route_truthy(menu->skip_input))
        return true;
    if (!menu->minigame)
        return ab_fail(error, "No active Golden Egg to leave");
    if (!menu->minigame->active)
        return true;
    AbDataEntry fields[] = {{menu_data("id"), menu_event(menu, "EID_CHANGE_SCENE")},
                            {menu_data("target"), menu_data("LEVEL_SELECTION_G")}};
    AbDataTable event = {fields, 2, 2, NULL};
    return ab_event_manager_notify(menu->events, &event, error);
}

#include "menu_soundboard.h"
#include "menu_keyboard.h"
#include "menu_sequencer.h"
#include "menu_radio.h"
#include "menu_accordion.h"

#endif
