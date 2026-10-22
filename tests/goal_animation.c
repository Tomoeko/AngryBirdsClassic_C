#include "ab/goal_animation.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Goal animation failed at %d: %s\n", __LINE__,             \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static uint32_t raw(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static float number(uint32_t bits) {
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

typedef struct {
    AbGoalAnimation *state;
    uint32_t input[12];
    uint32_t random;
    uint32_t events[16][10];
    size_t count, draws;
    bool eligible;
} NativeGoal;

static void snapshot(const AbGoalAnimation *state, uint32_t words[6]) {
    words[0] = raw(state->special_timer);
    words[1] = raw(state->blink_timer);
    words[2] = raw(state->oink_timer);
    words[3] = state->special_expression;
    words[4] = state->blink_on;
    words[5] = raw(state->strength);
}

static uint32_t *record(NativeGoal *test, uint32_t kind) {
    if (test->count >= 16) {
        return NULL;
    }
    uint32_t *event = test->events[test->count++];
    event[0] = kind;
    snapshot(test->state, event + 4);
    return event;
}

static void mutate(NativeGoal *test) {
    test->state->special_timer = number(test->input[11]);
    test->state->blink_timer = number(test->input[11]);
    test->state->oink_timer = number(test->input[11]);
    test->state->special_expression = !(test->input[8] & 1);
    test->state->blink_on = !(test->input[8] & (2 | 256));
    if (test->input[10] & 16) {
        test->eligible = false;
    }
}

static bool draw(void *context, int minimum, int maximum, int *output) {
    NativeGoal *test = context;
    CHECK(minimum >= 1 && maximum >= minimum && maximum <= 60);
    test->random ^= test->random << 13;
    test->random ^= test->random >> 17;
    test->random ^= test->random << 5;
    uint32_t value =
        (uint32_t)minimum + test->random % ((uint32_t)(maximum - minimum) + 1);
    if ((test->input[8] & 64) && !test->draws) {
        value = (uint32_t)maximum + 1;
    }
    ++test->draws;
    uint32_t *event = record(test, 1);
    CHECK(event);
    event[1] = (uint32_t)minimum;
    event[2] = (uint32_t)maximum;
    event[3] = value;
    *output = (int)value;
    return true;
}

static bool lost(void *context, bool *output, AbError *error) {
    (void)error;
    NativeGoal *test = context;
    CHECK(record(test, 2));
    if (test->input[10] & 1) {
        mutate(test);
    }
    *output = (test->input[8] & (8 | 128)) != 0;
    return true;
}

static bool sprite(void *context, AbGoalFace face, AbError *error) {
    (void)error;
    NativeGoal *test = context;
    uint32_t *event = record(test, 3);
    CHECK(event);
    event[1] = (uint32_t)face;
    if (test->input[10] & 2) {
        mutate(test);
    }
    return true;
}

static bool distance(void *context, float *output, AbError *error) {
    (void)error;
    NativeGoal *test = context;
    uint32_t *event = record(test, 4);
    CHECK(event);
    event[1] = test->input[7];
    if (test->input[10] & 4) {
        mutate(test);
    }
    *output = number(test->input[7]);
    return true;
}

static bool audio(void *context, float volume, AbError *error) {
    (void)error;
    NativeGoal *test = context;
    CHECK(record(test, 5));
    uint32_t *event = record(test, 6);
    CHECK(event);
    event[1] = raw(volume);
    if (test->input[10] & 8) {
        mutate(test);
    }
    return true;
}

static bool words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, output + index) == 1);
        CHECK(fgetc(file) == (index + 1 == count ? '\n' : ' '));
    }
    return true;
}

static bool compare(const uint32_t *actual, const uint32_t *expected, size_t count,
                    size_t item, const char *phase) {
    for (size_t index = 0; index < count; ++index) {
        if (actual[index] != expected[index]) {
            fprintf(stderr,
                    "Goal case %zu %s word %zu: %08" PRIx32 " != %08" PRIx32 "\n", item,
                    phase, index, actual[index], expected[index]);
            return false;
        }
    }
    return true;
}

