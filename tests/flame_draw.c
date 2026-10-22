#include "ab/flame_effect.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Flame draw failed at %d: %s\n", __LINE__, #condition);    \
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

static bool edge_checks(void) {
    AbFlameState state = {
        .scale = 2, .angle = 0.25f, .angle_present = true, .offset = {1, 2}};
    AbFlameDraw draw;
    CHECK(ab_flame_draw(&state, true, (AbVec2){3, 4}, 2, &draw, NULL) &&
          !draw.remove_callback && !draw.sprite_present && draw.position.x == 5 &&
          draw.position.y == 8 && draw.scale == 4 && draw.angle == 0.25f);
    CHECK(ab_flame_draw(NULL, false, (AbVec2){NAN, NAN}, NAN, &draw, NULL) &&
          draw.remove_callback && !draw.sprite_present && draw.scale == 0);
    CHECK(!ab_flame_draw(&state, true, (AbVec2){0}, 1, NULL, NULL));
    AbFlameDraw saved;
    memcpy(&saved, &draw, sizeof(saved));
    CHECK(!ab_flame_draw(NULL, true, (AbVec2){0}, 1, &draw, NULL));
    state.angle_present = false;
    CHECK(!ab_flame_draw(&state, true, (AbVec2){0}, 1, &draw, NULL));
    state.angle_present = true;
    state.new_angle = 1025;
    CHECK(!ab_flame_draw(&state, true, (AbVec2){0}, 1, &draw, NULL));
    CHECK(memcmp(&saved, &draw, sizeof(draw)) == 0);
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
          strcmp(header, "AB_ANDROID_FLAME_DRAW_V1 4096\n") == 0);
    const char *const names[] = {"frame1", "frame2", "frame3", "frame4",
                                 "frame5", "frame6", "frame7", "frame8"};
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[11], expected[9];
        CHECK(read_words(file, input, 11) && read_words(file, expected, 9) &&
              input[0] <= 3 && input[9] <= 8);
        bool present = input[0] >= 2;
        AbFlameState state = {.offset = {from_bits(input[4]), from_bits(input[5])},
                              .new_angle = from_bits(input[6]),
                              .scale = from_bits(input[7]),
                              .angle = from_bits(input[8]),
                              .angle_present = true,
                              .sprite_present = input[9] != 0};
        if (state.sprite_present) {
            const char *name = names[input[9] - 1];
            state.sprite = (AbSpan){(const uint8_t *)name, strlen(name)};
        }
        AbFlameDraw draw;
        AbError error = {0};
        if (!ab_flame_draw(&state, present,
                           (AbVec2){from_bits(input[2]), from_bits(input[3])},
                           from_bits(input[1]), &draw, &error)) {
            fprintf(stderr, "Native flame draw %zu failed: %s\n", item, error.message);
            return false;
        }
        CHECK(draw.remove_callback == !present &&
              draw.sprite_present == (present && state.sprite_present));
        CHECK(!draw.sprite_present ||
              (draw.sprite.size == state.sprite.size &&
               memcmp(draw.sprite.data, state.sprite.data, state.sprite.size) == 0));
        AbRandom48 random;
        uint32_t next;
        CHECK(ab_random48_seed(&random, input[10]) && ab_random48_next(&random, &next));
        uint32_t actual[] = {present,
                             present,
                             present,
                             present ? input[9] : 0,
                             to_bits(draw.position.x),
                             to_bits(draw.position.y),
                             to_bits(draw.scale),
                             to_bits(draw.angle),
                             next};
        for (size_t field = 0; field < 9; ++field) {
            if (actual[field] != expected[field]) {
                fprintf(stderr,
                        "Native flame draw %zu field %zu: %08" PRIx32
                        " instead of %08" PRIx32 "\n",
                        item, field, actual[field], expected[field]);
                return false;
            }
        }
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB flame draw reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    puts("All 4096 native flame draw, callback removal and RNG results match");
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
    puts("Flame draw position, scale, separate angle and removal checks pass");
    return 0;
}
