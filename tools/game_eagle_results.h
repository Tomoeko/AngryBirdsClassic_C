#ifndef AB_GAME_EAGLE_RESULTS_H
#define AB_GAME_EAGLE_RESULTS_H

/* EagleScore43724/46294/47537 and ProgressBar977. Keep the original feather
 * meter and percentage separate from LevelComplete's star animation. */
static bool result_eagle_meter(void *context, AbUiWidget *widget,
                               const AbUiDrawInput *input, AbError *error) {
    PreviewResults *results = context;
    PreviewMenu *menu = results->game->menu;
    AbUiDrawTransform parent;
    AbVec2 pivot, size;
    if (!ab_ui_draw_resolve(input, &parent) ||
        !menu->context.images.pivot(menu->context.images.context,
                                    menu_span("EAGLE_METER_EMPTY"), &pivot, error) ||
        !menu->context.images.bounds(menu->context.images.context,
                                     menu_span("EAGLE_METER_EMPTY"), &size, error))
        return false;
    float x = menu_real(widget->frame->fields.x.number_bits) + parent.position.x;
    float y = menu_real(widget->frame->fields.y.number_bits) + parent.position.y;
    float sx = menu_real(widget->frame->fields.scale_x.number_bits) * parent.scale.x;
    float sy = menu_real(widget->frame->fields.scale_y.number_bits) * parent.scale.y;
    menu->render.geometry = (AbSpriteRenderState){.translation_x = floorf(x / sx),
                                                  .translation_y = floorf(y / sy),
                                                  .scale_x = sx,
                                                  .scale_y = sy,
                                                  .rotation_origin_x = pivot.x,
                                                  .rotation_origin_y = pivot.y};
    AbUiSpriteRender sprite = {.image = menu_span("EAGLE_METER_EMPTY"),
                               .vertical = AB_ANCHOR_VPIVOT,
                               .horizontal = AB_ANCHOR_HPIVOT};
    if (!ab_ui_sprite_render(&sprite, &menu->render, &menu->context.render,
                             menu->context.draw_list, error))
        return false;
    if (results->feather_value < 100 &&
        !ab_screen_clip((AbScreenClipRect){x - pivot.x * sx, y - pivot.y * sy,
                                           (results->feather_value / 100) * size.x * sx,
                                           size.y * sy},
                        menu->context.screen_clip_offset, &menu->render.clip, error))
        return false;
    sprite.image = menu_span(results->feather_value >= 100 ? "EAGLE_METER_FULL"
                                                           : "EAGLE_METER_FILL");
    if (!ab_ui_sprite_render(&sprite, &menu->render, &menu->context.render,
                             menu->context.draw_list, error) ||
        !ab_screen_clip(
            (AbScreenClipRect){0, 0, results->game->screen.x, results->game->screen.y},
            menu->context.screen_clip_offset, &menu->render.clip, error))
        return false;
    menu->render.geometry = (AbSpriteRenderState){.scale_x = 1, .scale_y = 1};
    return ab_ui_widget_draw_base(widget, input, error);
}

static bool result_eagle_graph(PreviewResults *results, AbError *error) {
    PreviewMenu *menu = results->game->menu;
    if (!result_image(results, &results->meter_effect, "meterEffect",
                      "EAGLE_METER_EFFECT", NULL, error))
        return false;
    results->meter_effect.frame->fields.visible = result_boolean(false);
    AbUiFrameFields frame = {.name = menu_text("feather")};
    if (!ab_ui_widget_init(&results->feather, &menu->context, AB_UI_WIDGET_FRAME,
                           (AbUiWidgetChildren){0}, &frame, NULL, NULL, error) ||
        !ab_ui_widget_add(&results->root, &results->feather, error))
        return false;
    results->feather.methods.context = results;
    results->feather.methods.draw = result_eagle_meter;
    results->feather_value = results->game->eagle->percent;
    if (!result_text(results, &results->counter, "scoreCounter",
                     menu_span("FONT_CHALLENGE_SCORE"), "0%", error) ||
        !result_text(results, &results->best, "bestScore", results->highscore_font, "",
                     error))
        return false;
    results->best.widget.frame->fields.visible = result_boolean(false);
    results->title.state.fields.text = menu_text("TEXT_EAGLE_SCORE");
    return true;
}