static bool native_checks(FILE *file) {
    char header[80];
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB_ANDROID_GOAL_ANIMATION_V1 4096\n") == 0);
    size_t totals[6] = {0};
    for (size_t item = 0; item < 4096; ++item) {
        NativeGoal test = {0};
        uint32_t expected[8];
        CHECK(words(file, test.input, 12) && words(file, expected, 8) &&
              expected[7] <= 16);
        uint32_t flags = test.input[8];
        AbGoalAnimation state = {
            .strength = number(test.input[0]),
            .special_timer = number(test.input[3]),
            .blink_timer = number(test.input[4]),
            .oink_timer = number(test.input[5]),
            .special_expression = (flags & 1) != 0,
            .blink_on = (flags & (2 | 256)) != 0,
        };
        test.state = &state;
        test.random = test.input[9];
        test.eligible = !(flags & (16 | 32));
        AbGoalAnimationControls controls = {
            .context = &test,
            .random_integer = draw,
            .lose_condition = lost,
            .set_sprite = sprite,
            .camera_distance = distance,
            .play_oink = audio,
            .base_strength = number(test.input[1]),
            .damage1_min = number(test.input[2]),
            .disable_smiling = (flags & 4) != 0,
        };
        AbError error = {0};
        float seconds = number(test.input[6]);
        CHECK(ab_goal_expression_update(&state, seconds, test.eligible, &controls,
                                        &error));
        CHECK(ab_goal_oink_update(&state, seconds, test.eligible, &controls, &error));
        uint32_t actual[8];
        snapshot(&state, actual);
        actual[6] = test.random;
        actual[7] = (uint32_t)test.count;
        CHECK(compare(actual, expected, 8, item, "state"));
        for (size_t event = 0; event < test.count; ++event) {
            uint32_t expected_event[10];
            CHECK(words(file, expected_event, 10) &&
                  compare(test.events[event], expected_event, 10, item, "callback") &&
                  expected_event[0] >= 1 && expected_event[0] <= 6);
            ++totals[expected_event[0] - 1];
        }
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB goal animation reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("All 4096 native goal animation states and ordered callbacks match: "
           "%zu draws, %zu loss queries, %zu sprites, %zu distances, %zu sounds\n",
           totals[0], totals[1], totals[2], totals[3], totals[5]);
    return true;
}

static bool edge_checks(void) {
    AbGoalAnimation state = {
        .strength = 1, .special_timer = 1, .special_expression = true};
    AbGoalAnimationControls controls = {
        .base_strength = 1, .damage1_min = 0, .disable_smiling = true};
    CHECK(!ab_goal_expression_update(NULL, 0, true, NULL, NULL));
    CHECK(!ab_goal_oink_update(NULL, 0, false, NULL, NULL));
    CHECK(ab_goal_expression_update(&state, NAN, false, NULL, NULL));
    CHECK(ab_goal_oink_update(&state, NAN, false, NULL, NULL));
    CHECK(!ab_goal_expression_update(&state, NAN, true, &controls, NULL));
    CHECK(state.special_timer == 1 && state.special_expression);
    CHECK(ab_goal_expression_update(&state, 1, true, &controls, NULL));
    CHECK(state.special_timer == 0 && !state.special_expression);
    CHECK(ab_goal_expression_update(&state, 0, true, &controls, NULL));
    CHECK(ab_goal_oink_update(&state, 0, true, NULL, NULL));
    state.oink_timer = -1;
    CHECK(!ab_goal_oink_update(&state, 0, true, NULL, NULL));
    CHECK(state.oink_timer == -1);
    state.oink_timer = NAN;
    CHECK(!ab_goal_oink_update(&state, 0, true, NULL, NULL));
    return true;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        return edge_checks() ? 0 : 1;
    }
    if (argc != 2) {
        fprintf(stderr, "Usage: ab_goal_animation_tests [NATIVE_CORPUS]\n");
        return 2;
    }
    FILE *file = fopen(argv[1], "rb");
    if (!file) {
        return 1;
    }
    bool okay = native_checks(file);
    if (fclose(file) != 0) {
        okay = false;
    }
    return okay ? 0 : 1;
}
