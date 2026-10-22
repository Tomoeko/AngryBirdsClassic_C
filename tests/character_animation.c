#include "ab/character_animation.h"
#include "ab/random.h"
#include "character_state.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Character animation failed at %d: %s\n", __LINE__,        \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    AbRandom48 random;
    uint32_t controls[11], events[64][4];
    size_t count;
} CharacterTest;

typedef struct {
    CharacterTest *test;
    size_t index;
} CharacterContext;

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

static uint32_t *record(CharacterTest *test, uint32_t kind) {
    if (test->count >= 64) {
        return NULL;
    }
    uint32_t *event = test->events[test->count++];
    event[0] = kind;
    event[1] = event[2] = event[3] = 0;
    return event;
}

static bool draw(void *context, int minimum, int maximum, int *output) {
    CharacterTest *test = ((CharacterContext *)context)->test;
    float value;
    CHECK(ab_script_random(&test->random, 2, minimum, maximum, &value) ==
          AB_SCRIPT_RANDOM_OK);
    uint32_t *event = record(test, 1);
    CHECK(event);
    event[1] = raw((float)minimum);
    event[2] = raw((float)maximum);
    event[3] = raw(value);
    *output = (int)value;
    return true;
}

static bool lost(void *context, bool *output, AbError *error) {
    (void)error;
    CharacterTest *test = ((CharacterContext *)context)->test;
    CHECK(record(test, 2));
    *output = test->controls[10] != 0;
    return true;
}

static bool face(CharacterContext *context, uint32_t kind) {
    uint32_t *event = record(context->test, 3);
    CHECK(event);
    event[1] = (uint32_t)context->index;
    event[2] = kind;
    return true;
}

static bool goal_sprite(void *context, AbGoalFace sprite, AbError *error) {
    (void)error;
    return face(context, (uint32_t)sprite);
}

static bool bird_sprite(void *context, AbBirdFace sprite, AbError *error) {
    (void)error;
    return face(context, sprite == AB_BIRD_FACE_BASE   ? 5u
                         : sprite == AB_BIRD_FACE_YELL ? 6u
                                                       : 4u);
}

static bool distance(void *context, float *output, AbError *error) {
    (void)error;
    CharacterTest *test = ((CharacterContext *)context)->test;
    CHECK(record(test, 4));
    *output = number(test->controls[9]);
    return true;
}

static bool audio(CharacterContext *context, float volume, uint32_t group) {
    uint32_t *selection = record(context->test, 5);
    CHECK(selection);
    selection[1] = group;
    uint32_t *event = record(context->test, 6);
    CHECK(event);
    event[1] = raw(volume);
    return true;
}

static bool oink(void *context, float volume, AbError *error) {
    (void)error;
    return audio(context, volume, 1);
}

static bool yell(void *context, float volume, AbError *error) {
    (void)error;
    return audio(context, volume, 2);
}

