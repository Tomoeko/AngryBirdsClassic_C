#include "ab/floating_scores.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

struct AbFloatingScores {
    AbFloatingScoreLimits limits;
    AbBirdCollisionFloatingScore *records;
    size_t *active;
    uint8_t *text;
    size_t count, used, bytes;
};

bool ab_floating_scores_open(AbFloatingScoreLimits limits, AbFloatingScores **output,
                             AbError *error) {
    if (!output)
        return ab_fail(error, "Missing floating-score output");
    *output = NULL;
    if (limits.storage_bytes < sizeof(AbFloatingScores) ||
        (limits.records && !limits.text_bytes))
        return ab_fail(error, "Invalid floating-score byte or text limit");
    size_t remaining = limits.storage_bytes - sizeof(AbFloatingScores);
    if (limits.records > remaining / sizeof(AbBirdCollisionFloatingScore))
        return ab_fail(error, "Floating-score records exceed byte limit");
    remaining -= limits.records * sizeof(AbBirdCollisionFloatingScore);
    if (limits.records > remaining / sizeof(size_t))
        return ab_fail(error, "Floating-score active list exceeds byte limit");
    remaining -= limits.records * sizeof(size_t);
    if (limits.records && limits.text_bytes > remaining / limits.records)
        return ab_fail(error, "Floating-score text exceeds byte limit");
    AbFloatingScores *scores = calloc(1, sizeof(*scores));
    if (!scores)
        return ab_fail(error, "Cannot allocate floating scores");
    scores->limits = limits;
    scores->bytes =
        sizeof(*scores) +
        limits.records * (sizeof(*scores->records) + sizeof(*scores->active)) +
        limits.records * limits.text_bytes;
    if (limits.records) {
        scores->records = calloc(limits.records, sizeof(*scores->records));
        scores->active = calloc(limits.records, sizeof(*scores->active));
        scores->text = malloc(limits.records * limits.text_bytes);
        if (!scores->records || !scores->active || !scores->text) {
            ab_floating_scores_close(scores);
            return ab_fail(error, "Cannot allocate floating-score storage");
        }
    }
    *output = scores;
    return true;
}

void ab_floating_scores_close(AbFloatingScores *scores) {
    if (scores) {
        free(scores->records);
        free(scores->active);
        free(scores->text);
        free(scores);
    }
}

size_t ab_floating_scores_bytes(const AbFloatingScores *scores) {
    return scores ? scores->bytes : 0;
}

size_t ab_floating_scores_count(const AbFloatingScores *scores) {
    return scores ? scores->count : 0;
}

size_t ab_floating_scores_used(const AbFloatingScores *scores) {
    return scores ? scores->used : 0;
}

bool ab_floating_scores_add(void *context, const AbBirdCollisionFloatingScore *input,
                            AbError *error) {
    AbFloatingScores *scores = context;
    if (!scores || !input)
        return ab_fail(error, "Missing floating-score storage or descriptor");
    if (!(input->score > 0))
        return true;
    AbSpan incoming_text = input->text_present ? input->text : (AbSpan){0};
    AbSpan sprite =
        input->sprite.kind == AB_DATA_STRING ? input->sprite.string : (AbSpan){0};
    if (scores->used == scores->limits.records ||
        incoming_text.size > scores->limits.text_bytes ||
        sprite.size > scores->limits.text_bytes - incoming_text.size ||
        (incoming_text.size && !incoming_text.data) || (sprite.size && !sprite.data) ||
        !isfinite(input->score) || !isfinite(input->position.x) ||
        !isfinite(input->position.y) || !isfinite(input->time) ||
        !isfinite(input->lifetime) || !isfinite(input->max_scale) ||
        !isfinite(input->xs))
        return ab_fail(error, "Invalid or exhausted floating-score descriptor");
    AbBirdCollisionFloatingScore score = *input;
    uint8_t *text = scores->text + scores->used * scores->limits.text_bytes;
    if (incoming_text.size)
        memmove(text, incoming_text.data, incoming_text.size);
    score.text = (AbSpan){input->text_present ? text : NULL, incoming_text.size};
    if (score.sprite.kind == AB_DATA_STRING) {
        uint8_t *sprite_text = text + incoming_text.size;
        if (sprite.size)
            memmove(sprite_text, sprite.data, sprite.size);
        score.sprite.string.data = sprite_text;
    }
    scores->records[scores->used] = score;
    scores->active[scores->count++] = scores->used++;
    return true;
}

bool ab_floating_scores_add_removal(void *context,
                                    const AbBlockRemovalFloatingScore *input,
                                    AbError *error) {
    if (!context || !input)
        return ab_fail(error, "Missing destruction floating-score binding");
    if (!input->score.present || !(input->score.value > 0))
        return true;
    if (input->x.kind != AB_DATA_NUMBER || input->y.kind != AB_DATA_NUMBER)
        return ab_fail(error, "Nonnumeric destruction floating-score position");
    AbBirdCollisionFloatingScore score = {.text = input->text,
                                          .score = input->score.value,
                                          .time = input->time,
                                          .lifetime = input->lifetime,
                                          .max_scale = input->max_scale,
                                          .xs = input->xs,
                                          .sprite = input->sprite,
                                          .text_present = input->text_present};
    memcpy(&score.position.x, &input->x.bits, sizeof(score.position.x));
    memcpy(&score.position.y, &input->y.bits, sizeof(score.position.y));
    return ab_floating_scores_add(context, &score, error);
}

bool ab_floating_scores_update(AbFloatingScores *scores, float seconds,
                               AbError *error) {
    if (!scores)
        return ab_fail(error, "Invalid floating-score update");
    if (!scores->count)
        return true;
    if (!isfinite(seconds))
        return ab_fail(error, "Invalid floating-score update duration");
    size_t index = 0;
    while (index < scores->count) {
        AbBirdCollisionFloatingScore *score = scores->records + scores->active[index];
        if (!isfinite(score->time) || !isfinite(score->max_scale) ||
            !isfinite(score->lifetime))
            return ab_fail(error, "Nonfinite floating-score state");
        score->time = score->time + seconds;
        if (!isfinite(score->time))
            return ab_fail(error, "Floating-score time overflow");
        if (score->time < 0.25f) {
            score->xs = score->max_scale * score->time;
            score->xs = score->xs / 0.25f;
        } else if (score->time < score->lifetime - 0.25f) {
            score->xs = score->max_scale;
        } else {
            float remaining = score->lifetime - score->time;
            score->xs = score->max_scale * remaining;
            score->xs = score->xs / 0.25f;
        }
        if (!isfinite(score->xs))
            return ab_fail(error, "Floating-score scale overflow");
        if (score->time > score->lifetime) {
            size_t following = scores->count - index - 1;
            if (following)
                memmove(scores->active + index, scores->active + index + 1,
                        following * sizeof(*scores->active));
            --scores->count;
        } else {
            ++index;
        }
    }
    return true;
}

AbBirdCollisionFloatingScore *ab_floating_scores_at(AbFloatingScores *scores,
                                                    size_t index) {
    return scores && index < scores->count ? scores->records + scores->active[index]
                                           : NULL;
}
