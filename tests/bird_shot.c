#include "ab/bird_shot.h"
#include "ab/physics_step.h"
#include "ab/slingshot.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Shot failed at line %d: %s\n", __LINE__, #condition);     \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    AbShotState state;
    AbShotBird bird;
    AbVec2 position, impulse;
    uint32_t input[19], events[24][23];
    size_t event_count;
    AbShotOperation fail_at;
} ShotCase;

static float from_bits(uint32_t bits) {
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static uint32_t bits(float value) {
    uint32_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static uint32_t text_id(const char *value) {
    return !value                           ? 0
           : strcmp(value, "hook") == 0     ? 1
           : strcmp(value, "flying") == 0   ? 2
           : strcmp(value, "sound") == 0    ? 3
           : strcmp(value, "resolved") == 0 ? 4
           : strcmp(value, "idle") == 0     ? 5
                                            : UINT32_MAX;
}

static void snapshot(const ShotCase *test, uint32_t output[15]) {
    const AbShotState *state = &test->state;
    const AbShotBird *bird = &test->bird;
    output[0] = (bird->shot ? 1u : 0u) | (bird->has_collided ? 2u : 0u) |
                (state->specialty_available ? 4u : 0u) |
                (state->moving_above_tolerance ? 8u : 0u) |
                (state->selection_played ? 16u : 0u) | (state->bird_ready ? 32u : 0u) |
                (state->bird_fired ? 64u : 0u) | (state->dragged ? 128u : 0u) |
                (state->flying ? 256u : 0u) | (state->current_bird_name ? 512u : 0u) |
                (state->other_bird_count ? 1024u : 0u);
    const float values[] = {
        state->birds_shot,      state->tutorial_timer,   state->next_bird_timer,
        state->last_score_time, state->old_score,        bird->mass,
        bird->start_impulse.x,  bird->start_impulse.y,   bird->position.x,
        bird->position.y,       bird->launch_velocity.x, bird->launch_velocity.y};
    for (size_t field = 0; field < 12; ++field) {
        output[field + 1] = bits(values[field]);
    }
    output[13] = text_id(bird->sprite);
    output[14] = bits(state->score);
}

static bool host(void *context, AbShotCall *call, AbError *error) {
    ShotCase *test = context;
    CHECK(test->event_count < 24);
    uint32_t *event = test->events[test->event_count++];
    event[0] = call->operation;
    event[1] = bits(call->vector.x);
    event[2] = bits(call->vector.y);
    event[3] = bits(call->point.x);
    event[4] = bits(call->point.y);
    event[5] = bits(call->value);
    event[6] = bits(call->backwards_count);
    snapshot(test, event + 7);
    event[22] = text_id(call->text);
    if (call->operation == test->fail_at) {
        return ab_fail(error, "Requested shot boundary failure");
    }
    uint32_t flags = test->input[18];
    switch (call->operation) {
        case AB_SHOT_SET_POSITION:
            CHECK(call->bird == &test->bird);
            call->bird->position = call->vector;
            break;
        case AB_SHOT_SET_VELOCITY:
            CHECK(call->bird == &test->bird && call->vector.x == 0 &&
                  call->vector.y == 0);
            break;
        case AB_SHOT_READ_SHOT_COUNT:
            call->value = from_bits(test->input[16]);
            break;
        case AB_SHOT_READ_BACKWARDS_COUNT:
            call->value = from_bits(test->input[17]);
            break;
        case AB_SHOT_NOTIFY_BIRD:
            CHECK(call->launch_position == &test->position &&
                  call->launch_impulse == &test->impulse);
            event[1] = bits(call->launch_impulse->x);
            event[2] = bits(call->launch_impulse->y);
            event[3] = bits(call->launch_position->x);
            event[4] = bits(call->launch_position->y);
            if (flags & 128) {
                test->impulse.x = -test->impulse.x;
                test->impulse.y += 0.25f;
                test->position.x += 2;
                test->position.y -= 3;
                test->bird.position.x += 0.5f;
                test->bird.position.y -= 0.75f;
                test->bird.mass *= 2;
            }
            break;
        case AB_SHOT_LEVEL_HOOK:
            if (flags & 256) {
                test->bird.mass *= 0.5f;
                test->state.score += 125;
                test->state.bird_reload_delay += 0.125f;
                test->impulse.x += 1.25f;
            }
            break;
        case AB_SHOT_READ_DEFINITION:
            call->definition =
                (AbShotDefinition){.on_shot = flags & 2 ? "hook" : NULL,
                                   .sprite_flying = flags & 4 ? "flying" : NULL,
                                   .launch_sound = flags & 16 ? "sound" : NULL};
            break;
        case AB_SHOT_DEFINITION_HOOK:
            test->state.tutorial_timer = 999;
            break;
        case AB_SHOT_RESOLVE_AUDIO:
            call->text = "resolved";
            break;
        case AB_SHOT_SCRIPT_LAUNCH:
            if (flags & 512) {
                test->bird.mass *= 4;
                test->impulse.y -= 0.5f;
            }
            break;
        case AB_SHOT_APPLY_IMPULSE:
        case AB_SHOT_NOTIFY_ALL:
        case AB_SHOT_SET_SPRITE:
        case AB_SHOT_ATTACH_PARTICLES:
        case AB_SHOT_PLAY_AUDIO:
        case AB_SHOT_PLAY_SLING_SOUND:
            break;
        default:
            CHECK(false);
    }
    return true;
}

static void prepare(ShotCase *test) {
    const uint32_t *input = test->input;
    test->position = (AbVec2){from_bits(input[0]), from_bits(input[1])};
    test->impulse = (AbVec2){from_bits(input[2]), from_bits(input[3])};
    test->bird = (AbShotBird){.name = "first",
                              .sprite = "idle",
                              .position = {from_bits(input[4]), from_bits(input[5])},
                              .mass = from_bits(input[6]),
                              .has_collided = true,
                              .override_drawing = (input[18] & 8) != 0,
                              .has_script = (input[18] & 32) != 0};
    test->state = (AbShotState){.dragged = &test->bird,
                                .current_bird_name = "first",
                                .tutorial_timer = from_bits(input[14]),
                                .birds_shot = from_bits(input[7]),
                                .bird_count = from_bits(input[8]),
                                .next_bird_timer = from_bits(input[15]),
                                .bird_reload_delay = from_bits(input[9]),
                                .last_score_time = from_bits(input[12]),
                                .time = from_bits(input[10]),
                                .old_score = from_bits(input[13]),
                                .score = from_bits(input[11]),
                                .other_bird_count = 1,
                                .selection_played = true,
                                .bird_ready = true,
                                .has_level_hook = (input[18] & 1) != 0};
}

static bool edge_checks(void) {
    ShotCase test = {0};
    test.input[6] = bits(2);
    test.input[8] = bits(1);
    test.input[2] = bits(4);
    test.input[3] = bits(-6);
    prepare(&test);
    AbError error = {0};
    CHECK(!ab_bird_shot(NULL, &test.position, &test.impulse, host, &test, &error));
    CHECK(test.event_count == 0);
    CHECK(
        ab_bird_shot(&test.state, &test.position, &test.impulse, host, &test, &error));
    CHECK(!test.state.dragged && test.state.flying == &test.bird &&
          test.bird.launch_velocity.x == 2 && test.bird.launch_velocity.y == -3);
    CHECK(test.state.bird_fired && !test.state.bird_ready &&
          test.state.specialty_available && !test.state.selection_played);
    test = (ShotCase){.fail_at = AB_SHOT_APPLY_IMPULSE};
    test.input[6] = bits(2);
    prepare(&test);
    CHECK(
        !ab_bird_shot(&test.state, &test.position, &test.impulse, host, &test, &error));
    CHECK(test.event_count == 6 && test.state.dragged && !test.bird.shot &&
          !test.state.bird_fired && test.state.bird_ready);
    return true;
}

static bool read_words(FILE *file, uint32_t *values, size_t count) {
    for (size_t field = 0; field < count; ++field) {
        CHECK(fscanf(file, "%8" SCNx32, values + field) == 1);
    }
    CHECK(fgetc(file) == '\n');
    return true;
}

static bool compare_words(const uint32_t *actual, const uint32_t *expected,
                          size_t count, size_t item, size_t event) {
    for (size_t field = 0; field < count; ++field) {
        if (actual[field] != expected[field]) {
            fprintf(stderr,
                    "Shot %zu event %zu field %zu: %08" PRIx32 " != %08" PRIx32 "\n",
                    item, event, field, actual[field], expected[field]);
            return false;
        }
    }
    return true;
}

static bool live_shot_checks(FILE *file) {
    uint32_t controls[8], data[17], expected[5];
    CHECK(read_words(file, controls, 8) && read_words(file, data, 17));
    CHECK(data[0] == 2 && data[1] == AB_BODY_DYNAMIC && data[2] <= 1);
    AbSlingLaunchInput launch = {
        .rest = {from_bits(controls[0]), from_bits(controls[1])},
        .rubber_band = {from_bits(data[11]), from_bits(data[12])},
        .stretch = from_bits(controls[6]),
        .launch_force = from_bits(controls[3]),
        .physics_scale = from_bits(controls[4]),
        .bird_mass = from_bits(controls[7]),
        .slingshot_power = from_bits(controls[5]),
        .bird_present = true};
    AbVec2 impulse;
    CHECK(ab_slingshot_launch_impulse(&launch, NULL, &impulse));
    CHECK(bits(impulse.x) == data[13] && bits(impulse.y) == data[14]);
    AbBodyMotion body = {
        .mass = {.type = (AbBodyType)data[1],
                 .linear_velocity = {from_bits(data[3]), from_bits(data[4])},
                 .angular_velocity = from_bits(data[5]),
                 .inverse_mass = from_bits(data[7]),
                 .inverse_inertia = from_bits(data[8]),
                 .center = {from_bits(data[9]), from_bits(data[10])},
                 .transform.position = {from_bits(data[11]), from_bits(data[12])}},
        .awake = data[2] != 0,
        .sleep_time = from_bits(data[6])};
    AbBodyMotion before = body;
    AbVec2 point = {from_bits(data[15]), from_bits(data[16])};
    CHECK(ab_body_impulse_apply(&body, impulse, &point));
    uint32_t actual[] = {36 | (body.awake ? 2u : 0u), bits(body.mass.linear_velocity.x),
                         bits(body.mass.linear_velocity.y),
                         bits(body.mass.angular_velocity), bits(body.sleep_time)};
    CHECK(read_words(file, expected, 5) && compare_words(actual, expected, 5, 0, 0));
    body.mass.linear_velocity = before.mass.linear_velocity;
    CHECK(memcmp(&body, &before, sizeof(body)) == 0);
    char marker[80];
    CHECK(fgets(marker, sizeof(marker), file));
    CHECK(strcmp(marker, "AB live bird shot complete\n") == 0);
    CHECK(fgetc(file) == EOF && !ferror(file));
    puts("Live first-level launch impulse and resulting velocity match the native "
         "entry/return capture; all other captured scene fields were unchanged.");
    return true;
}

static bool native_checks(FILE *file) {
    char marker[80];
    CHECK(fgets(marker, sizeof(marker), file));
    if (strcmp(marker, "AB_LIVE_BIRD_SHOT_V1 1\n") == 0) {
        return live_shot_checks(file);
    }
    CHECK(strcmp(marker, "AB_ANDROID_BIRD_SHOT_V1 4096\n") == 0);
    size_t events = 0;
    for (size_t item = 0; item < 4096; ++item) {
        ShotCase test = {0};
        uint32_t actual[19], expected[23], count;
        CHECK(read_words(file, test.input, 19));
        prepare(&test);
        AbError error = {0};
        CHECK(ab_bird_shot(&test.state, &test.position, &test.impulse, host, &test,
                           &error));
        snapshot(&test, actual);
        actual[15] = bits(test.position.x);
        actual[16] = bits(test.position.y);
        actual[17] = bits(test.impulse.x);
        actual[18] = bits(test.impulse.y);
        CHECK(read_words(file, expected, 19));
        CHECK(compare_words(actual, expected, 19, item, SIZE_MAX));
        CHECK(read_words(file, &count, 1) && count == test.event_count);
        for (size_t event = 0; event < count; ++event) {
            CHECK(read_words(file, expected, 23));
            CHECK(compare_words(test.events[event], expected, 23, item, event));
        }
        events += count;
    }
    CHECK(fgets(marker, sizeof(marker), file));
    CHECK(strcmp(marker, "AB bird shot reference complete\n") == 0);
    CHECK(fgetc(file) == EOF && !ferror(file));
    printf("All 4096 original bird-shot transitions and %zu ordered boundaries match, "
           "including mutable event vectors and callback mass changes.\n",
           events);
    return true;
}

int main(int argc, char **argv) {
    if (argc > 2) {
        fprintf(stderr, "Usage: ab_bird_shot_tests [NATIVE_BIRD_SHOT_TRACE]\n");
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
