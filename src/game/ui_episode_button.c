#include "ab/ui_episode_button.h"

#include "ab/camera_mover.h"
#include "ab/ui_value_text.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static AbUiPointerValue number(float value) {
    AbUiPointerValue output = {.kind = AB_UI_POINTER_NUMBER};
    memcpy(&output.number_bits, &value, sizeof(value));
    return output;
}

static AbUiPointerValue text(const char *value) {
    return (AbUiPointerValue){.kind = AB_UI_POINTER_TEXT,
                              .text = {(const uint8_t *)value, strlen(value)}};
}

static AbUiPointerValue boolean(bool value) {
    return (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN, .boolean = value};
}

static bool truthy(AbUiPointerValue value) {
    return ab_ui_pointer_value_truthy(&value);
}

static bool named(AbUiPointerValue value, const char *name) {
    return value.kind == AB_UI_POINTER_TEXT && value.text.size == strlen(name) &&
           value.text.data && !memcmp(value.text.data, name, value.text.size);
}

static bool numeric(AbUiPointerValue value, float *output, AbError *error) {
    if (value.kind != AB_UI_POINTER_NUMBER) {
        ab_fail(error, "Nonnumeric EpisodeButton field");
        return false;
    }
    memcpy(output, &value.number_bits, sizeof(*output));
    return isfinite(*output) || ab_fail(error, "Nonfinite EpisodeButton field");
}

static bool valid(AbUiEpisodeButton *button, AbError *error) {
    return button && button->widget.initialized && button->widget.context &&
                   !button->widget.context->failed
               ? true
               : ab_fail(error, "Invalid or failed EpisodeButton");
}

static bool finish(AbUiEpisodeButton *button, bool okay) {
    if (!okay && button && button->widget.context)
        button->widget.context->failed = true;
    return okay;
}

static const AbDataValue *field(const AbDataTable *table, const char *name) {
    return ab_data_get(
        table, (AbDataValue){.kind = AB_DATA_STRING, .string = text(name).text});
}

static bool value(const AbDataValue *input, AbUiPointerValue *output, AbError *error) {
    if (!input || input->kind == AB_DATA_NIL)
        *output = (AbUiPointerValue){0};
    else if (input->kind == AB_DATA_BOOLEAN)
        *output = boolean(input->bits != 0);
    else if (input->kind == AB_DATA_NUMBER)
        *output = (AbUiPointerValue){.kind = AB_UI_POINTER_NUMBER,
                                     .number_bits = input->bits};
    else if (input->kind == AB_DATA_STRING)
        *output = (AbUiPointerValue){.kind = AB_UI_POINTER_TEXT, .text = input->string};
    else {
        ab_fail(error, "Unsupported EpisodeButton metadata value");
        return false;
    }
    return ab_ui_pointer_value_valid(output) ||
           ab_fail(error, "Invalid EpisodeButton metadata value");
}

static bool metadata(AbUiEpisodeButton *button, const AbDataTable **output,
                     AbError *error) {
    AbDataValue key = {0};
    if (button->episode.kind == AB_UI_POINTER_TEXT)
        key = (AbDataValue){.kind = AB_DATA_STRING, .string = button->episode.text};
    else if (button->episode.kind == AB_UI_POINTER_NUMBER)
        key =
            (AbDataValue){.kind = AB_DATA_NUMBER, .bits = button->episode.number_bits};
    else {
        ab_fail(error, "Missing numeric/text EpisodeButton episode");
        return false;
    }
    const AbDataValue *entry = ab_data_get(button->host.episodes, key);
    if (!entry || entry->kind != AB_DATA_TABLE || !entry->table) {
        ab_fail(error, "Missing EpisodeButton catalog record");
        return false;
    }
    *output = entry->table;
    return true;
}

static bool set_image(AbUiEpisodeButton *button, AbUiWidget *image, const char *name,
                      AbError *error) {
    return ab_ui_image_state_set_image(&image->state.image, text(name),
                                       &button->widget.context->images, error);
}

static bool ugc_opened(AbUiEpisodeButton *button, bool *output, AbError *error) {
    if (!button->host.settings || !button->host.ugc_class)
        return ab_fail(error, "Missing actual UGC settings/class state");
    AbUiPointerValue value_read;
    if (!value(field(button->host.settings, "UGCEpisodeCardGiftBoxOpened"), &value_read,
               error))
        return false;
    *output = truthy(value_read);
    return true;
}

