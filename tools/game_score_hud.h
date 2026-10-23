#ifndef AB_GAME_SCORE_HUD_H
#define AB_GAME_SCORE_HUD_H

#include "ab/game_hud.h"
#include "ab/utf8.h"

/* Execute the existing native-validated ordinary GameHud score plan. World
 * drawing already ran at the controller boundary; child HUD widgets follow
 * this score/floating pass in game_session_draw. */
static bool game_score_hud_draw(PreviewGame *game, AbError *error) {
    PreviewMenu *menu = game->menu;
    /* This direct GameHud body has no clipping parent. The previous menu's
     * Frame clip may retain its old viewport through a desktop resize. */
    menu->render.clip = (AbDrawClip){0};
    const AbUiFontBinding *basic = NULL;
    for (size_t index = 0; index < menu->fonts.count; ++index)
        if (menu_route_equal((AbDataValue){.kind = AB_DATA_STRING,
                                           .string = menu->fonts.bindings[index].name},
                             game_text("FONT_BASIC")))
            basic = menu->fonts.bindings + index;
    if (!basic || game->globals.score.kind != AB_DATA_NUMBER)
        return ab_fail(error, "Missing gameplay score font or numeric total");
    float score = menu_real(game->globals.score.bits);
    if (!isfinite(score) || score < 0 || score >= 2147483648.0f)
        return ab_fail(error, "Gameplay score exceeds native integer text domain");
    char number[32];
    int written = snprintf(number, sizeof(number), "%d", (int)score);
    size_t length;
    AbFontMeasure measure;
    if (written < 1 || (size_t)written >= sizeof(number) ||
        !ab_utf8_to_codes(menu_span(number), menu->codes, 128, &length) ||
        !ab_font_measure_codes(basic->font, menu->codes, length, 0, length, &measure))
        return ab_fail(error, "Cannot measure current gameplay score");
    float scale;
    if (!ab_ui_scaling_get(&menu->scaling, &scale, error))
        return false;
    AbGameHudPlan plan;
    if (!ab_game_hud_plan(
            &(AbGameHudInput){.screen_width = game->screen.x,
                              .margin = {menu_real(menu->margin_fields[0].value.bits),
                                         menu_real(menu->margin_fields[1].value.bits)},
                              .scale = scale,
                              .score = score,
                              .highscore = game->hud_highscore,
                              .font_leading = (float)basic->font->leading,
                              .measured_score_width = (float)measure.width,
                              .previous_score_width = game->hud_score_width,
                              .previous_width_present = game->hud_score_width_present,
                              .highscore_present = game->hud_highscore > 0,
                              .show_score = true,
                              .update_mode = true},
            &plan, error))
        return false;
    AbVec2 draw_scale = {1, 1};
    for (size_t index = 0; index < plan.count; ++index) {
        const AbGameHudCall *call = plan.calls + index;
        if (call->kind == AB_HUD_SET_STATE) {
            draw_scale = call->scale;
            menu->render.geometry =
                (AbSpriteRenderState){.translation_x = call->position.x,
                                      .translation_y = call->position.y,
                                      .scale_x = draw_scale.x,
                                      .scale_y = draw_scale.y};
        } else if (call->kind == AB_HUD_DRAW_TEXT) {
            AbSpan label;
            if (call->text == AB_HUD_SCORE_LABEL)
                label = menu_span("MI_SCORE");
            else if (call->text == AB_HUD_HIGHSCORE_LABEL)
                label = menu_span("MI_HIGH_SCORE");
            else {
                written = snprintf(number, sizeof(number), "%d", call->number);
                if (written < 1 || (size_t)written >= sizeof(number))
                    return ab_fail(error, "Cannot format gameplay HUD score");
                label = menu_span(number);
            }
            AbUiTextDrawPlan text = {
                .font = menu_span("FONT_BASIC"),
                .group = menu_span("TEXTS_BASIC"),
                .text = label,
                .layout = {.placement = {.x = call->position.x,
                                         .y = call->position.y,
                                         .scale_x = 1,
                                         .scale_y = 1,
                                         .vertical = AB_ANCHOR_TOP,
                                         .horizontal = AB_ANCHOR_RIGHT}},
                .rotation = {.cosine = 1},
                .scale = draw_scale,
                .draw_alpha = 1,
                .after_alpha = 1,
                .draw = true};
            if (!ab_ui_font_resources_draw(&menu->fonts, &menu->context, &text, error))
                return false;
        }
    }
    game->hud_score_width = plan.next_score_width;
    game->hud_score_width_present = true;
    return game_floating_scores_draw(game, error);
}

#endif
