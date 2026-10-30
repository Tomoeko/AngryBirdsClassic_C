#include "ab/bird_shockwave.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Shockwave failed at %d: %s\n", __LINE__, #condition);     \
            return false;                                                              \
        }                                                                              \
    } while (0)

static AbDataValue text(const char *name) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)name, strlen(name)}};
}

static AbDataValue number(float value) {
    AbDataValue output = {.kind = AB_DATA_NUMBER};
    memcpy(&output.bits, &value, sizeof(value));
    return output;
}

static bool phases(void) {
    const char *names[] = {"duration",      "delay",
                           "radius",        "force",
                           "minimumForce",  "forceDropOffPoint",
                           "damage",        "minimumDamage",
                           "initialRadius", "damageBasedOnMaxStrength"};
    float values[] = {2, 0.5f, 10, 100, 10, 0.25f, 0.5f, 0.25f, 2, 0};
    AbDataEntry entries[10];
    for (size_t index = 0; index < 10; ++index)
        entries[index] = (AbDataEntry){text(names[index]), number(values[index])};
    AbDataTable definition = {entries, 10, 10, NULL};
    AbBirdShockwave wave;
    CHECK(ab_bird_shockwave_begin(&definition, &wave, NULL));
    CHECK(wave.time == 2 && wave.delay_timer == 0.5f && wave.delay_present &&
          wave.definition.damage_from_definition);
    AbBirdShockwavePhase phase;
    CHECK(ab_bird_shockwave_advance(&wave, 0.5f, &phase, NULL));
    CHECK(phase.delaying && !phase.speed_written && !wave.delay_present &&
          wave.time == 2 && !phase.finished);
    CHECK(ab_bird_shockwave_advance(&wave, 0, &phase, NULL));
    CHECK(!phase.delaying && phase.speed_written && phase.physics_speed == 0.6f &&
          fabsf(wave.current_radius - 2) < 0.00001f && wave.force == 100 &&
          phase.shake_present && phase.time_ratio == 1);
    wave.time = 0.5f;
    CHECK(ab_bird_shockwave_advance(&wave, 0.5f, &phase, NULL));
    CHECK(phase.finished && phase.physics_speed == 1 && !phase.shake_present &&
          wave.time == 0 && fabsf(wave.current_radius - 9.391036f) < 0.00001f &&
          fabsf(wave.force - 40) < 0.00001f);
    AbBirdShockwave saved = wave;
    CHECK(!ab_bird_shockwave_advance(&wave, NAN, &phase, NULL) &&
          !memcmp(&saved, &wave, sizeof(wave)));
    entries[0].value = number(0);
    CHECK(!ab_bird_shockwave_begin(&definition, &wave, NULL));
    entries[0].value = number(2);
    entries[5].value = number(1);
    CHECK(!ab_bird_shockwave_begin(&definition, &wave, NULL));
    return true;
}

static bool impacts(void) {
    AbBirdShockwave wave = {.definition = {.force = 100,
                                           .damage = 0.5f,
                                           .minimum_damage = 0.25f,
                                           .damage_from_definition = true},
                            .current_radius = 10,
                            .progress = 0.5f,
                            .force = 40};
    AbBirdShockwaveObject object = {.relative = {2, 0},
                                    .strength = 50,
                                    .definition_strength = 100,
                                    .definition_defence = 1,
                                    .live = true,
                                    .definition_strength_present = true};
    AbBirdShockwaveImpact impact;
    CHECK(ab_bird_shockwave_impact(&wave, &object, &impact, NULL));
    CHECK(impact.inside && impact.break_joints && impact.mark_damaged &&
          impact.inflict && impact.damage == 36.5f && impact.score == 360 &&
          impact.force && !impact.impulse && impact.vector.x == 80);
    object.same_wave = true;
    CHECK(ab_bird_shockwave_impact(&wave, &object, &impact, NULL) && !impact.inflict &&
          !impact.mark_damaged && impact.force);
    wave.definition.impulse = true;
    CHECK(ab_bird_shockwave_impact(&wave, &object, &impact, NULL) && !impact.force &&
          !impact.impulse);
    object.same_wave = false;
    CHECK(ab_bird_shockwave_impact(&wave, &object, &impact, NULL) && impact.impulse);
    object.bird = true;
    object.relative.x = NAN;
    CHECK(ab_bird_shockwave_impact(&wave, &object, &impact, NULL) && !impact.inside);
    object.shot = true;
    object.relative.x = 2;
    CHECK(ab_bird_shockwave_impact(&wave, &object, &impact, NULL) && impact.inside &&
          !impact.impulse && !impact.inflict);
    wave.definition.impulse = false;
    CHECK(ab_bird_shockwave_impact(&wave, &object, &impact, NULL) && impact.force);
    object.relative.x = 10;
    CHECK(ab_bird_shockwave_impact(&wave, &object, &impact, NULL) && !impact.inside);
    object.relative.x = nextafterf(10, 0);
    CHECK(ab_bird_shockwave_impact(&wave, &object, &impact, NULL) && impact.inside);
    object.bird = false;
    object.definition_strength_present = false;
    object.relative.x = 2;
    CHECK(ab_bird_shockwave_impact(&wave, &object, &impact, NULL) &&
          impact.damage == 17.75f && impact.score == 170);
    object.strength = NAN;
    CHECK(!ab_bird_shockwave_impact(&wave, &object, &impact, NULL));
    object.live = false;
    CHECK(ab_bird_shockwave_impact(&wave, &object, &impact, NULL) &&
          impact.mark_damaged && !impact.inflict && impact.force);
    return true;
}

int main(void) {
    return phases() && impacts() ? 0 : 1;
}