static bool random_timer(AbUiEpisodeButton *button, AbError *error) {
    float timer;
    if (ab_script_random(button->host.random, 2, -6, -3, &timer) != AB_SCRIPT_RANDOM_OK)
        return ab_fail(error, "Missing or invalid shared EpisodeButton script RNG");
    button->new_timer = number(timer);
    return true;
}

static bool new_sign(AbUiEpisodeButton *button, AbError *error) {
    if (button->class_kind == AB_UI_EPISODE_GOLDEN_EGGS)
        return ab_fail(error, "GoldenEggButton has no new-sign method");
    button->new_sign.frame->fields.visible = boolean(false);
    if (truthy(button->disable_new_sign))
        return true;
    if (!set_image(button, &button->new_sign, "BUTTON_NEWLEVELS", error))
        return false;
    if (button->episode.kind == AB_UI_POINTER_NONE)
        return true;
    AbUiEpisodeButtonHost *host = &button->host;
    bool complete, seen, new_levels, visited;
    if (!host->complete ||
        !host->complete(host->context, button->episode, &complete, error))
        return ab_fail(error, "Missing EpisodeButton completion query");
    if (complete)
        return true;
    if (!host->newest_change_seen ||
        !host->newest_change_seen(host->context, button->episode, &seen, error) ||
        !host->has_new_levels ||
        !host->has_new_levels(host->context, button->episode, &new_levels, error))
        return ab_fail(error, "Missing EpisodeButton new-level/version query");
    bool unseen = !seen;
    if (!new_levels && !unseen)
        return true;
    AbUiPointerValue page;
    if (!host->new_levels_page ||
        !host->new_levels_page(host->context, button->episode, &page, error) ||
        !host->page_visited ||
        !host->page_visited(host->context, button->episode, page, &visited, error))
        return ab_fail(error, "Missing EpisodeButton page visit query");
    if (visited && !unseen)
        return true;
    button->new_sign.frame->fields.visible = boolean(true);
    return random_timer(button, error);
}

bool ab_ui_episode_button_new_sign(AbUiEpisodeButton *button, AbError *error) {
    return valid(button, error) && finish(button, new_sign(button, error));
}

static bool set_episode(AbUiEpisodeButton *button, AbUiPointerValue episode,
                        const AbDataTable *supplied, AbError *error) {
    button->episode = episode;
    button->widget.state.meta = episode;
    if (!supplied ||
        !value(field(supplied, "disableNewSign"), &button->disable_new_sign, error))
        return ab_fail(error, "Missing EpisodeButton supplied metadata");
    if (button->class_kind == AB_UI_EPISODE_GOLDEN_EGGS)
        return ab_ui_image_button_state_set_image(
            &button->widget.state, text("BTN_GOLDENEGGS"), (AbUiPointerValue){0},
            &button->widget.context->images, error);
    bool wrapped = false;
    if (button->class_kind == AB_UI_EPISODE_UGC) {
        bool opened;
        if (!ugc_opened(button, &opened, error))
            return false;
        wrapped = !opened;
    }
    const AbDataTable *record;
    AbUiPointerValue image;
    if (!metadata(button, &record, error) ||
        !value(field(record, wrapped ? "button_wrapped" : "button"), &image, error) ||
        !ab_ui_image_button_state_set_image(&button->widget.state, image,
                                            (AbUiPointerValue){0},
                                            &button->widget.context->images, error))
        return false;
    if (wrapped)
        button->host.ugc_class->wrapped_selected = true;
    return new_sign(button, error);
}

bool ab_ui_episode_button_set_episode(AbUiEpisodeButton *button,
                                      AbUiPointerValue episode,
                                      const AbDataTable *supplied, AbError *error) {
    return valid(button, error) &&
           finish(button, set_episode(button, episode, supplied, error));
}

bool ab_ui_episode_button_set_open(AbUiEpisodeButton *button, AbUiPointerValue open,
                                   AbError *error) {
    if (!valid(button, error) || !ab_ui_pointer_value_valid(&open))
        return false;
    button->locked = boolean(!truthy(open));
    return finish(button, ab_ui_image_button_state_enable(
                              &button->widget.state, open,
                              &button->widget.context->images, error));
}

