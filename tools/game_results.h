#ifndef AB_GAME_RESULTS_H
#define AB_GAME_RESULTS_H

#include "ab/fill_rectangle.h"
#include "ab/game_rectangle.h"
#include "ab/ui_text_clipping.h"
#include "ab/ui_value_text.h"

typedef struct {
    AbUiTextClipping owner;
    AbUiTextState lines[8];
    char names[8][40];
    AbFontWrappedLine wrapped[8];
    AbSpan text[8];
    uint32_t codes[256];
    uint8_t bytes[2048];
} ResultTextClip;

struct PreviewResults {
    PreviewGame *game;
    AbUiWidget root, buttons[3], stars[3], powerup, badge, best_stars[3], pig;
    AbUiWidget meter_effect, feather;
    float feather_value;
    bool eagle;
    AbUiTextWidget title, counter, best;
    AbUiFrameNode *frames[16];
    AbUiDrawChild draw[16];
    AbUiPointerChild pointer[16];
    ResultTextClip title_clip, best_clip;
    AbFontAsset fonts[2];
    AbSpan highscore_font;
    size_t fonts_loaded, button_count;
    AbDataTable button_meta;
    AbDataEntry button_menu;
    AbUiPointerValue pending_action;
    float timer, shade, center_width, reference_width, silver, gold, stars_earned;
    float shake_x, shake_y, badge_start_x, badge_start_y, badge_end_x, badge_end_y;
    uint8_t counter_bytes[64], best_bytes[256];
    bool star_achieved[3], badge_shown, failed;
};

static AbUiPointerValue result_boolean(bool value) {
    return (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN, .boolean = value};
}

static void result_position(AbUiWidget *widget, float x, float y) {
    widget->frame->fields.x = menu_number(x);
    widget->frame->fields.y = menu_number(y);
}

static bool result_image(PreviewResults *results, AbUiWidget *widget, const char *name,
                         const char *image, const char *action, AbError *error) {
    PreviewMenu *menu = results->game->menu;
    AbUiImageArguments arguments = {.frame.name = menu_text(name),
                                    .image = menu_text(image)};
    AbUiImageButtonArguments button = {.image.frame.name = menu_text(name),
                                       .return_value = action ? menu_text(action)
                                                              : (AbUiPointerValue){0},
                                       .activate_on_release = result_boolean(true)};
    if (!ab_ui_widget_init(widget, &menu->context,
                           action ? AB_UI_WIDGET_IMAGE_BUTTON : AB_UI_WIDGET_IMAGE,
                           (AbUiWidgetChildren){0}, NULL, action ? NULL : &arguments,
                           action ? &button : NULL, error))
        return false;
    if (action && !ab_ui_image_button_state_set_image(&widget->state, menu_text(image),
                                                      (AbUiPointerValue){0},
                                                      &menu->context.images, error))
        return false;
    return ab_ui_widget_add(&results->root, widget, error);
}

static bool result_text(PreviewResults *results, AbUiTextWidget *widget,
                        const char *name, AbSpan font, const char *label,
                        AbError *error) {
    PreviewMenu *menu = results->game->menu;
    AbUiFrameFields frame = {.name = menu_text(name)};
    if (!ab_ui_text_widget_init(widget, &menu->context, (AbUiWidgetChildren){0},
                                &menu->fonts, &frame, NULL, error))
        return false;
    widget->state.fields.horizontal_anchor = menu_text("HCENTER");
    widget->state.fields.vertical_anchor = menu_text("VCENTER");
    widget->state.fields.text = menu_text(label);
    return ab_ui_text_widget_set_font(
               widget, (AbUiPointerValue){.kind = AB_UI_POINTER_TEXT, .text = font},
               error) &&
           ab_ui_widget_add(&results->root, &widget->widget, error);
}

static bool result_clip(AbUiTextWidget *text, ResultTextClip *clip, AbError *error) {
    /* Application result labels expose no external line aliases. Previous
     * generations have finished drawing at this quiescent layout boundary. */
    clip->owner = (AbUiTextClipping){0};
    if (!ab_ui_text_clipping_init(
            &clip->owner,
            (AbUiTextClipStorage){clip->lines, clip->names, 8, clip->wrapped,
                                  clip->text, 8, clip->codes, 256, clip->bytes, 2048},
            error))
        return false;
    return ab_ui_text_widget_clip(text, &clip->owner, (AbUiPointerValue){0},
                                  (AbUiPointerValue){0}, error);
}

