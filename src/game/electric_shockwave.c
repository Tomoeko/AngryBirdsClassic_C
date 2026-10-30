#include "ab/electric_shockwave.h"
#include "ab/script_math.h"

#include <math.h>
#include <string.h>

static const float script_pi = 0x1.921fb6p+1f;

AbSpan ab_electric_shockwave_sprite(AbElectricSprite sprite) {
    static const char *const names[] = {
        "LIGHTNING_EXPLOSION_0", "LIGHTNING_EXPLOSION_1", "LIGHTNING_EXPLOSION_2",
        "LIGHTNING_EXPLOSION_3", "BIRD_GREY_BLUE",        "BIRD_GREY_BLUE_2",
        "BOMB_BIRD_SHOCK_6"};
    return (unsigned)sprite < AB_ELECTRIC_SPRITES
               ? (AbSpan){(const uint8_t *)names[sprite], strlen(names[sprite])}
               : (AbSpan){0};
}

static bool random_value(AbRandom48 *random, unsigned arguments, int32_t low,
                         int32_t high, float *value, AbError *error) {
    return ab_script_random(random, arguments, low, high, value) ==
               AB_SCRIPT_RANDOM_OK ||
           ab_fail(error, "Invalid ElectricShockwave shared RNG");
}

/* libAngryBirdsClassic.so math.rad at ARM VA 0x81ff48 promotes the Lua float,
 * multiplies by this binary64 literal, then rounds back to binary32. */
static float radians(float degrees) {
    return (float)((double)degrees * 0x1.1df46a2529d39p-6);
}

static float clamp_angle(float angle) {
    float period = script_pi * 2;
    float result = (float)fmod((double)angle, (double)period);
    while (result < 0)
        result += period;
    return result;
}

static bool update_jitter(AbElectricShockwave *effect, const AbBirdShockwave *wave,
                          float seconds, AbRandom48 *random, bool *puff,
                          AbError *error) {
    const int32_t limits[] = {10, 2, 3, 2, 4, 4};
    const float multipliers[] = {30, 90, 90, 90, 90, 90};
    for (size_t index = 0; index < 6; ++index) {
        float value;
        if (!random_value(random, 2, -limits[index], limits[index], &value, error))
            return false;
        effect->targets[index] = value * multipliers[index];
    }
    effect->outer_timer -= seconds;
    effect->inner_timer -= seconds;
    effect->lightning_timer -= seconds;
    float responsiveness = fminf(1, 5 * seconds);
    for (size_t index = 0; index < 6; ++index) {
        float change =
            (effect->targets[index] - effect->values[index]) * responsiveness;
        effect->velocities[index] += change;
        effect->values[index] += effect->velocities[index] * seconds;
    }
    float values[4];
    for (size_t index = 0; index < 4; ++index)
        if (!random_value(random, index == 3 ? 2 : 0, 0, 10, values + index, error))
            return false;
    effect->offset_x = (values[0] - .5f) * 1.1f;
    effect->offset_y = (values[1] - .5f) * 1.1f;
    effect->scale_offset = (values[2] - .5f) * .15f;
    effect->alpha_offset = values[3] * .05f;
    *puff = wave->progress > .9f && effect->bird_visible;
    if (*puff)
        effect->bird_visible = false;
    return true;
}

bool ab_electric_shockwave_init(AbElectricShockwave *effect, AbRandom48 *random,
                                AbError *error) {
    if (!effect)
        return ab_fail(error, "Missing ElectricShockwave state");
    *effect = (AbElectricShockwave){.bird_visible = true};
    bool puff;
    return update_jitter(effect, &(AbBirdShockwave){0}, 0, random, &puff, error);
}

static bool update_sparks(AbElectricShockwave *effect, const AbBirdShockwave *wave,
                          float visual_scale, AbRandom48 *random, AbError *error) {
    float ratio = fmaxf(0, wave->delay_timer / wave->definition.delay);
    float progress = 1 - ratio;
    effect->spark_count = 0;
    for (size_t index = 0; index < 10; ++index) {
        float angle = (2 * script_pi) * (float)(index + 1);
        angle /= 10;
        float cosine, sine;
        if (!ab_script_cosine(angle, &cosine) || !ab_script_sine(angle, &sine))
            return ab_fail(error, "ElectricShockwave spark angle is out of range");
        float values[5];
        for (size_t sample = 0; sample < 5; ++sample)
            if (!random_value(random, 0, 0, 0, values + sample, error))
                return false;
        float radius = (1 - progress) * 10;
        float x = cosine * radius + (sine * (values[0] - .5f)) * 10;
        float y = sine * radius - (cosine * (values[1] - .5f)) * 10;
        effect->sparks[index] =
            (AbElectricSpriteDraw){.sprite = AB_ELECTRIC_SPARK,
                                   .position = {x, y},
                                   .scale = .1f * visual_scale + (values[2] - .5f),
                                   .angle = values[3] * script_pi,
                                   .alpha = 1 - values[4] * .75f};
        ++effect->spark_count;
    }
    return true;
}

bool ab_electric_shockwave_update(AbElectricShockwave *effect,
                                  const AbBirdShockwave *wave, float visual_scale,
                                  float seconds, AbRandom48 *random, bool *puff,
                                  AbError *error) {
    if (!effect || !wave || !puff || !isfinite(seconds) || seconds < 0 ||
        !isfinite(visual_scale))
        return ab_fail(error, "Invalid ElectricShockwave update");
    *puff = false;
    if (wave->delay_present)
        return wave->definition.delay > 0
                   ? update_sparks(effect, wave, visual_scale, random, error)
                   : ab_fail(error, "ElectricShockwave has a zero charge duration");
    return wave->time <= 0 || update_jitter(effect, wave, seconds, random, puff, error);
}