static bool fraction(float earned, float available, uint8_t *storage, size_t capacity,
                     AbUiPointerValue *output, AbError *error) {
    uint8_t first_bytes[32], second_bytes[32];
    AbSpan first, second;
    if (!ab_ui_value_text(number(earned), first_bytes, sizeof(first_bytes), &first) ||
        !ab_ui_value_text(number(available), second_bytes, sizeof(second_bytes),
                          &second) ||
        first.size >= capacity || second.size > capacity - first.size - 1)
        return ab_fail(error, "EpisodeButton fraction text exceeds storage");
    memcpy(storage, first.data, first.size);
    storage[first.size] = '/';
    memcpy(storage + first.size + 1, second.data, second.size);
    *output = (AbUiPointerValue){.kind = AB_UI_POINTER_TEXT,
                                 .text = {storage, first.size + 1 + second.size}};
    return true;
}

static bool update_scores(AbUiEpisodeButton *button, AbError *error) {
    AbUiEpisodeButtonHost *host = &button->host;
    float earned, available, score;
    if (button->class_kind == AB_UI_EPISODE_GOLDEN_EGGS) {
        AbSpan label;
        if (!host->stars ||
            !host->stars(host->context, text("G"), &earned, &available, error) ||
            !ab_ui_value_text(number(earned), button->star_label,
                              sizeof(button->star_label), &label))
            return ab_fail(error, "Missing GoldenEggButton star count");
        button->stars.state.fields.text =
            (AbUiPointerValue){.kind = AB_UI_POINTER_TEXT, .text = label};
        return true;
    }
    if (button->episode.kind == AB_UI_POINTER_NONE || !host->stars ||
        !host->stars(host->context, button->episode, &earned, &available, error) ||
        !fraction(earned, available, button->star_label, sizeof(button->star_label),
                  &button->stars.state.fields.text, error))
        return ab_fail(error, "Missing or invalid EpisodeButton star totals");
    if (!host->score || !host->score(host->context, button->episode, &score, error) ||
        !isfinite(score) || score < -2147483648.0f || score >= 2147483648.0f)
        return ab_fail(error, "Missing or invalid EpisodeButton total score");
    int count = snprintf((char *)button->score_label, sizeof(button->score_label), "%d",
                         (int)score);
    if (count < 0 || (size_t)count >= sizeof(button->score_label))
        return ab_fail(error, "EpisodeButton score text exceeds storage");
    button->score.state.fields.text = (AbUiPointerValue){
        .kind = AB_UI_POINTER_TEXT, .text = {button->score_label, (size_t)count}};
    bool show;
    if (!host->show_eagle ||
        !host->show_eagle(host->context, button->episode, &show, error))
        return ab_fail(error, "Missing EpisodeButton eagle visibility query");
    const AbDataTable *record;
    AbUiPointerValue disabled;
    if (!metadata(button, &record, error) ||
        !value(field(record, "disableEpisodeButtonScores"), &disabled, error))
        return false;
    if (!truthy(disabled)) {
        button->feather_box.frame->fields.visible = boolean(show);
        button->feather_score.state.frame.fields.visible = boolean(show);
    }
    if (show &&
        (!host->feathers ||
         !host->feathers(host->context, button->episode, &earned, &available, error) ||
         !fraction(earned, available, button->feather_label,
                   sizeof(button->feather_label),
                   &button->feather_score.state.fields.text, error)))
        return ab_fail(error, "Missing or invalid EpisodeButton feather totals");
    if (host->pc_profile) {
        button->feather_box.frame->fields.visible = boolean(false);
        button->feather_score.state.frame.fields.visible = boolean(false);
    }
    return new_sign(button, error);
}

bool ab_ui_episode_button_update_scores(AbUiEpisodeButton *button, AbError *error) {
    return valid(button, error) && finish(button, update_scores(button, error));
}

static bool scaled_size(AbUiEpisodeButton *button, AbUiPointerValue image,
                        AbVec2 *output, AbError *error) {
    return image.kind == AB_UI_POINTER_TEXT
               ? ab_ui_scaling_sprite(button->host.scaling,
                                      &button->widget.context->images, image.text,
                                      false, output, error)
               : ab_fail(error, "Missing EpisodeButton layout image");
}

