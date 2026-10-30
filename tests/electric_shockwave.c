#include "ab/electric_shockwave.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(expression)                                                              \
    do {                                                                               \
        if (!(expression)) {                                                           \
            fprintf(stderr, "%s:%d: %s (%s)\n", __FILE__, __LINE__, #expression,       \
                    error.message);                                                    \
            return false;                                                              \
        }                                                                              \
    } while (0)

static bool effects(void) {
    AbError error = {0};
    AbElectricShockwave effect;
    AbRandom48 random, expected;
    CHECK(ab_random48_seed(&random, 42));
    expected = random;
    CHECK(ab_electric_shockwave_init(&effect, &random, &error));
    for (size_t sample = 0; sample < 10; ++sample) {
        uint32_t value;
        CHECK(ab_random48_next(&expected, &value));
    }
    CHECK(random.value == expected.value && effect.bird_visible);
    CHECK(effect.bird_index == 0 && effect.spark_count == 0);
    for (size_t channel = 0; channel < 6; ++channel)
        CHECK(effect.values[channel] == 0 && effect.velocities[channel] == 0);

    AbBirdShockwave wave = {.definition = {.delay = .4f},
                            .delay_timer = .4f,
                            .time = 1.9f,
                            .delay_present = true};
    bool puff = true;
    CHECK(ab_electric_shockwave_update(&effect, &wave, 1, 1.0f / 30, &random, &puff,
                                       &error));
    CHECK(!puff && effect.spark_count == 10);
    for (size_t sample = 0; sample < 50; ++sample) {
        uint32_t value;
        CHECK(ab_random48_next(&expected, &value));
    }
    CHECK(random.value == expected.value);
    AbElectricSpriteDraw draws[11];
    size_t count = SIZE_MAX;
    CHECK(ab_electric_shockwave_draw(&effect, &wave, (AbVec2){15, 3}, 1, 2, .5f,
                                     &random, draws, 11, &count, &error));
    CHECK(count == 11 && draws[0].sprite == AB_ELECTRIC_CHARGING_BIRD);
    CHECK(draws[0].scale == 2 && draws[0].angle == .5f);
    CHECK(random.value == expected.value);
    for (size_t index = 1; index < count; ++index) {
        CHECK(draws[index].sprite == AB_ELECTRIC_SPARK);
        CHECK(draws[index].alpha >= .25f && draws[index].alpha <= 1);
        CHECK(draws[index].scale >= -.8f && draws[index].scale <= 1.2f);
    }
    wave.delay_present = false;
    wave.progress = .5f;
    CHECK(ab_electric_shockwave_update(&effect, &wave, 1, 1.0f / 30, &random, &puff,
                                       &error));
    CHECK(!puff && effect.bird_visible);
    CHECK(ab_electric_shockwave_draw(&effect, &wave, (AbVec2){15, 3}, 1, 2, .5f,
                                     &random, draws, 11, &count, &error));
    CHECK(count == 7 && effect.bird_index == 1);
    CHECK(draws[0].sprite == AB_ELECTRIC_OUTER && draws[0].scale == 1.4f);
    CHECK(draws[1].sprite == AB_ELECTRIC_INNER && draws[6].sprite == AB_ELECTRIC_BIRD);
    for (size_t index = 2; index < 6; ++index)
        CHECK(draws[index].sprite == AB_ELECTRIC_RAY);
    uint64_t before = random.value;
    CHECK(ab_electric_shockwave_draw(&effect, &wave, (AbVec2){15, 3}, 1, 2, .5f,
                                     &random, draws, 11, &count, &error));
    CHECK(random.value == before && effect.bird_index == 1);
    wave.progress = .95f;
    CHECK(ab_electric_shockwave_update(&effect, &wave, 1, 1.0f / 30, &random, &puff,
                                       &error));
    CHECK(puff && !effect.bird_visible);
    CHECK(ab_electric_shockwave_update(&effect, &wave, 1, 1.0f / 30, &random, &puff,
                                       &error));
    CHECK(!puff);
    CHECK(ab_electric_shockwave_draw(&effect, &wave, (AbVec2){15, 3}, 1, 2, .5f,
                                     &random, draws, 11, &count, &error));
    CHECK(count == 6);
    wave.time = 0;
    before = random.value;
    CHECK(ab_electric_shockwave_update(&effect, &wave, 1, 1, &random, &puff, &error));
    CHECK(!puff && random.value == before);
    CHECK(!ab_electric_shockwave_draw(&effect, &wave, (AbVec2){0}, 1, 1, 0, &random,
                                      draws, 10, &count, &error));
    CHECK(
        !ab_electric_shockwave_update(&effect, &wave, 1, NAN, &random, &puff, &error));
    for (unsigned sprite = 0; sprite < AB_ELECTRIC_SPRITES; ++sprite)
        CHECK(ab_electric_shockwave_sprite((AbElectricSprite)sprite).size != 0);
    CHECK(!ab_electric_shockwave_sprite(AB_ELECTRIC_SPRITES).size);
    return true;
}

int main(void) {
    return effects() ? 0 : 1;
}
