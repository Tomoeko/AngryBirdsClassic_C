#include "ab/bird_animation.h"

#include <math.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Bird animation failed at %d: %s\n", __LINE__,             \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    AbBirdAnimation *state;
    int values[2];
    size_t draws;
    size_t sprites;
    bool saw_active;
} EdgeAnimation;

static bool draw_edge(void *context, int minimum, int maximum, int *output) {
    EdgeAnimation *edge = context;
    CHECK(edge->draws < 2 && minimum <= maximum);
    *output = edge->values[edge->draws++];
    return true;
}

static bool set_edge(void *context, AbBirdFace face, AbError *error) {
    (void)error;
    EdgeAnimation *edge = context;
    ++edge->sprites;
    edge->saw_active = edge->state->active;
    CHECK(face == AB_BIRD_FACE_BASE && edge->state->timer == 1.51f);
    edge->state->timer = 3;
    return true;
}

static bool edge_checks(void) {
    AbBirdAnimation state = {0};
    CHECK(!ab_bird_animation_update(NULL, 0, true, NULL, NULL));
    CHECK(ab_bird_animation_update(&state, NAN, false, NULL, NULL));
    CHECK(!ab_bird_animation_update(&state, NAN, true, NULL, NULL));
    CHECK(state.timer == 0 && !state.active);
    CHECK(ab_bird_animation_update(&state, 0, true, NULL, NULL));
    state.timer = -1;
    CHECK(!ab_bird_animation_update(&state, 0, true, NULL, NULL));
    EdgeAnimation edge = {.state = &state, .values = {21, 5}};
    AbBirdAnimationControls controls = {.context = &edge, .random_integer = draw_edge};
    CHECK(ab_bird_animation_update(&state, 0, true, &controls, NULL));
    CHECK(state.timer == 0.21f && state.active && edge.draws == 2 && !edge.sprites);
    state.timer = -1;
    edge.values[0] = 151;
    edge.draws = 0;
    controls.set_sprite = set_edge;
    CHECK(ab_bird_animation_update(&state, 0, true, &controls, NULL));
    CHECK(state.timer == 3 && !state.active && edge.saw_active && edge.sprites == 1);
    return true;
}

typedef struct {
    AbBirdAnimation *state;
    AbBirdAnimationControls *controls;
    uint32_t input[7];
    uint32_t random_state;
    uint32_t events[10][6];
    size_t count;
} NativeAnimation;

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

static uint32_t *record_event(NativeAnimation *test, uint32_t kind) {
    if (test->count >= 10) {
        return NULL;
    }
    uint32_t *event = test->events[test->count++];
    event[0] = kind;
    event[4] = raw(test->state->timer);
    event[5] = test->state->active;
    return event;
}

static void mutate_animation(NativeAnimation *test) {
    test->state->timer = number(test->input[6]);
    test->state->active = !(test->input[2] & 1);
}

static bool native_random(void *context, int minimum, int maximum, int *output) {
    NativeAnimation *test = context;
    CHECK(minimum >= 1 && maximum >= minimum && maximum <= 150);
    test->random_state ^= test->random_state << 13;
    test->random_state ^= test->random_state >> 17;
    test->random_state ^= test->random_state << 5;
    uint32_t value =
        (uint32_t)minimum + test->random_state % ((uint32_t)(maximum - minimum) + 1);
    if ((test->input[2] & 128) && !test->count) {
        value = (uint32_t)maximum + 1;
    }
    uint32_t *event = record_event(test, 1);
    CHECK(event);
    event[1] = (uint32_t)minimum;
    event[2] = (uint32_t)maximum;
    event[3] = value;
    *output = (int)value;
    return true;
}

static bool native_distance(void *context, float *output, AbError *error) {
    (void)error;
    NativeAnimation *test = context;
    uint32_t *event = record_event(test, 2);
    CHECK(event);
    event[1] = test->input[3];
    *output = number(test->input[3]);
    if (test->input[5] & 2) {
        test->controls->eagle_bait_launched = !(test->input[2] & 16);
        test->controls->yell_sprite_present = !(test->input[2] & 4);
    }
    return true;
}