static bool layout_new_sign(AbUiEpisodeButton *button, AbVec2 size, float sx,
                            AbError *error) {
    AbUiFrameFields *sign = &button->new_sign.frame->fields;
    if (!truthy(sign->visible))
        return true;
    float width, height, scale;
    if (!numeric(sign->w, &width, error) || !numeric(sign->h, &height, error) ||
        !ab_ui_scaling_get(button->host.scaling, &scale, error))
        return false;
    float x, y;
    if (named(button->episode, "FLOCKFAVORITES")) {
        x = (-size.x * 0.5f) + (width * 0.2f) * scale;
        y = (-size.y * 0.5f) + (height * 0.2f) * scale;
    } else {
        float fraction = width * sx < size.x * 0.5f ? 0.52f : 0.6f;
        x = size.x * 0.5f - (width * fraction) * scale;
        y = (-size.y * 0.5f) + (height * 0.4f) * scale;
    }
    sign->x = number(x);
    sign->y = number(y);
    button->new_original_position = (AbVec2){x, y};
    return true;
}

static bool golden_layout(AbUiEpisodeButton *button, AbError *error) {
    if (!ab_ui_widget_lifecycle_base(&button->widget, AB_UI_FRAME_LAYOUT, error))
        return false;
    button->lock.frame->fields.visible = button->locked;
    const AbDataTable *record;
    if (!metadata(button, &record, error) ||
        !set_episode(button, button->episode, record, error))
        return false;
    AbUiImageResourceBoundaries *resources = &button->widget.context->images;
    AbUiPointerValue image = button->widget.state.image.image;
    AbUiPointerValue box = button->star_box.state.image.image;
    AbVec2 size, box_size;
    float sx, sy;
    if (image.kind != AB_UI_POINTER_TEXT || box.kind != AB_UI_POINTER_TEXT ||
        !resources->bounds ||
        !resources->bounds(resources->context, image.text, &size, error) ||
        !resources->bounds(resources->context, box.text, &box_size, error) ||
        !numeric(button->widget.frame->fields.scale_x, &sx, error) ||
        !numeric(button->widget.frame->fields.scale_y, &sy, error))
        return ab_fail(error, "Invalid GoldenEggButton layout resources");
    float y = (size.y * 0.5f) * sy - (box_size.y * 0.5f) * sy;
    y = y - ((size.x - box_size.x) * 0.5f) * sx;
    button->star_box.frame->fields.y = number(y);
    button->stars.state.frame.fields.y = number(y + (box_size.y * 0.25f) * sy);
    return true;
}

