#ifndef AB_GAME_RESULTS_DRAW_H
#define AB_GAME_RESULTS_DRAW_H

#include "game_results_layout.h"

static bool result_emit(PreviewResults *results, const char *definition, float amount,
                        AbVec2 position, AbVec2 size, AbError *error) {
    return ab_particles_emit(ab_classic_particles_engine(results->game->particles),
                             &(AbParticleEmission){.definition = menu_span(definition),
                                                   .amount = amount,
                                                   .position = position,
                                                   .size = size,
                                                   .ignore_limits = true,
                                                   .alternate = true},
                             error);
}

static bool result_star(PreviewResults *results, size_t index, AbError *error) {
    PreviewMenu *menu = results->game->menu;
    AbUiWidget *star = results->stars + index;
    const char *const definitions[] = {"levelCompleteStars1", "levelCompleteStars2",
                                       "levelCompleteStars3"};
    const char *const sounds[] = {"star_1", "star_2", "star_3"};
    const char *const images[] = {"BIG_STAR_1", "BIG_STAR_2", "BIG_STAR_3"};
    if (!results->star_achieved[index]) {
        AbVec2 size;
        if (!ab_ui_scaling_sprite(&menu->scaling, &menu->context.images,
                                  star->state.image.image.text, false, &size, error) ||
            !result_emit(results, definitions[index], 40,
                         (AbVec2){menu_real(star->frame->fields.x.number_bits),
                                  menu_real(star->frame->fields.y.number_bits)},
                         (AbVec2){size.x / 3, size.y / 3}, error))
            return false;
        uint32_t handle;
        if (!ab_audio_bank_play_resource(menu->audio, menu_span(sounds[index]), 0.7f,
                                         false, 0, &handle, error))
            return false;
        results->star_achieved[index] = true;
    }
    star->state.image.alpha = menu_number(1);
    return ab_ui_image_state_set_image(&star->state.image, menu_text(images[index]),
                                       &menu->context.images, error);
}

static bool result_highscore(PreviewResults *results, float duration, AbError *error) {
    PreviewGame *game = results->game;
    PreviewMenu *menu = game->menu;
    if (results->timer < duration + 1)
        return true;
    bool improved =
        menu_real(game->globals.score.bits) > menu_real(game->previous_score.bits);
    if (improved && !results->badge_shown) {
        results->badge.frame->fields.visible = result_boolean(true);
        AbVec2 size;
        uint32_t handle;
        if (!menu->context.images.bounds(menu->context.images.context,
                                         menu_span("NEW_HIGHSCORE_BADGE"), &size,
                                         error) ||
            !ab_audio_bank_play_resource(menu->audio, menu_span("new_highscore"), 1,
                                         false, 0, &handle, error) ||
            !result_emit(
                results, "newHighScoreStars", 20,
                (AbVec2){menu_real(results->badge.frame->fields.x.number_bits),
                         menu_real(results->badge.frame->fields.y.number_bits)},
                size, error))
            return false;
        results->badge_shown = true;
    } else if (!improved) {
        for (size_t index = 0; index < 3; ++index)
            results->best_stars[index].frame->fields.visible = result_boolean(true);
    }
    if (improved) {
        float scale;
        if (!ab_ui_scaling_get(&menu->scaling, &scale, error))
            return false;
        float arrival = duration + 1 + 0.2f;
        if (results->timer < arrival) {
            float progress = (results->timer - duration - 1) / 0.2f;
            results->badge.frame->fields.scale_x =
                results->badge.frame->fields.scale_y = menu_number(7 - 6 * progress);
            result_position(
                &results->badge,
                results->badge_start_x +
                    (results->badge_end_x - results->badge_start_x) * progress,
                results->badge_start_y +
                    (results->badge_end_y - results->badge_start_y) * progress);
        } else {
            results->badge.frame->fields.scale_x =
                results->badge.frame->fields.scale_y = menu_number(scale);
            result_position(&results->badge, results->badge_end_x,
                            results->badge_end_y);
        }
        float end = arrival + 0.2f;
        if (results->timer > arrival && results->timer < end) {
            float draws[2];
            for (size_t index = 0; index < 2; ++index)
                if (ab_script_random(&menu->random, 2, 0, 100, draws + index) !=
                    AB_SCRIPT_RANDOM_OK)
                    return ab_fail(error, "Result badge shake random draw failed");
            results->shake_x =
                (draws[0] - 50) * (end - results->timer) * game->screen.y / 768;
            results->shake_y =
                (draws[1] - 50) * (end - results->timer) * game->screen.y / 768;
        } else if (results->timer > end) {
            results->shake_x = results->shake_y = 0;
        }
    }
    results->best.widget.frame->fields.visible = result_boolean(true);
    return true;
}