static void append(AbElectricSpriteDraw *draws, size_t *count, AbElectricSprite sprite,
                   AbVec2 position, float scale, float angle, float alpha) {
    draws[(*count)++] = (AbElectricSpriteDraw){sprite, position, scale, angle, alpha};
}

static bool reset_angle(AbRandom48 *random, float *timer, float factor, float base,
                        int32_t low, int32_t high, float step, float *angle,
                        AbError *error) {
    if (*timer > 0)
        return true;
    float value;
    if (!random_value(random, 0, 0, 0, &value, error))
        return false;
    *timer = value * factor + base;
    if (!random_value(random, 2, low, high, &value, error))
        return false;
    *angle = value * step;
    return true;
}

bool ab_electric_shockwave_draw(AbElectricShockwave *effect,
                                const AbBirdShockwave *wave, AbVec2 position,
                                float visual_scale, float bird_scale, float bird_angle,
                                AbRandom48 *random, AbElectricSpriteDraw *draws,
                                size_t capacity, size_t *count, AbError *error) {
    if (!effect || !wave || !draws || !count || capacity < 11 ||
        !isfinite(visual_scale) || !isfinite(bird_scale) || !isfinite(bird_angle) ||
        !isfinite(wave->progress) || wave->progress < -1 || wave->progress > 1 ||
        effect->bird_index >= 6 || effect->spark_count > 10)
        return ab_fail(error, "Invalid ElectricShockwave draw state/storage");
    *count = 0;
    if (wave->delay_present) {
        append(draws, count, AB_ELECTRIC_CHARGING_BIRD, position, bird_scale,
               bird_angle, 1);
        for (size_t index = 0; index < effect->spark_count; ++index) {
            AbElectricSpriteDraw spark = effect->sparks[index];
            spark.position.x += position.x;
            spark.position.y += position.y;
            spark.scale *= 2;
            draws[(*count)++] = spark;
        }
        return true;
    }
    float progress = wave->progress;
    float sine;
    if (!ab_script_sine(progress * script_pi, &sine))
        return ab_fail(error, "ElectricShockwave draw angle is out of range");
    /* The source also computes 1-sin(t*pi), but does not read that value. */
    float alpha = progress > .6f ? 1 - (progress - .6f) / (1 - .6f) : 1;
    if (!reset_angle(random, &effect->outer_timer, .075f, .025f, 0, 7, 45,
                     &effect->outer_angle, error))
        return false;
    append(draws, count, AB_ELECTRIC_OUTER, position, (progress * 2.8f) * visual_scale,
           radians(effect->outer_angle + effect->values[0]), alpha);
    float inner = progress * progress + .1f;
    if (!reset_angle(random, &effect->inner_timer, .1f, .01f, -2, 2, 90,
                     &effect->inner_angle, error))
        return false;
    AbVec2 offset = {position.x + effect->offset_x, position.y + effect->offset_y};
    append(draws, count, AB_ELECTRIC_INNER, offset,
           (inner * 2) * visual_scale + effect->scale_offset,
           radians(effect->inner_angle + effect->values[1]), 1 - inner);
    bool change_bird = effect->lightning_timer <= 0;
    if (!reset_angle(random, &effect->lightning_timer, .1f, .01f, -4, 4, 45,
                     &effect->lightning_angle, error))
        return false;
    if (change_bird)
        effect->bird_index = (effect->bird_index + 1) % 6;
    float angles[4];
    for (size_t index = 0; index < 4; ++index) {
        float degrees = effect->lightning_angle + effect->values[index + 2];
        if (index)
            degrees += (float)index * 45;
        angles[index] = clamp_angle(radians(degrees));
        /* Each added quarter-turn is deliberately not wrapped again. */
        bool close;
        do {
            close = false;
            for (size_t prior = 0; prior < index; ++prior)
                close |= fabsf(angles[index] - angles[prior]) < script_pi * .5f;
            if (close)
                angles[index] += script_pi * .5f;
        } while (close && index > 1);
    }
    float ray_alpha = progress < .8f ? alpha + effect->alpha_offset : alpha;
    const float scales[] = {(progress * 1.8f) * visual_scale + effect->scale_offset,
                            (inner * 2) * visual_scale + effect->scale_offset * 5,
                            (progress * 1.5f) * visual_scale + effect->scale_offset * 5,
                            (progress * 1.7f) * visual_scale + effect->scale_offset};
    const float alphas[] = {ray_alpha * .4f, ray_alpha * .6f, (1 - inner) * .8f,
                            ray_alpha * .4f};
    const size_t order[] = {1, 0, 2, 3};
    for (size_t index = 0; index < 4; ++index)
        append(draws, count, AB_ELECTRIC_RAY, offset, scales[index],
               angles[order[index]], alphas[index]);
    if (effect->bird_visible) {
        static const AbElectricSprite birds[] = {
            AB_ELECTRIC_BIRD, AB_ELECTRIC_BIRD,         AB_ELECTRIC_SHOCKED_BIRD,
            AB_ELECTRIC_BIRD, AB_ELECTRIC_SHOCKED_BIRD, AB_ELECTRIC_BIRD};
        AbElectricSprite bird =
            progress > .98f ? AB_ELECTRIC_BIRD : birds[effect->bird_index];
        append(draws, count, bird, position, bird_scale, bird_angle, 1);
    }
    return true;
}