static bool layout(AbUiEpisodeButton *button, AbError *error) {
    if (button->class_kind == AB_UI_EPISODE_GOLDEN_EGGS)
        return golden_layout(button, error);
    const AbDataTable *record;
    bool ugc = button->class_kind == AB_UI_EPISODE_UGC;
    if ((!ugc && (!metadata(button, &record, error) ||
                  !set_episode(button, button->episode, record, error))) ||
        !ab_ui_widget_lifecycle_base(&button->widget, AB_UI_FRAME_LAYOUT, error))
        return false;
    button->lock.frame->fields.visible = button->locked;
    if (ugc && !ab_ui_scaling_set_to(button->host.scaling,
                                     &button->widget.frame->fields, error))
        return false;
    AbUiPointerValue image = button->reference_button
                                 ? button->reference_button->state.image.image
                                 : (AbUiPointerValue){0};
    if (!truthy(image))
        image = button->widget.state.image.image;
    AbVec2 size, star_size, score_size;
    if (!scaled_size(button, image, &size, error) ||
        !scaled_size(button, button->star_box.state.image.image, &star_size, error) ||
        (!ugc && !set_image(button, &button->score_box, "EP_BTN_SCOREBOX", error)) ||
        !scaled_size(button, button->score_box.state.image.image, &score_size, error))
        return false;
    if (ugc) {
        button->ribbon.frame->fields.x = number(0);
        button->ribbon.frame->fields.y = number(size.y * -0.445f);
    }
    AbUiTextMetricBoundaries fonts =
        ab_ui_font_resources_boundaries(button->host.fonts);
    float unused_leading;
    if (!fonts.resources.set_font(fonts.resources.context,
                                  button->score.state.fields.font, error) ||
        !fonts.leading(fonts.resources.context, &unused_leading, error))
        return false;
    AbUiFrameFields *own = button->widget.frame ? &button->widget.frame->fields : NULL;
    float width, sx, sy, star_width, feather_width;
    if (!own || !numeric(own->w, &width, error) || !numeric(own->scale_x, &sx, error) ||
        !numeric(own->scale_y, &sy, error) ||
        !numeric(button->star_box.frame->fields.w, &star_width, error) ||
        !numeric(button->feather_box.frame->fields.w, &feather_width, error))
        return false;
    float x = ugc ? size.x * 0.5f : (size.x - width * sx) / 2;
    if (!(x <= 0))
        x = 0;
    button->star_box.frame->fields.x = button->score_box.frame->fields.x = number(x);
    button->feather_box.frame->fields.x = number(x + (star_width - feather_width) / 4);
    if (ugc && (!set_image(button, &button->score_box, "EP_BTN_SCOREBOX", error) ||
                !set_image(button, &button->ribbon, "UGC_EPISODE_CARD_BANNER", error)))
        return false;
    float gap = (size.x - star_size.x) * 0.25f;
    float label_offset = star_size.y * 0.25f;
    bool eagle;
    if (!button->host.show_eagle ||
        !button->host.show_eagle(button->host.context, button->episode, &eagle, error))
        return ab_fail(error, "Missing EpisodeButton layout eagle query");
    float star_y, score_y;
    if (!eagle) {
        star_y = (size.y * 0.5f - (star_size.y * 0.5f) * sy) - gap * 2;
        score_y =
            ((star_y - star_size.y * 0.5f) - (score_size.y * 0.5f) * sy) - gap * 2;
        button->feather_box.frame->fields.visible = boolean(false);
        button->feather_score.state.frame.fields.visible = boolean(false);
    } else {
        button->feather_box.frame->fields.visible = boolean(true);
        button->feather_score.state.frame.fields.visible = boolean(true);
        AbVec2 feather_size;
        if (!scaled_size(button, button->feather_box.state.image.image, &feather_size,
                         error))
            return false;
        float feather_y = (size.y * 0.5f - feather_size.y * 0.5f) - gap * 2;
        button->feather_box.frame->fields.y = number(feather_y);
        star_y = ((feather_y - feather_size.y * 0.5f) - star_size.y * 0.5f) - gap;
        score_y = ((star_y - star_size.y * 0.5f) - score_size.y * 0.5f) - gap;
        button->feather_score.state.frame.fields.x = number(star_size.x * 0.2f);
        button->feather_score.state.frame.fields.y = number(feather_y + 0.0f);
    }
    button->star_box.frame->fields.y = number(star_y);
    button->score_box.frame->fields.y = number(score_y);
    button->stars.state.frame.fields.y = number(star_y + label_offset);
    button->score.state.frame.fields.y = number(score_y + label_offset);
    if (!layout_new_sign(button, size, sx, error))
        return false;
    if (ugc) {
        if (!button->host.ugc_class)
            return ab_fail(error, "Missing UGC class state during layout");
        if (!button->host.ugc_class->normal) {
            AbUiWidget *hidden[] = {&button->ribbon,
                                    &button->score_box,
                                    &button->star_box,
                                    &button->feather_box,
                                    &button->feather_score.widget,
                                    &button->score.widget,
                                    &button->stars.widget,
                                    &button->new_sign};
            for (size_t index = 0; index < sizeof(hidden) / sizeof(hidden[0]); ++index)
                hidden[index]->frame->fields.visible = boolean(false);
        } else {
            button->new_sign.frame->fields.visible = boolean(false);
        }
    }
    if (button->class_kind == AB_UI_EPISODE_EGG_DEFENDER) {
        AbUiImageResourceBoundaries *resources = &button->widget.context->images;
        AbUiPointerValue sign = button->new_sign.state.image.image;
        AbVec2 bounds;
        AbUiScaling *scaling = button->host.scaling;
        AbUiScalingEnvironment environment;
        float y;
        if (!resources->bounds || sign.kind != AB_UI_POINTER_TEXT ||
            !resources->bounds(resources->context, sign.text, &bounds, error) ||
            !scaling || !scaling->host.environment ||
            !scaling->host.environment(scaling->host.context, &environment, error) ||
            !isfinite(environment.screen.y) ||
            !numeric(button->new_sign.frame->fields.y, &y, error))
            return ab_fail(error, "Invalid Egg Defender new-sign layout inputs");
        button->new_sign.frame->fields.y = number(fmaxf(
            y - bounds.y / 4.0f, (-environment.screen.y) / 2.0f + bounds.y * 0.9f));
    }
    return true;
}

