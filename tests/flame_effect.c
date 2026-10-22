#include "ab/flame_effect.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Flame effect failed at %d: %s\n", __LINE__, #condition);  \
            return false;                                                              \
        }                                                                              \
    } while (0)

static float from_bits(uint32_t bits) {
    float result;
    memcpy(&result, &bits, sizeof(result));
    return result;
}

static uint32_t to_bits(float number) {
    uint32_t result;
    memcpy(&result, &number, sizeof(result));
    return result;
}

static AbSpan span(const char *text) {
    return (AbSpan){(const uint8_t *)text, strlen(text)};
}

static bool edge_checks(void) {
    AbSpan sprites[] = {span("frame1"), span("frame2"), span("frame3")};
    AbFlameOffset special = {span("base"), {3, 4}};
    AbFlameDefinition definition = {.sprites = sprites,
                                    .count = 3,
                                    .offsets = &special,
                                    .offset_count = 1,
                                    .default_offset = {1, 2},
                                    .default_present = true};
    AbFlameState state = {.timer = 1,
                          .timer_speed = 1,
                          .scale = NAN,
                          .angle = NAN,
                          .old_angle = NAN,
                          .new_angle = NAN};
    AbFlameStep step = {.body_angle = 0.25f,
                        .object_sprite = span("base"),
                        .object_sprite_present = true};
    AbRandom48 random, expected;
    CHECK(ab_random48_seed(&random, 17));
    expected = random;
    float ignored;
    CHECK(ab_script_random(&expected, 0, 0, 0, &ignored) == AB_SCRIPT_RANDOM_OK);
    CHECK(ab_flame_update(&state, &definition, &step, &random, NULL));
    CHECK(state.timer == 0 && state.sprite_present && state.sprite.size == 6 &&
          memcmp(state.sprite.data, "frame1", 6) == 0 && state.angle_present &&
          state.angle == 0 && to_bits(state.old_angle) == 0 &&
          state.new_angle == 0.25f && isnan(state.scale) && state.offset.x == 3 &&
          state.offset.y == 4 && random.value == expected.value);
    state.timer = 2;
    step.object_sprite = span("other");
    CHECK(ab_flame_update(&state, &definition, &step, &random, NULL) &&
          state.timer == 1 && !state.sprite_present && state.offset.x == 1 &&
          state.offset.y == 2);
    definition.default_present = false;
    CHECK(ab_flame_update(&state, &definition, &step, &random, NULL) &&
          state.offset.x == 0 && state.offset.y == 0);
    AbFlameState saved;
    memcpy(&saved, &state, sizeof(saved));
    uint64_t before = random.value;
    CHECK(!ab_flame_update(NULL, &definition, &step, &random, NULL));
    CHECK(!ab_flame_update(&state, NULL, &step, &random, NULL));
    CHECK(!ab_flame_update(&state, &definition, NULL, &random, NULL));
    CHECK(!ab_flame_update(&state, &definition, &step, NULL, NULL) &&
          memcmp(&saved, &state, sizeof(state)) == 0 && random.value == before);
    step.seconds = NAN;
    CHECK(!ab_flame_update(&state, &definition, &step, &random, NULL) &&
          memcmp(&saved, &state, sizeof(state)) == 0 && random.value == before);
    step.seconds = 0;
    special.sprite = (AbSpan){NULL, 1};
    expected = random;
    CHECK(ab_script_random(&expected, 0, 0, 0, &ignored) == AB_SCRIPT_RANDOM_OK);
    CHECK(!ab_flame_update(&state, &definition, &step, &random, NULL) &&
          memcmp(&saved, &state, sizeof(state)) == 0 && random.value == expected.value);
    definition = (AbFlameDefinition){0};
    CHECK(ab_flame_update(&state, &definition, &step, &random, NULL) &&
          !state.sprite_present && state.angle_present);
    return true;
}