static bool native_audio(void *context, float volume, AbError *error) {
    (void)error;
    NativeAnimation *test = context;
    CHECK(record_event(test, 3));
    uint32_t *event = record_event(test, 4);
    CHECK(event);
    event[1] = raw(volume);
    if (test->input[5] & 4) {
        mutate_animation(test);
    }
    return true;
}

static bool native_sprite(void *context, AbBirdFace face, AbError *error) {
    (void)error;
    NativeAnimation *test = context;
    uint32_t *event = record_event(test, 5);
    CHECK(event);
    event[1] = (uint32_t)face;
    if (test->input[5] & 1) {
        mutate_animation(test);
    }
    return true;
}

static bool read_words(FILE *file, uint32_t *words, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%8" SCNx32, words + index) == 1);
    }
    CHECK(fgetc(file) == '\n');
    return true;
}

static bool compare_words(const uint32_t *actual, const uint32_t *expected,
                          size_t count, size_t item, size_t event) {
    for (size_t index = 0; index < count; ++index) {
        if (actual[index] != expected[index]) {
            fprintf(stderr,
                    "Bird animation %zu event %zu field %zu: %08" PRIx32
                    " != %08" PRIx32 "\n",
                    item, event, index, actual[index], expected[index]);
            return false;
        }
    }
    return true;
}

static bool native_checks(FILE *file) {
    char header[80];
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB_ANDROID_BIRD_ANIMATION_V1 4096\n") == 0);
    size_t draws = 0, sprites = 0, audio = 0, distances = 0;
    for (size_t item = 0; item < 4096; ++item) {
        NativeAnimation test = {0};
        uint32_t expected[4];
        CHECK(read_words(file, test.input, 7) && read_words(file, expected, 4));
        CHECK(test.input[2] <= 511 && test.input[5] <= 7 && expected[3] <= 10);
        AbBirdAnimation state = {.timer = number(test.input[0]),
                                 .active = (test.input[2] & 1) != 0};
        AbBirdAnimationControls controls = {
            .context = &test,
            .random_integer = native_random,
            .camera_distance = native_distance,
            .play_yell = native_audio,
            .set_sprite = native_sprite,
            .yell_sprite_present = (test.input[2] & 4) != 0,
            .blink_sprite_present = (test.input[2] & 8) != 0,
            .eagle_bait_launched = (test.input[2] & 16) != 0,
        };
        test.state = &state;
        test.controls = &controls;
        test.random_state = test.input[4];
        bool eligible =
            (test.input[2] & 2) && !(test.input[2] & 64) && !(test.input[2] & 256);
        AbError error = {0};
        CHECK(ab_bird_animation_update(&state, number(test.input[1]), eligible,
                                       &controls, &error));
        uint32_t actual[] = {raw(state.timer), state.active, test.random_state,
                             (uint32_t)test.count};
        CHECK(compare_words(actual, expected, 4, item, SIZE_MAX));
        for (size_t event = 0; event < test.count; ++event) {
            uint32_t reference[6];
            CHECK(read_words(file, reference, 6));
            CHECK(compare_words(test.events[event], reference, 6, item, event));
            draws += reference[0] == 1;
            distances += reference[0] == 2;
            audio += reference[0] == 4;
            sprites += reference[0] == 5;
        }
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB bird animation reference complete\n") == 0);
    CHECK(fgetc(file) == EOF && !ferror(file));
    printf("Matched 4096 original bird animation updates: %zu draws, %zu distance "
           "queries, %zu audio calls and %zu sprite changes\n",
           draws, distances, audio, sprites);
    return true;
}

int main(int argc, char **argv) {
    if (!edge_checks() || argc > 2) {
        return 1;
    }
    if (argc == 1) {
        return 0;
    }
    FILE *file = fopen(argv[1], "rb");
    if (!file) {
        return 1;
    }
    bool okay = native_checks(file);
    if (fclose(file)) {
        okay = false;
    }
    return okay ? 0 : 1;
}