static bool named_draw(void *context, AbUiWidget *widget, AbSpan method,
                       const AbUiDrawInput *input, AbError *error) {
    (void)context;
    if (!input || (input->present & ~63u))
        return ab_fail(error, "Invalid EpisodeButton phase transform");
    AbVec2 position = {(input->present & 1) ? input->transform.position.x : 0,
                       (input->present & 2) ? input->transform.position.y : 0};
    if (!isfinite(position.x) || !isfinite(position.y))
        return ab_fail(error, "Nonfinite EpisodeButton phase position");
    AbUiDrawInput phase = {
        .transform = {.position = position, .scale = {1, 1}, .local_factor = 1},
        .present = 63};
    AbUiPointerValue name = {.kind = AB_UI_POINTER_TEXT, .text = method};
    if (named(name, "drawBackground")) {
        phase.transform.angle = (input->present & 16) ? input->transform.angle : 0;
        return ab_ui_widget_draw_self(widget, &phase, error);
    }
    AbUiPointerValue tag;
    if (named(name, "drawMidground"))
        tag = text("boxes");
    else if (named(name, "drawForeground"))
        tag = text("scores");
    else
        return ab_fail(error, "Unknown EpisodeButton draw phase");
    AbUiImageButtonAnimation animation = {.hover_enabled =
                                              truthy(widget->state.hover_enabled)};
    if (animation.hover_enabled &&
        !numeric(widget->state.hover_scale, &animation.hover_scale, error))
        return false;
    if (!ab_ui_image_button_hover_scale(&animation, &phase.transform.local_factor))
        return ab_fail(error, "Invalid EpisodeButton phase hover scale");
    return ab_ui_widget_draw_with_tag(widget, &phase, &tag, error);
}

static float bounce(float time) {
    float position = time / 0.8f;
    float curve;
    if (position < 0.3636363744735718f) {
        curve = 0.15f * 7.5625f;
        curve = curve * position;
        curve = curve * position;
        return curve + 0.0f;
    }
    float offset;
    if (position < 0.7272727489471436f) {
        position = position - 0.5454545617103577f;
        offset = 0.75f;
    } else if (position < 0.9090909361839294f) {
        position = position - 0.8181818127632141f;
        offset = 0.9375f;
    } else {
        position = position - 0.9545454382896423f;
        offset = 0.984375f;
    }
    curve = 7.5625f * position;
    curve = curve * position;
    curve = curve + offset;
    curve = 0.15f * curve;
    return curve + 0.0f;
}

static bool update_new_sign(AbUiEpisodeButton *button, float seconds, AbError *error) {
    if (!truthy(button->new_sign.frame->fields.visible))
        return true;
    float timer = 0;
    if (truthy(button->new_timer) && !numeric(button->new_timer, &timer, error))
        return false;
    timer = timer + seconds;
    if (!isfinite(timer))
        return ab_fail(error, "EpisodeButton new-sign timer overflow");
    button->new_timer = number(timer);
    float scale = 1;
    if (timer > 0 && timer < 0.2f) {
        float change;
        if (!ab_camera_tween(AB_CAMERA_TWEEN_CUBIC_OUT, timer, 0, 0.15f, 0.2f, &change))
            return ab_fail(error, "Invalid EpisodeButton new-sign cubic tween");
        scale = 1.0f - change;
    } else if (timer > 0.2f && timer < 1) {
        scale = 0.85f + bounce(timer - 0.2f);
    } else if (timer > 1 && !random_timer(button, error)) {
        return false;
    }
    button->new_sign.frame->fields.scale_x = button->new_sign.frame->fields.scale_y =
        number(scale);
    return true;
}

static bool pingpong(float *time, bool *reverse, float seconds, float start, float end,
                     float *output, AbError *error) {
    /* These two concrete source tween pairs use duration 1, no delay, no repeat
     * count and no finished/wrapped methods. Overshoot is retained in time. */
    *time = *time + (*reverse ? -seconds : seconds);
    if (!isfinite(*time))
        return ab_fail(error, "UGC card tween time overflow");
    float clamped = fmaxf(0, fminf(1, *time));
    if (*reverse ? clamped <= 0 : clamped >= 1)
        *reverse = !*reverse;
    float value = (end - start) * clamped;
    *output = value + start;
    return isfinite(*output) || ab_fail(error, "UGC card tween value overflow");
}