static bool apply_jump(void *context, size_t index, const AbBirdJumpFrame *frame,
                       AbError *error) {
    (void)error;
    CharacterTest *test = context;
    for (size_t cursor = 0; cursor < frame->event_count; ++cursor) {
        const AbBirdJumpEvent *input = frame->events + cursor;
        uint32_t *event = record(test, input->kind == AB_BIRD_JUMP_POSITION ? 7u : 8u);
        CHECK(event);
        event[1] = (uint32_t)index;
        event[2] = raw(input->value.x);
        if (input->kind == AB_BIRD_JUMP_POSITION) {
            event[3] = raw(input->value.y);
        }
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

static AbCharacterAnimationObject from_words(const uint32_t input[20]) {
    return (AbCharacterAnimationObject){
        .goal = {number(input[0]), number(input[1]), number(input[2]), number(input[3]),
                 input[4] != 0, input[5] != 0},
        .bird = {number(input[6]), input[7] != 0},
        .jump = {{number(input[8]), number(input[9])},
                 number(input[10]),
                 number(input[11]),
                 number(input[12]),
                 number(input[13]),
                 number(input[14]),
                 number(input[15]),
                 number(input[16]),
                 number(input[17]),
                 input[18] != 0,
                 input[19] != 0},
    };
}

static bool compare(const uint32_t *actual, const uint32_t *expected, size_t count,
                    size_t sample, size_t frame, const char *phase) {
    for (size_t index = 0; index < count; ++index) {
        if (actual[index] != expected[index]) {
            fprintf(stderr,
                    "Character sample %zu frame %zu %s word %zu: %08" PRIx32
                    " != %08" PRIx32 "\n",
                    sample, frame, phase, index, actual[index], expected[index]);
            return false;
        }
    }
    return true;
}

static bool native_checks(FILE *file) {
    char header[80];
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB_ANDROID_CHARACTER_ANIMATION_V1 64 32 5\n") == 0);
    size_t totals[8] = {0};
    for (size_t sample = 0; sample < 64; ++sample) {
        CharacterTest test = {0};
        uint32_t seed, native_order[5];
        CHECK(words(file, &seed, 1) && words(file, native_order, 5) &&
              ab_random48_seed(&test.random, seed));
        size_t order[5];
        for (size_t index = 0; index < 5; ++index) {
            CHECK(native_order[index] < 5);
            order[index] = native_order[index];
        }
        AbCharacterAnimationObject objects[5];
        AbCharacterAnimationBindings bindings[5];
        CharacterContext contexts[5];
        for (size_t index = 0; index < 5; ++index) {
            uint32_t initial[20];
            CHECK(words(file, initial, 20));
            objects[index] = from_words(initial);
            contexts[index] = (CharacterContext){&test, index};
            bindings[index] = (AbCharacterAnimationBindings){
                .goal = {.context = contexts + index,
                         .random_integer = draw,
                         .lose_condition = lost,
                         .set_sprite = goal_sprite,
                         .camera_distance = distance,
                         .play_oink = oink,
                         .base_strength = 10,
                         .damage1_min = 50},
                .bird = {.context = contexts + index,
                         .random_integer = draw,
                         .camera_distance = distance,
                         .play_yell = yell,
                         .set_sprite = bird_sprite,
                         .yell_sprite_present = true,
                         .blink_sprite_present = true},
                .jump = {contexts + index, draw},
            };
        }
        AbCharacterAnimations animation = {
            .objects = objects,
            .bindings = bindings,
            .count = 5,
            .goal_order = order,
            .goal_count = 2,
            .bird_order = order + 2,
            .bird_count = 3,
            .context = &test,
            .apply_jump = apply_jump,
        };
        for (size_t frame = 0; frame < 32; ++frame) {
            uint32_t expected[3];
            CHECK(words(file, test.controls, 11) && words(file, expected, 3));
            CHECK(expected[1] <= 65535 && expected[2] <= 64 && test.controls[1] < 5 &&
                  test.controls[7] == 31);
            animation.current_bird = test.controls[1];
            animation.level_completed = (test.controls[2] & 2) != 0;
            for (size_t index = 0; index < 5; ++index) {
                objects[index].override_drawing = (test.controls[3] >> index) & 1;
                objects[index].frozen_solid = (test.controls[4] >> index) & 1;
                objects[index].shot = (test.controls[5] >> index) & 1;
                objects[index].animation_enabled = true;
                objects[index].allow_roll = test.controls[8] != 0;
                bindings[index].goal.disable_smiling = (test.controls[6] >> index) & 1;
                bindings[index].bird.eagle_bait_launched = (test.controls[2] & 1) != 0;
            }
            test.count = 0;
            AbError error = {0};
            CHECK(ab_character_animations_update(&animation, number(test.controls[0]),
                                                 &error));
            uint32_t actual[] = {(uint32_t)test.random.value,
                                 (uint32_t)(test.random.value >> 32),
                                 (uint32_t)test.count};
            CHECK(compare(actual, expected, 3, sample, frame, "RNG/events"));
            for (size_t index = 0; index < 5; ++index) {
                uint32_t state[20], expected_state[20];
                character_state_words(objects + index, state);
                CHECK(words(file, expected_state, 20) &&
                      compare(state, expected_state, 20, sample, frame,
                              index < 2 ? "goal" : "bird"));
            }
            for (size_t event = 0; event < test.count; ++event) {
                uint32_t expected_event[4];
                CHECK(words(file, expected_event, 4) && expected_event[0] >= 1 &&
                      expected_event[0] <= 8 &&
                      compare(test.events[event], expected_event, 4, sample, frame,
                              "callback"));
                ++totals[expected_event[0] - 1];
            }
        }
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB character animation reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("All 2048 native five-object frames and RNG states match: %zu draws, "
           "%zu sprites, %zu sounds, %zu positions, %zu rotations\n",
           totals[0], totals[2], totals[5], totals[6], totals[7]);
    return true;
}

static bool edge_checks(void) {
    AbCharacterAnimations animation = {0};
    CHECK(!ab_character_animations_update(NULL, 0, NULL));
    CHECK(ab_character_animations_update(&animation, NAN, NULL));
    AbCharacterAnimationObject objects[2] = {0};
    AbCharacterAnimationBindings bindings[2] = {0};
    size_t order[] = {0, 0};
    animation = (AbCharacterAnimations){.objects = objects,
                                        .bindings = bindings,
                                        .count = 2,
                                        .bird_order = order,
                                        .bird_count = 2,
                                        .current_bird = SIZE_MAX};
    CHECK(!ab_character_animations_update(&animation, 0, NULL));
    CHECK(objects[0].bird.timer == 0 && objects[1].bird.timer == 0);
    order[1] = 2;
    CHECK(!ab_character_animations_update(&animation, 0, NULL));
    order[1] = 1;
    CHECK(ab_character_animations_update(&animation, NAN, NULL));
    objects[0].animation_enabled = true;
    objects[0].bird.timer = 1;
    objects[0].jump.timer = -1;
    animation.current_bird = 0;
    CHECK(ab_character_animations_update(&animation, 0.5f, NULL));
    CHECK(objects[0].bird.timer == 0.5f && objects[0].jump.timer == -1);
    animation.current_bird = 1;
    CHECK(!ab_character_animations_update(&animation, 0, NULL));
    bool removed[] = {true, true};
    AbCharacterAnimationObject saved[2];
    memcpy(saved, objects, sizeof(saved));
    animation.removed_objects = removed;
    animation.goal_order = order;
    animation.goal_count = 2;
    CHECK(ab_character_animations_update(&animation, NAN, NULL) &&
          !memcmp(saved, objects, sizeof(saved)));
    return true;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        return edge_checks() ? 0 : 1;
    }
    if (argc != 2) {
        fprintf(stderr, "Usage: ab_character_animation_tests [NATIVE_CORPUS]\n");
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
