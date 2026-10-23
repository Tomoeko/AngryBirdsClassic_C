#ifndef AB_GAME_RESULTS_LAYOUT_H
#define AB_GAME_RESULTS_LAYOUT_H

static bool result_number_text(float value, uint8_t *bytes, size_t capacity,
                               AbUiPointerValue *output, AbError *error) {
    AbSpan text;
    if (!ab_ui_value_text(menu_number(value), bytes, capacity, &text))
        return ab_fail(error, "Result number exceeds its text storage");
    *output = (AbUiPointerValue){.kind = AB_UI_POINTER_TEXT, .text = text};
    return true;
}

static bool result_measure(PreviewResults *results, AbUiPointerValue font,
                           AbUiPointerValue text, float *width, AbError *error) {
    AbUiTextMetricBoundaries metrics =
        ab_ui_font_resources_boundaries(&results->game->menu->fonts);
    AbUiPointerValue measured;
    if (!metrics.resources.set_font(metrics.resources.context, font, error) ||
        !metrics.resources.measure(metrics.resources.context, text, &measured, error) ||
        measured.kind != AB_UI_POINTER_NUMBER)
        return false;
    *width = menu_real(measured.number_bits);
    return isfinite(*width) || ab_fail(error, "Nonfinite result text width");
}

static bool game_results_layout(PreviewGame *game, AbError *error) {
    PreviewResults *results = game->results;
    PreviewMenu *menu = game->menu;
    AbVec2 star;
    float scale;
    if (!ab_ui_scaling_sprite(&menu->scaling, &menu->context.images,
                              menu_span("BIG_STAR_2"), false, &star, error) ||
        !ab_ui_scaling_get(&menu->scaling, &scale, error))
        return false;
    float width = game->screen.x, height = game->screen.y;
    results->center_width = fmaxf(fminf(409.6f, width * 0.4f), 1.78f * star.x);
    results->reference_width = fmaxf(fminf(1024, width), 4.45f * star.x);
    if (results->eagle)
        return result_eagle_layout(results, error);
    if (results->failed) {
        result_position(&results->title.widget, width * 0.5f, height * 0.25f);
        results->title.state.fields.box_size = menu_number(results->center_width);
        if (!result_clip(&results->title, &results->title_clip, error) ||
            !ab_ui_scaling_set_to(&menu->scaling, &results->title.widget.frame->fields,
                                  error))
            return false;
        for (size_t index = 0; index < results->button_count; ++index) {
            float center = results->button_count == 2 ? 0.5f : 1;
            result_position(results->buttons + index,
                            width * 0.5f + ((float)index - center) *
                                               results->reference_width * 0.125f,
                            height * 0.8f);
            if (!ab_ui_scaling_set_to(&menu->scaling,
                                      &results->buttons[index].frame->fields, error))
                return false;
        }
        AbVec2 pig;
        if (!menu->context.images.bounds(menu->context.images.context,
                                         menu_span("LEVEL_FAILED_PIG"), &pig, error) ||
            !(pig.y > 0))
            return ab_fail(error, "Missing actual failed-pig reference height");
        float world_scale =
            width / height <= 1024.0f / 768.0f ? width / 1024 : height / 768;
        float pig_scale = (206 / pig.y) * world_scale;
        result_position(&results->pig, width * 0.5f, height * 0.5f);
        results->pig.frame->fields.scale_x = results->pig.frame->fields.scale_y =
            menu_number(pig_scale);
        return true;
    }
    result_position(&results->title.widget, width * 0.5f, height * 0.18f);
    results->title.state.fields.box_size = menu_number(results->center_width);
    if (!result_clip(&results->title, &results->title_clip, error) ||
        !ab_ui_scaling_set_to(&menu->scaling, &results->title.widget.frame->fields,
                              error))
        return false;
    for (size_t index = 0; index < 3; ++index) {
        float x = width * 0.5f + ((float)index - 1) * results->reference_width * 0.125f;
        result_position(results->buttons + index, x, height * 0.8f);
        result_position(results->stars + index, x, height * 0.4f);
        if (!ab_ui_scaling_set_to(&menu->scaling,
                                  &results->buttons[index].frame->fields, error) ||
            !ab_ui_scaling_set_to(&menu->scaling, &results->stars[index].frame->fields,
                                  error))
            return false;
    }
    result_position(&results->counter.widget, width * 0.5f, height * 0.58f);
    if (!ab_ui_scaling_set_to(&menu->scaling, &results->counter.widget.frame->fields,
                              error))
        return false;
    uint8_t bytes[64];
    AbUiPointerValue final_score;
    float counter_width;
    if (!result_number_text(menu_real(game->globals.score.bits), bytes, sizeof(bytes),
                            &final_score, error) ||
        !result_measure(results, results->counter.state.fields.font, final_score,
                        &counter_width, error))
        return false;
    results->badge_end_x = width * 0.5f + results->reference_width * 0.2f;
    results->badge_end_y = height * 0.625f;
    results->badge_start_x = width;
    results->badge_start_y = height * 0.3f;
    result_position(&results->badge, results->badge_end_x, results->badge_end_y);
    if (!ab_ui_scaling_set_to(&menu->scaling, &results->badge.frame->fields, error))
        return false;
    AbVec2 indicator;
    if (!menu->context.images.bounds(menu->context.images.context,
                                     menu_span("INGAME_BUTTON_POWERUPANIM"), &indicator,
                                     error))
        return false;
    result_position(&results->powerup,
                    width * 0.5f - counter_width * scale * 0.5f -
                        indicator.x * scale * 0.5f,
                    height * 0.58f);
    results->powerup.frame->fields.scale_x = results->powerup.frame->fields.scale_y =
        menu_number(0.4f * scale);
    float score = menu_real(game->globals.score.bits);
    float previous = menu_real(game->previous_score.bits);
    if (score <= previous) {
        AbSpan label;
        AbUiPointerValue previous_text = {0};
        if (!ab_text_resources_get(&menu->text_resources, menu_span("TEXTS_BASIC"),
                                   menu_span("TEXT_BEST_SCORE"), &label, error) ||
            !result_number_text(previous, bytes, sizeof(bytes), &previous_text, error))
            return false;
        if (label.size + 1 + previous_text.text.size > sizeof(results->best_bytes))
            return ab_fail(error, "Localized best score exceeds text storage");
        memcpy(results->best_bytes, label.data, label.size);
        results->best_bytes[label.size] = ' ';
        memcpy(results->best_bytes + label.size + 1, previous_text.text.data,
               previous_text.text.size);
        results->best.state.fields.text = (AbUiPointerValue){
            .kind = AB_UI_POINTER_TEXT,
            .text = {results->best_bytes, label.size + 1 + previous_text.text.size}};
        AbVec2 small_star;
        float text_width;
        if (!menu->context.images.bounds(menu->context.images.context,
                                         menu_span("BEST_STAR_GREY"), &small_star,
                                         error) ||
            !result_measure(results, results->best.state.fields.font,
                            results->best.state.fields.text, &text_width, error))
            return false;
        float spacing = small_star.x * 1.1f;
        float left = width * 0.5f - 0.5f * (text_width + 3.25f * spacing) * scale;
        result_position(&results->best.widget, left + text_width * 0.5f * scale,
                        height * 0.65f);
        for (size_t index = 0; index < 3; ++index) {
            result_position(results->best_stars + index,
                            left +
                                (text_width + ((float)index + 0.75f) * spacing) * scale,
                            height * 0.65f);
            if (!ab_ui_scaling_set_to(&menu->scaling,
                                      &results->best_stars[index].frame->fields, error))
                return false;
        }
    } else {
        result_position(&results->best.widget, width * 0.5f, height * 0.64f);
        results->best.state.fields.text = menu_text("MI_NEW_HIGHSCORE");
    }
    if (!ab_ui_scaling_set_to(&menu->scaling, &results->best.widget.frame->fields,
                              error) ||
        !result_clip(&results->best, &results->best_clip, error))
        return false;
    AbVec2 counter_background;
    return menu->context.images.bounds(menu->context.images.context,
                                       menu_span("BG_STAR_COUNTER"),
                                       &counter_background, error);
}

#endif