static bool update_ugc(AbUiEpisodeButton *button, float seconds, AbError *error) {
    AbUiEpisodeUgcClass *state = button->host.ugc_class;
    if (!state)
        return ab_fail(error, "Missing UGC card class state during update");
    float scale;
    if (state->normal) {
        if (!pingpong(&button->ribbon_time, &button->ribbon_reverse, seconds, 0.975f,
                      1.025f, &scale, error))
            return false;
        button->ribbon.frame->fields.scale_x = button->ribbon.frame->fields.scale_y =
            number(scale);
        return true;
    }
    if (!pingpong(&button->gift_time, &button->gift_reverse, seconds,
                  button->gift_start, button->gift_end, &scale, error))
        return false;
    button->widget.frame->fields.scale_x = button->widget.frame->fields.scale_y =
        number(scale);
    bool opened;
    if (!ugc_opened(button, &opened, error))
        return false;
    if (!opened)
        return true;
    state->normal = true;
    AbUiWidget *shown[] = {&button->ribbon,
                           &button->score_box,
                           &button->star_box,
                           &button->feather_box,
                           &button->feather_score.widget,
                           &button->score.widget,
                           &button->stars.widget};
    for (size_t index = 0; index < sizeof(shown) / sizeof(shown[0]); ++index)
        shown[index]->frame->fields.visible = boolean(true);
    const AbDataTable *record;
    AbUiPointerValue image;
    if (!metadata(button, &record, error) ||
        !value(field(record, "button"), &image, error) ||
        !ab_ui_image_button_state_set_image(&button->widget.state, image,
                                            (AbUiPointerValue){0},
                                            &button->widget.context->images, error))
        return false;
    AbVec2 size;
    if (!scaled_size(button, image, &size, error))
        return false;
    if (!button->widget.tree.parent)
        return true;
    AbUiFrameNode *parent_node = button->widget.tree.parent;
    AbUiWidget *parent = parent_node->context;
    float parent_x, x, y;
    if (!parent || &parent->tree != parent_node ||
        parent->context != button->widget.context ||
        !numeric(parent->scroll[0], &parent_x, error) ||
        !numeric(button->widget.frame->fields.x, &x, error) ||
        !numeric(button->widget.frame->fields.y, &y, error))
        return ab_fail(error, "UGC unwrap requires its actual scroll parent");
    AbParticleEmission emission = {.definition = text("giftBoxUnwrapBuff").text,
                                   .amount = 100,
                                   .position = {x - parent_x, y},
                                   .size = size,
                                   .ignore_limits = true,
                                   .alternate = true};
    return ab_particles_emit(button->host.particles, &emission, error);
}

static bool update(void *context, AbUiWidget *widget, float seconds, float time,
                   AbError *error) {
    AbUiEpisodeButton *button = context;
    if (!ab_ui_widget_update_base(widget, seconds, time, error))
        return false;
    if (button->class_kind == AB_UI_EPISODE_GOLDEN_EGGS)
        return true;
    if (!update_new_sign(button, seconds, error))
        return false;
    return button->class_kind != AB_UI_EPISODE_UGC ||
           update_ugc(button, seconds, error);
}

static bool lifecycle(void *context, AbUiWidget *widget, AbUiFrameMethod method,
                      AbError *error) {
    AbUiEpisodeButton *button = context;
    if (method == AB_UI_FRAME_LAYOUT)
        return layout(button, error);
    if (!ab_ui_widget_lifecycle_base(widget, method, error))
        return false;
    return method != AB_UI_FRAME_ENTRY || update_scores(button, error);
}

static bool image_child(AbUiEpisodeButton *button, AbUiWidget *child, const char *name,
                        const char *sprite, const char *tag, bool hidden,
                        AbError *error) {
    if (!ab_ui_widget_init(child, button->widget.context, AB_UI_WIDGET_IMAGE,
                           (AbUiWidgetChildren){0}, NULL, NULL, NULL, error))
        return false;
    child->frame->fields.name = text(name);
    if (!set_image(button, child, sprite, error))
        return false;
    if (hidden)
        child->frame->fields.visible = boolean(false);
    child->tag = text(tag);
    return ab_ui_widget_add(&button->widget, child, error);
}