static bool result_font(PreviewResults *results, AbSpan name, AbError *error) {
    PreviewMenu *menu = results->game->menu;
    for (size_t index = 0; index < menu->fonts.count; ++index)
        if (menu->font_bindings[index].name.size == name.size &&
            !memcmp(menu->font_bindings[index].name.data, name.data, name.size))
            return true;
    if (results->fonts_loaded == 2 || menu->fonts.count == 4 || !name.size ||
        name.size > 64)
        return ab_fail(error, "Result font bindings exceed their bounds");
    for (size_t index = 0; index < name.size; ++index)
        if (!((name.data[index] >= 'A' && name.data[index] <= 'Z') ||
              (name.data[index] >= '0' && name.data[index] <= '9') ||
              name.data[index] == '_'))
            return ab_fail(error, "Unsupported selected result font identifier");
    char path[128];
    int count = snprintf(path, sizeof(path), "assets/data/fonts/%s/%.*s.dat",
                         menu->highest_resources ? "1024x768" : "480x320",
                         (int)name.size, (const char *)name.data);
    if (count < 1 || (size_t)count >= sizeof(path))
        return ab_fail(error, "Selected result font path exceeds its bound");
    AbFontAsset *font = results->fonts + results->fonts_loaded;
    uint32_t texture = 64 + (uint32_t)results->fonts_loaded;
    if (!ab_font_asset_open(menu->archive, path, 8 * 1024 * 1024, 2 * 1024 * 1024, font,
                            error))
        return false;
    if (!menu->texture_host.upload(
            menu->texture_host.context, texture,
            (AbSpriteAtlasImage){.image = &font->image,
                                 .alpha = font->has_alpha ? AB_SPRITE_ALPHA_PRESENT
                                                          : AB_SPRITE_ALPHA_OPAQUE},
            error)) {
        ab_font_asset_close(font);
        return false;
    }
    ++results->fonts_loaded;
    menu->font_bindings[menu->fonts.count++] = (AbUiFontBinding){
        name, &font->font, texture, font->image.width, font->image.height};
    return true;
}

#include "game_eagle_results.h"