static bool game_results_update(PreviewGame *game, float seconds, AbError *error) {
    PreviewResults *results = game->results;
    results->timer += seconds;
    if (results->failed) {
        results->shade = fminf(results->shade + seconds, 0.5f);
        return ab_ui_widget_update(&results->root, seconds, game->menu->time, error);
    }
    results->shade = fminf(results->shade + seconds, 0.65f);
    float score = menu_real(game->globals.score.bits);
    float duration = fminf(score / results->silver * 2 - 0.5f, 2.7f);
    float reveal = fminf(results->timer * 1.25f + 0.5f, 3);
    for (float outer = 0; outer <= reveal; outer += 1)
        for (size_t index = 0;
             (float)(index + 1) <= fminf(results->stars_earned, outer); ++index)
            if (!result_star(results, index, error))
                return false;
    float displayed = results->timer < duration
                          ? floorf(score * fminf(results->timer, duration) / duration)
                          : score;
    if (!result_number_text(displayed, results->counter_bytes,
                            sizeof(results->counter_bytes),
                            &results->counter.state.fields.text, error))
        return false;
    if (results->timer >= duration) {
        bool playing;
        if (!ab_audio_bank_is_playing(game->menu->audio, menu_span("score_count"),
                                      &playing, error) ||
            (playing &&
             !ab_audio_bank_stop(game->menu->audio, menu_span("score_count"), error)))
            return false;
    }
    if (!result_highscore(results, duration, error))
        return false;
    /* LevelComplete.update calls LevelEnd.update after its own shade write. */
    results->shade = fminf(results->shade + seconds, 0.5f);
    return ab_ui_widget_update(&results->root, seconds, game->menu->time, error);
}

static bool result_rectangle(PreviewResults *results, float alpha, float left,
                             float top, float right, float bottom, AbError *error) {
    PreviewGame *game = results->game;
    AbGameRectanglePlan plan;
    if (!ab_game_rectangle_plan(&(AbGameRectangle){.color = {0, 0, 0, alpha},
                                                   .left = left,
                                                   .top = top,
                                                   .right = right,
                                                   .bottom = bottom,
                                                   .respect_state = true},
                                &plan, error))
        return false;
    AbDrawCommand command;
    bool visible;
    return ab_fill_rectangle_reset_command(
               (AbFillRectangle){plan.argb, plan.left, plan.top, plan.width,
                                 plan.height, AB_FILL_BLEND_AUTO},
               (uint32_t)game->screen.x, (uint32_t)game->screen.y, 63, &command,
               &visible, error) &&
           (!visible ||
            ab_draw_list_push(game->menu->context.draw_list, &command, error));
}

static bool result_background(PreviewResults *results, AbError *error) {
    PreviewGame *game = results->game;
    game->menu->render.geometry = (AbSpriteRenderState){.scale_x = 1, .scale_y = 1};
    float center = game->screen.x * 0.5f + results->shake_x;
    return result_rectangle(results, results->shade, 0, 0, game->screen.x,
                            game->screen.y, error) &&
           result_rectangle(
               results, results->shade / 0.5f, center - results->center_width * 0.5f, 0,
               center + results->center_width * 0.5f, ceilf(game->screen.y), error);
}

static bool game_results_draw(PreviewGame *game, AbError *error) {
    PreviewResults *results = game->results;
    AbUiDrawInput input = {.transform.position = {results->shake_x, results->shake_y},
                           .present = 3};
    /* Both LevelComplete.draw and its LevelEnd.draw base paint the background. */
    if (!result_background(results, error) ||
        (!results->failed && !result_background(results, error)) ||
        !ab_ui_widget_draw(&results->root, &input, error))
        return false;
    return ab_particles_draw_menu(
        ab_classic_particles_engine(game->particles),
        (AbParticleDrawStyle){.provider = &game->menu->sprites.provider,
                              .width = (uint32_t)game->screen.x,
                              .height = (uint32_t)game->screen.y,
                              .blend = AB_BLEND_PREMULTIPLIED,
                              .color = {1, 1, 1, 1},
                              .opacity = 1},
        1, game->menu->context.draw_list, error);
}

#endif