static bool read_words(FILE *file, uint32_t *words, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, words + index) == 1);
    }
    CHECK(fgetc(file) == '\n');
    return true;
}

static bool native_checks(FILE *file) {
    char header[80];
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB_ANDROID_FLAME_EFFECT_V1 4096\n") == 0);
    const char *const names[] = {"frame1", "frame2", "frame3", "frame4",
                                 "frame5", "frame6", "frame7", "frame8"};
    AbSpan sprites[8];
    for (size_t index = 0; index < 8; ++index) {
        sprites[index] = span(names[index]);
    }
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[20], expected[10];
        CHECK(read_words(file, input, 20) && read_words(file, expected, 10) &&
              input[4] <= 2 && input[8] <= 8 && input[10] <= 2 && input[11] <= 3 &&
              input[19] < 8);
        AbFlameOffset special = {span("base"),
                                 {from_bits(input[12]), from_bits(input[13])}};
        AbFlameDefinition definition = {
            .sprites = sprites,
            .count = input[8],
            .offsets = &special,
            .offset_count = input[11] & 1,
            .default_offset = {from_bits(input[14]), from_bits(input[15])},
            .default_present = (input[11] & 2) != 0};
        AbFlameState state = {.timer = from_bits(input[1]),
                              .timer_speed = from_bits(input[2]),
                              .scale = from_bits(input[16]),
                              .angle = from_bits(input[3]),
                              .old_angle = from_bits(input[17]),
                              .new_angle = from_bits(input[18]),
                              .sprite = sprites[input[19]],
                              .sprite_present = true,
                              .angle_present = input[4] == 2};
        AbFlameStep step = {.seconds = from_bits(input[0]),
                            .body_angle = from_bits(input[5]),
                            .velocity = {from_bits(input[6]), from_bits(input[7])},
                            .object_sprite = span(input[10] == 1 ? "base" : "other"),
                            .object_sprite_present = input[10] != 0};
        AbRandom48 random;
        CHECK(ab_random48_seed(&random, input[9]));
        AbError error = {0};
        if (!ab_flame_update(&state, &definition, &step, &random, &error)) {
            fprintf(stderr, "Native flame case %zu failed: %s\n", item, error.message);
            return false;
        }
        CHECK(state.angle_present);
        uint32_t sprite = 0, next;
        if (state.sprite_present) {
            for (size_t index = 0; index < 8; ++index) {
                if (state.sprite.size == sprites[index].size &&
                    memcmp(state.sprite.data, sprites[index].data, state.sprite.size) ==
                        0) {
                    sprite = (uint32_t)index + 1;
                }
            }
            CHECK(sprite);
        }
        CHECK(ab_random48_next(&random, &next));
        uint32_t actual[] = {to_bits(state.timer),       sprite,
                             to_bits(state.new_angle),   to_bits(state.old_angle),
                             to_bits(state.angle),       to_bits(state.offset.x),
                             to_bits(state.offset.y),    to_bits(state.scale),
                             to_bits(state.timer_speed), next};
        for (size_t field = 0; field < 10; ++field) {
            if (actual[field] != expected[field]) {
                fprintf(stderr,
                        "Native flame case %zu field %zu: %08" PRIx32
                        " instead of %08" PRIx32 "\n",
                        item, field, actual[field], expected[field]);
                return false;
            }
        }
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB flame effect reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    puts("All 4096 native flame timer, sprite, angle, offset and RNG results match");
    return true;
}

int main(int argc, char **argv) {
    if (argc > 2 || !edge_checks()) {
        return 1;
    }
    if (argc == 2) {
        FILE *file = fopen(argv[1], "rb");
        bool okay = file && native_checks(file);
        if (file && fclose(file) != 0) {
            okay = false;
        }
        return okay ? 0 : 1;
    }
    puts("Flame carry, sprite bounds, offset fallback, RNG and failure state pass");
    return 0;
}