static bool game_results_graph(PreviewResults *results, AbError *error) {
    PreviewMenu *menu = results->game->menu;
    AbUiFrameFields frame = {
        .name = menu_text(results->failed ? "LevelFailed" : "LevelComplete")};
    if (!ab_ui_widget_init(
            &results->root, &menu->context, AB_UI_WIDGET_FRAME,
            (AbUiWidgetChildren){results->frames, results->draw, results->pointer, 16},
            &frame, NULL, NULL, error) ||
        !result_text(results, &results->title, "pageTitle", menu_span("FONT_BASIC"),
                     results->failed ? "MT_LEVEL_FAILED" : "MT_LEVEL_COMPLETE", error))
        return false;
    const char *const images[] = {"BUTTON_MENU", "BUTTON_RESTART", "BUTTON_NEXTLEVEL"};
    const char *const actions[] = {"RETURN_TO_LEVEL_SELECTION", "RESTART_LEVEL",
                                   "NEXT_LEVEL"};
    const char *const button_names[] = {"button1", "button2", "button3"};
    for (size_t index = 0; index < 3; ++index)
        if (!result_image(results, results->buttons + index, button_names[index],
                          images[index], actions[index], error))
            return false;
    results->button_menu =
        (AbDataEntry){menu_data("menu"),
                      menu_data(results->failed ? "failed menu" : "complete menu")};
    results->button_meta = (AbDataTable){&results->button_menu, 1, 1, NULL};
    for (size_t index = 1; index < 3; ++index)
        results->buttons[index].state.meta = (AbUiPointerValue){
            .kind = AB_UI_POINTER_OBJECT, .object = (uintptr_t)&results->button_meta};
    results->button_count = 3;
    if (results->failed) {
        bool eagle;
        if (!ab_episode_progress_show_eagle(&menu->progress, &eagle, error))
            return false;
        if (eagle)
            return ab_fail(error, "Failed result requires its active Eagle controls");
        const AbClassicLevelRequest *request = &menu->level_request;
        bool disabled = menu_route_truthy(
            menu_route_field(request->episode, "disable_next_level_button"));
        const AbDataTable *overrides[] = {request->page, request->level};
        for (size_t index = 0; index < 2; ++index) {
            AbDataValue value =
                menu_route_field(overrides[index], "disable_next_level_button");
            if (menu_route_truthy(value))
                disabled = true;
            else if (value.kind == AB_DATA_BOOLEAN)
                disabled = false;
        }
        bool cleared = !results->game->first_clear;
        bool next = cleared && !disabled;
        if (cleared &&
            menu_route_truthy(menu_route_field(request->level, "clear_cutscene")))
            return ab_fail(error, "Failed result requires its cleared cutscene action");
        results->button_count = next ? 3 : 2;
        results->buttons[2].frame->fields.visible = result_boolean(next);
        const char *pig_image = "LEVEL_FAILED_PIG";
        AbUiPointerValue theme = menu->menu_settings.theme;
        if (theme.kind == AB_UI_POINTER_TEXT) {
            AbDataValue record = {0};
            const AbDataValue *found = ab_data_get(
                menu->theme_data.data.globals,
                (AbDataValue){.kind = AB_DATA_STRING, .string = theme.text});
            if (found)
                record = *found;
            AbDataValue override =
                menu_route_field(record.table, "level_failed_pig_override");
            if (menu_route_truthy(override)) {
                if (override.kind != AB_DATA_STRING)
                    return ab_fail(error, "Invalid failed-pig theme override");
                if (!result_image(results, &results->pig, "pig", pig_image, NULL,
                                  error))
                    return false;
                return ab_ui_image_state_set_image(
                    &results->pig.state.image,
                    (AbUiPointerValue){.kind = AB_UI_POINTER_TEXT,
                                       .text = override.string},
                    &menu->context.images, error);
            }
        }
        return result_image(results, &results->pig, "pig", pig_image, NULL, error);
    }
    if (results->eagle)
        return result_eagle_graph(results, error);
    const char *const star_names[] = {"star1", "star2", "star3"};
    const char *const star_images[] = {"BIG_STAR_EMPTY_1", "BIG_STAR_EMPTY_2",
                                       "BIG_STAR_EMPTY_3"};
    for (size_t index = 0; index < 3; ++index)
        if (!result_image(results, results->stars + index, star_names[index],
                          star_images[index], NULL, error))
            return false;
    if (!result_text(results, &results->counter, "scoreCounter",
                     menu_span("FONT_CHALLENGE_SCORE"), "0", error) ||
        !result_image(results, &results->powerup, "powerup_indicator",
                      "INGAME_BUTTON_POWERUPANIM", NULL, error) ||
        !result_image(results, &results->badge, "newHighscore", "NEW_HIGHSCORE_BADGE",
                      NULL, error) ||
        !result_text(results, &results->best, "bestScore", results->highscore_font, "",
                     error))
        return false;
    results->powerup.frame->fields.visible = result_boolean(false);
    results->badge.frame->fields.visible = result_boolean(false);
    results->badge.frame->fields.scale_x = results->badge.frame->fields.scale_y =
        menu_number(7);
    results->best.widget.frame->fields.visible = result_boolean(false);
    const char *const best_names[] = {"bestStar1", "bestStar2", "bestStar3"};
    float previous = menu_real(results->game->previous_score.bits);
    float thresholds[] = {0, results->silver, results->gold};
    for (size_t index = 0; index < 3; ++index) {
        if (!result_image(results, results->best_stars + index, best_names[index],
                          previous >= thresholds[index] ? "BEST_STAR_YELLOW"
                                                        : "BEST_STAR_GREY",
                          NULL, error))
            return false;
        results->best_stars[index].frame->fields.visible = result_boolean(false);
    }
    return true;
}

#include "game_results_draw.h"