static bool text_child(AbUiEpisodeButton *button, AbUiTextWidget *child,
                       const char *name, const char *label, AbError *error) {
    if (!ab_ui_text_widget_init(child, button->widget.context, (AbUiWidgetChildren){0},
                                button->host.fonts, NULL, NULL, error))
        return false;
    child->state.frame.fields.name = text(name);
    child->state.fields.text = text(label);
    child->state.fields.font = text("FONT_LS_SMALL");
    child->widget.tag = text("scores");
    return ab_ui_widget_add(&button->widget, &child->widget, error);
}

bool ab_ui_episode_button_init(AbUiEpisodeButton *button, AbUiWidgetContext *context,
                               const AbUiImageButtonArguments *arguments,
                               AbUiEpisodeButtonHost host, AbError *error) {
    return ab_ui_episode_button_init_class(button, context, arguments, host,
                                           AB_UI_EPISODE_STANDARD, error);
}

bool ab_ui_episode_button_init_class(AbUiEpisodeButton *button,
                                     AbUiWidgetContext *context,
                                     const AbUiImageButtonArguments *arguments,
                                     AbUiEpisodeButtonHost host,
                                     AbUiEpisodeButtonClass class_kind,
                                     AbError *error) {
    if (!button || button->widget.initialized || !host.fonts ||
        (unsigned)class_kind > AB_UI_EPISODE_UGC)
        return ab_fail(error, "Invalid EpisodeButton constructor");
    button->host = host;
    button->class_kind = class_kind;
    if (!ab_ui_widget_init(
            &button->widget, context, AB_UI_WIDGET_IMAGE_BUTTON,
            (AbUiWidgetChildren){button->frames, button->draw, button->pointer, 9},
            NULL, NULL, arguments, error))
        return false;
    button->widget.state.activate_on_release = boolean(true);
    if (!image_child(button, &button->lock, "lock", "LS_LEVEL_PACK_LOCK", "boxes", true,
                     error))
        return false;
    bool okay;
    if (class_kind == AB_UI_EPISODE_GOLDEN_EGGS) {
        okay = image_child(button, &button->star_box, "star_box", "EP_BTN_STARBOX",
                           "boxes", false, error) &&
               text_child(button, &button->stars, "stars", "", error);
    } else {
        if (class_kind == AB_UI_EPISODE_UGC &&
            !image_child(button, &button->ribbon, "birdDayRibbonImage",
                         "UGC_EPISODE_CARD_BANNER", "boxes", false, error))
            return finish(button, false);
        okay = image_child(button, &button->score_box, "score_box", "EP_BTN_SCOREBOX",
                           "boxes", false, error) &&
               text_child(button, &button->score, "score", "0", error) &&
               image_child(button, &button->star_box, "star_box", "EP_BTN_STARBOX",
                           "boxes", false, error) &&
               text_child(button, &button->stars, "stars", "", error) &&
               image_child(button, &button->feather_box, "feather_box",
                           "EP_BTN_EAGLEBOX", "boxes", false, error) &&
               image_child(button, &button->new_sign, "new_sign", "BUTTON_NEWLEVELS",
                           "scores", false, error) &&
               text_child(button, &button->feather_score, "feather_score", "0", error);
    }
    if (!okay)
        return finish(button, false);
    if (host.pc_profile && class_kind != AB_UI_EPISODE_GOLDEN_EGGS) {
        button->feather_box.frame->fields.visible = boolean(false);
        button->feather_score.state.frame.fields.visible = boolean(false);
    }
    if (class_kind == AB_UI_EPISODE_UGC) {
        bool opened;
        if (!ugc_opened(button, &opened, error))
            return finish(button, false);
        host.ugc_class->normal = opened;
        if (!ab_ui_scaling_get(host.scaling, &button->gift_start, error))
            return finish(button, false);
        button->gift_end = button->gift_start * 0.975f;
        /* The source constructs a third tweener from an absent global; it has
         * no properties, no effects and is never updated by this class. */
    }
    button->widget.methods = (AbUiWidgetMethods){.context = button,
                                                 .lifecycle = lifecycle,
                                                 .update = update,
                                                 .named_draw = named_draw};
    return true;
}