static bool result_eagle_layout(PreviewResults *results, AbError *error) {
    PreviewGame *game = results->game;
    PreviewMenu *menu = game->menu;
    float width = game->screen.x, height = game->screen.y;
    result_position(&results->title.widget, width * .5f, height * .185f);
    results->title.state.fields.box_size = menu_number(results->center_width);
    for (size_t index = 0; index < results->button_count; ++index) {
        float offset = (float)index - (results->button_count == 2 ? .5f : 1);
        result_position(results->buttons + index,
                        width * .5f + offset * results->reference_width * .125f,
                        height * .75f);
        if (!ab_ui_scaling_set_to(&menu->scaling,
                                  &results->buttons[index].frame->fields, error))
            return false;
    }
    result_position(&results->meter_effect, width * .5f, height * .41f);
    result_position(&results->feather, width * .5f, height * .41f);
    result_position(&results->counter.widget, width * .5f, height * .555f);
    result_position(&results->best.widget, width * .5f, height * .63f);
    AbUiWidget *widgets[] = {&results->meter_effect, &results->feather,
                             &results->counter.widget, &results->best.widget,
                             &results->title.widget};
    for (size_t index = 0; index < sizeof(widgets) / sizeof(*widgets); ++index)
        if (!ab_ui_scaling_set_to(&menu->scaling, &widgets[index]->frame->fields,
                                  error))
            return false;
    bool improved = game->eagle->percent > game->eagle->previous_percent;
    results->best.widget.frame->fields.visible = result_boolean(!improved);
    if (improved) {
        results->title.state.fields.text = menu_text("TEXT_EAGLE_HIGHSCORE");
    } else {
        AbSpan label;
        if (!ab_text_resources_get(&menu->text_resources, menu_span("TEXTS_BASIC"),
                                   menu_span("TEXT_BEST_SCORE"), &label, error))
            return false;
        int count = snprintf((char *)results->best_bytes, sizeof(results->best_bytes),
                             "%.*s %.0f%%", (int)label.size, (const char *)label.data,
                             (double)game->eagle->previous_percent);
        if (count < 0 || (size_t)count >= sizeof(results->best_bytes))
            return ab_fail(error, "Eagle best-score text exceeds storage");
        results->best.state.fields.text = (AbUiPointerValue){
            .kind = AB_UI_POINTER_TEXT, .text = {results->best_bytes, (size_t)count}};
    }
    return result_clip(&results->title, &results->title_clip, error);
}

static bool result_eagle_update(PreviewResults *results, float seconds,
                                AbError *error) {
    PreviewGame *game = results->game;
    PreviewMenu *menu = game->menu;
    float score = game->eagle->percent;
    float duration = fmaxf(score * .03f, 1);
    float displayed =
        floorf((score * fminf(results->timer, duration)) / fmaxf(duration, 1));
    results->meter_effect.frame->fields.angle = menu_number(results->timer);
    results->feather_value = displayed;
    if (displayed >= 100) {
        results->meter_effect.frame->fields.visible = result_boolean(true);
        if (!results->badge_shown) {
            uint32_t handle;
            if (!ab_audio_bank_play_resource(menu->audio, menu_span("new_highscore"), 1,
                                             false, 0, &handle, error))
                return false;
            results->badge_shown = true;
        }
    }
    int count = snprintf((char *)results->counter_bytes, sizeof(results->counter_bytes),
                         "%.0f%%", (double)displayed);
    if (count < 0 || (size_t)count >= sizeof(results->counter_bytes))
        return ab_fail(error, "Eagle score text exceeds storage");
    results->counter.state.fields.text = (AbUiPointerValue){
        .kind = AB_UI_POINTER_TEXT, .text = {results->counter_bytes, (size_t)count}};
    if (results->timer >= duration) {
        bool playing;
        if (!ab_audio_bank_is_playing(menu->audio, menu_span("score_count"), &playing,
                                      error) ||
            (playing &&
             !ab_audio_bank_stop(menu->audio, menu_span("score_count"), error)))
            return false;
    }
    results->shade = fminf(results->shade + seconds, .5f);
    return ab_ui_widget_update(&results->root, seconds, menu->time, error);
}

#endif