static void game_results_free(PreviewGame *game) {
    PreviewResults *results = game->results;
    if (!results)
        return;
    PreviewMenu *menu = game->menu;
    AbError error = {0};
    if (results->root.initialized && !menu->context.failed)
        (void)ab_ui_widget_lifecycle(&results->root, AB_UI_FRAME_EXIT, &error);
    (void)ab_audio_bank_stop(menu->audio, menu_span("score_count"), &error);
    menu->fonts.current = NULL;
    menu->fonts.count = 2;
    for (size_t index = 0; index < results->fonts_loaded; ++index) {
        (void)menu->texture_host.remove(menu->texture_host.context,
                                        64 + (uint32_t)index, &error);
        ab_font_asset_close(results->fonts + index);
    }
    free(results);
    game->results = NULL;
}

static bool game_results_open(PreviewGame *game, AbError *error) {
    if (game->results || game->previous_score.kind != AB_DATA_NUMBER)
        return ab_fail(error, "Result construction requires an ordinary ended level");
    game_pause_free(game);
    PreviewMenu *menu = game->menu;
    PreviewResults *results = calloc(1, sizeof(*results));
    if (!results)
        return ab_fail(error, "Cannot allocate level-result controls");
    game->results = results;
    results->game = game;
    results->failed = !game->result_complete;
    results->eagle = menu_route_truthy(game->eagle_bait_launched);
    const AbDataValue *thresholds =
        ab_data_get(menu->star_limits.data.globals, game->level_name);
    if (!results->failed && (!thresholds || thresholds->kind != AB_DATA_TABLE))
        return ab_fail(error, "Missing selected result star thresholds");
    const AbDataTable *threshold_table =
        thresholds && thresholds->kind == AB_DATA_TABLE ? thresholds->table : NULL;
    AbDataValue silver = menu_route_field(threshold_table, "silverScore"),
                gold = menu_route_field(threshold_table, "goldScore");
    if (!results->failed &&
        (silver.kind != AB_DATA_NUMBER || gold.kind != AB_DATA_NUMBER))
        return ab_fail(error, "Invalid selected result star thresholds");
    results->silver = menu_real(silver.bits);
    results->gold = menu_real(gold.bits);
    float score = menu_real(game->globals.score.bits);
    results->stars_earned = score >= results->gold     ? 3.0f
                            : score >= results->silver ? 2.0f
                                                       : 1.0f;
    const char *const classes[] = {"LevelEndRoot", "LevelEnd",
                                   results->failed  ? "LevelFailed"
                                   : results->eagle ? "EagleScore"
                                                    : "LevelComplete"};
    for (size_t index = 0; index < 3; ++index) {
        const AbDataTable *groups = ab_dynamic_asset_requirements_get(
            &menu->requirements, menu_span(classes[index]));
        if (groups &&
            !ab_android_assets_manage(menu->planning, groups, true,
                                      ab_asset_resources_host, menu->resources, error))
            return false;
    }
    if (!ab_asset_textures_sync(menu->textures,
                                ab_asset_resources_owners(menu->resources), error) ||
        (!results->failed &&
         (!ab_text_resources_get(&menu->text_resources, menu_span("TEXTS_BASIC"),
                                 menu_span("FONT_HIGHSCORE"), &results->highscore_font,
                                 error) ||
          !result_font(results, menu_span("FONT_CHALLENGE_SCORE"), error) ||
          !result_font(results, results->highscore_font, error))) ||
        !game_results_graph(results, error) || !game_results_layout(game, error) ||
        !ab_ui_widget_lifecycle(&results->root, AB_UI_FRAME_ENTRY, error))
        return false;
    AbSpan flag[] = {menu_span("flagTable"), menu_span("levelEndScreenShown")};
    const AbDataTable *settings = ab_settings_statistics_root(menu->settings);
    if ((menu_route_field(settings, "flagTable").kind == AB_DATA_NIL &&
         !ab_settings_statistics_create_table(menu->settings, flag, 1, 1, error)) ||
        !ab_settings_statistics_write(menu->settings, flag, 2, game->enabled, error))
        return false;
    game->save_pending = true;
    if (results->failed)
        return true;
    uint32_t handle;
    return ab_audio_bank_play_resource(menu->audio, menu_span("score_count"), 0.7f,
                                       true, 7, &handle, error);
}

#endif
