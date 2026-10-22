#include "ab/game_hud.h"

#include <math.h>

static void append_font(AbGameHudPlan *plan, AbGameHudFont font) {
    plan->calls[plan->count++] = (AbGameHudCall){.kind = AB_HUD_SET_FONT, .font = font};
}

static void append_state(AbGameHudPlan *plan, AbVec2 position, float scale) {
    plan->calls[plan->count++] = (AbGameHudCall){
        .kind = AB_HUD_SET_STATE, .position = position, .scale = {scale, scale}};
}

static bool append_text(AbGameHudPlan *plan, AbGameHudText text, int32_t number,
                        AbVec2 position, AbError *error) {
    if (!isfinite(position.x) || !isfinite(position.y)) {
        return ab_fail(error, "HUD text position exceeds finite range");
    }
    plan->calls[plan->count++] = (AbGameHudCall){
        .kind = AB_HUD_DRAW_TEXT, .position = position, .text = text, .number = number};
    return true;
}

static bool score_number(float value, int32_t *output) {
    if (!isfinite(value) || value < 0 || value >= 2147483648.0f) {
        return false;
    }
    *output = (int32_t)value;
    return true;
}

static bool score_text(const AbGameHudInput *input, AbGameHudPlan *plan,
                       AbVec2 position, float leading, AbError *error) {
    float scale = input->scale * 1.3f;
    if (!isfinite(scale) || scale == 0) {
        return ab_fail(error, "Invalid visible HUD text scale");
    }
    position.x /= scale;
    position.y /= scale;
    int32_t score, highscore = 0;
    if (!score_number(input->score, &score) ||
        (input->highscore_present && !score_number(input->highscore, &highscore))) {
        return ab_fail(error, "Unsupported visible HUD numeric score");
    }
    append_state(plan, (AbVec2){0}, scale);
    if (!append_text(plan, AB_HUD_SCORE_LABEL, 0, position, error)) {
        return false;
    }
    append_font(plan, AB_HUD_FONT_BASIC);
    if (!append_text(plan, AB_HUD_SCORE_VALUE, score,
                     (AbVec2){position.x, leading + position.y}, error)) {
        return false;
    }
    if (input->highscore_present) {
        float small_scale = 0.7f * scale;
        float y = leading + leading;
        y += position.y;
        y /= 0.7f;
        float x = position.x / 0.7f;
        if (!isfinite(small_scale) || small_scale == 0) {
            return ab_fail(error, "Invalid HUD highscore scale");
        }
        append_state(plan, (AbVec2){0}, small_scale);
        if (!append_text(plan, AB_HUD_HIGHSCORE_VALUE, highscore,
                         (AbVec2){x, leading + y}, error)) {
            return false;
        }
        append_font(plan, AB_HUD_FONT_BASIC);
        if (!append_text(plan, AB_HUD_HIGHSCORE_LABEL, 0, (AbVec2){x, y}, error)) {
            return false;
        }
    }
    append_state(plan, (AbVec2){0}, 1);
    return true;
}

bool ab_game_hud_plan(const AbGameHudInput *input, AbGameHudPlan *output,
                      AbError *error) {
    if (!input || !output || !isfinite(input->scale) ||
        !isfinite(input->measured_score_width) ||
        (input->previous_width_present && !isfinite(input->previous_score_width))) {
        return ab_fail(error, "Invalid HUD scale or measured width");
    }
    AbGameHudPlan plan = {0};
    plan.next_score_width = input->measured_score_width * input->scale;
    if (!isfinite(plan.next_score_width)) {
        return ab_fail(error, "HUD measured width exceeds finite range");
    }
    if (input->previous_width_present &&
        input->previous_score_width > plan.next_score_width) {
        plan.next_score_width = input->previous_score_width;
    }
    AbVec2 text_origin = {0};
    float text_scale = 1;
    if (input->retina) {
        text_origin.x = -input->screen_width / 2.0f;
        text_scale = 2;
        if (!isfinite(text_origin.x)) {
            return ab_fail(error, "Invalid retina HUD width");
        }
    }
    append_font(&plan, AB_HUD_FONT_BASIC);
    if (!input->update_mode || input->tutorial_active) {
        plan.calls[plan.count++] = (AbGameHudCall){.kind = AB_HUD_DRAW_WORLD};
    }
    append_state(&plan, text_origin, text_scale);
    if (input->show_score) {
        AbVec2 position = {input->screen_width - 3.0f, 0};
        position.x -= input->margin.x;
        if (input->margin.y > position.y) {
            position.y = input->margin.y;
        }
        float leading = input->font_leading + 1.0f;
        if (!isfinite(input->screen_width) || !isfinite(input->margin.x) ||
            !isfinite(input->margin.y) || !isfinite(leading) || !isfinite(position.x)) {
            return ab_fail(error, "Invalid HUD margins or font leading");
        }
        append_font(&plan, AB_HUD_FONT_BASIC);
        if (!input->hide_scores &&
            !score_text(input, &plan, position, leading, error)) {
            return false;
        }
        append_font(&plan, AB_HUD_FONT_SCORE);
    }
    append_state(&plan, text_origin, text_scale);
    append_font(&plan, AB_HUD_FONT_BASIC);
    plan.calls[plan.count++] = (AbGameHudCall){.kind = AB_HUD_DRAW_CHILDREN};
    *output = plan;
    return true;
}
