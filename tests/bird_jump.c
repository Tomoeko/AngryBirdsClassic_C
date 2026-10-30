#include "ab/bird_jump.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Bird jump failed at line %d: %s\n", __LINE__,             \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    uint32_t state;
    uint32_t events[12][4];
    size_t count;
    bool overshoot;
    size_t overshoot_index;
} JumpRandom;

static uint32_t bits(float value) {
    uint32_t output;
    memcpy(&output, &value, sizeof(output));
    return output;
}

static float from_bits(uint32_t value) {
    float output;
    memcpy(&output, &value, sizeof(output));
    return output;
}

static bool random_integer(void *context, int minimum, int maximum, int *output) {
    JumpRandom *random = context;
    CHECK(random->count < 12 && minimum >= 0 && maximum >= minimum && maximum <= 35);
    random->state ^= random->state << 13;
    random->state ^= random->state >> 17;
    random->state ^= random->state << 5;
    uint32_t value =
        (uint32_t)minimum + random->state % ((uint32_t)(maximum - minimum) + 1);
    if (random->overshoot && random->count == random->overshoot_index) {
        value = (uint32_t)maximum + 1;
    }
    uint32_t *event = random->events[random->count++];
    event[0] = 3;
    event[1] = (uint32_t)minimum;
    event[2] = (uint32_t)maximum;
    event[3] = value;
    *output = (int)value;
    return true;
}

static bool edge_checks(void) {
    AbBirdJumpState state = {.timer = 1};
    AbBirdJumpFrame output = {0}, before = output;
    CHECK(!ab_bird_jump_frame(NULL, 0, false, false, NULL, &output));
    CHECK(!ab_bird_jump_frame(&state, NAN, false, false, NULL, &output));
    CHECK(memcmp(&before, &output, sizeof(output)) == 0);
    state.timer = -1;
    CHECK(!ab_bird_jump_frame(&state, 0, false, false, NULL, &output));
    CHECK(memcmp(&before, &output, sizeof(output)) == 0);
    state.timer = 0;
    CHECK(ab_bird_jump_frame(&state, 0, false, false, NULL, &output));
    CHECK(!output.next.jumping && output.event_count == 0);
    state.timer = -1;
    JumpRandom random = {.state = 7, .overshoot = true};
    AbBirdJumpRandom source = {&random, random_integer};
    CHECK(ab_bird_jump_frame(&state, 0, false, false, &source, &output));
    CHECK(output.next.timer == 3.6f && output.next.jumping);
    return true;
}

static bool read_words(FILE *file, uint32_t *output, size_t count) {
    for (size_t field = 0; field < count; ++field) {
        CHECK(fscanf(file, "%8" SCNx32, output + field) == 1);
    }
    CHECK(fgetc(file) == '\n');
    return true;
}

static bool compare_words(const uint32_t *actual, const uint32_t *expected,
                          size_t count, size_t item, const char *section) {
    for (size_t field = 0; field < count; ++field) {
        if (actual[field] != expected[field]) {
            fprintf(stderr, "Bird jump %zu %s %zu: %08" PRIx32 " != %08" PRIx32 "\n",
                    item, section, field, actual[field], expected[field]);
            return false;
        }
    }
    return true;
}

static bool native_checks(FILE *file) {
    char marker[80];
    CHECK(fgets(marker, sizeof(marker), file));
    bool overshoot = strcmp(marker, "AB_ANDROID_BIRD_JUMP_OVERSHOOT_V1 4096\n") == 0;
    CHECK(overshoot || strcmp(marker, "AB_ANDROID_BIRD_JUMP_V1 4096\n") == 0);
    size_t draws = 0, positions = 0, rotations = 0;
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[13], expected[13], count = 0;
        CHECK(read_words(file, input, 13) && input[10] <= 15);
        CHECK(read_words(file, expected, 13) && read_words(file, &count, 1) &&
              count <= 12);
        AbBirdJumpState state = {.position = {from_bits(input[0]), from_bits(input[1])},
                                 .rotation = from_bits(input[2]),
                                 .timer = from_bits(input[3]),
                                 .height = from_bits(input[4]),
                                 .speed = from_bits(input[5]),
                                 .phase = from_bits(input[6]),
                                 .roll = from_bits(input[7]),
                                 .saved_y = from_bits(input[8]),
                                 .saved_angle = from_bits(input[9]),
                                 .jumping = (input[10] & 1) != 0,
                                 .rebound = (input[10] & 2) != 0};
        JumpRandom random = {.state = input[12],
                             .overshoot = overshoot,
                             .overshoot_index = input[12] & 3};
        AbBirdJumpRandom source = {&random, random_integer};
        AbBirdJumpFrame frame;
        CHECK(ab_bird_jump_frame(&state, from_bits(input[11]), (input[10] & 8) != 0,
                                 (input[10] & 4) != 0, &source, &frame));
        const AbBirdJumpState *next = &frame.next;
        uint32_t actual[] = {bits(next->position.x),
                             bits(next->position.y),
                             bits(next->rotation),
                             bits(next->timer),
                             bits(next->height),
                             bits(next->speed),
                             bits(next->phase),
                             bits(next->roll),
                             bits(next->saved_y),
                             bits(next->saved_angle),
                             next->jumping,
                             next->rebound,
                             random.state};
        CHECK(compare_words(actual, expected, 13, item, "state"));
        CHECK(random.count + frame.event_count == count);
        draws += random.count;
        for (size_t event = 0; event < frame.event_count; ++event) {
            const AbBirdJumpEvent *value = frame.events + event;
            CHECK(random.count < 12);
            uint32_t *record = random.events[random.count++];
            record[0] = (uint32_t)value->kind;
            record[1] = bits(value->value.x);
            record[2] = bits(value->value.y);
            record[3] = 0;
            positions += value->kind == AB_BIRD_JUMP_POSITION;
            rotations += value->kind == AB_BIRD_JUMP_ROTATION;
        }
        for (size_t event = 0; event < count; ++event) {
            uint32_t reference[4];
            CHECK(read_words(file, reference, 4));
            CHECK(compare_words(random.events[event], reference, 4, item, "event"));
        }
    }
    CHECK(fgets(marker, sizeof(marker), file));
    CHECK(strcmp(marker, "AB bird jump reference complete\n") == 0);
    CHECK(fgetc(file) == EOF && !ferror(file));
    printf("All 4096 original idle jump updates match: %zu random draws, %zu "
           "positions and %zu rotations.\n",
           draws, positions, rotations);
    return true;
}

int main(int argc, char **argv) {
    if (argc > 2) {
        fprintf(stderr, "Usage: ab_bird_jump_tests [NATIVE_JUMP_TRACE]\n");
        return 2;
    }
    if (!edge_checks()) {
        return 1;
    }
    if (argc == 1) {
        return 0;
    }
    FILE *file = fopen(argv[1], "rb");
    if (!file) {
        perror(argv[1]);
        return 1;
    }
    bool okay = native_checks(file);
    if (fclose(file) != 0) {
        okay = false;
    }
    return okay ? 0 : 1;
}
