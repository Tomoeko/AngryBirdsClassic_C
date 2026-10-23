#include "ab/mighty_eagle_timer.h"

#include "mighty_eagle_timer_cases.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Eagle timer check at %d: %s\n", __LINE__, #condition);    \
            return false;                                                              \
        }                                                                              \
    } while (0)

static AbDataValue number(float value) {
    AbDataValue result = {.kind = AB_DATA_NUMBER};
    memcpy(&result.bits, &value, sizeof(value));
    return result;
}

static float real(uint32_t bits) {
    float result;
    memcpy(&result, &bits, sizeof(result));
    return result;
}

typedef struct {
    AbMightyEagleTimer *state;
    uint32_t events[5][3];
    size_t count;
    bool mutate;
    unsigned fail;
} Fixture;

static bool host(void *context, AbMightyEagleTimerCall call, AbError *error) {
    Fixture *fixture = context;
    CHECK(fixture->count < 5);
    uint32_t *event = fixture->events[fixture->count++];
    event[0] = (uint32_t)call.operation + 1;
    memcpy(event + 1, &call.position.x, sizeof(float));
    memcpy(event + 2, &call.position.y, sizeof(float));
    if (fixture->fail == event[0])
        return ab_fail(error, "Controlled Eagle timer host failure");
    if (fixture->mutate && (call.operation == AB_MIGHTY_EAGLE_CREATE ||
                            call.operation == AB_MIGHTY_EAGLE_YELL)) {
        fixture->state->camera_shake = number(50);
        fixture->state->destroy_joints =
            (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1};
        if (call.operation == AB_MIGHTY_EAGLE_CREATE)
            fixture->state->timer = number(-0.25f);
    }
    return true;
}

static bool edges(void) {
    AbMightyEagleTimer state = {0};
    CHECK(ab_mighty_eagle_timer_update(&state, NULL, NAN, NULL, NULL, NULL));
    state.timer = number(7.8f);
    Fixture fixture = {.state = &state, .fail = AB_MIGHTY_EAGLE_FLY + 1};
    CHECK(!ab_mighty_eagle_timer_update(&state, NULL, 0.2f, host, &fixture, NULL) &&
          fixture.count == 2 && state.sound_played.kind == AB_DATA_NIL &&
          real(state.timer.bits) == 7.8f - 0.2f);
    state = (AbMightyEagleTimer){.timer = number(6),
                                 .moving = {.kind = AB_DATA_BOOLEAN, .bits = 1},
                                 .bait_x = number(2),
                                 .bait_y = number(3)};
    AbDataValue specialty = {.kind = AB_DATA_BOOLEAN, .bits = 1};
    fixture = (Fixture){.state = &state, .mutate = true};
    CHECK(ab_mighty_eagle_timer_update(&state, &specialty, 0, host, &fixture, NULL) &&
          fixture.count == 3 && fixture.events[0][0] == AB_MIGHTY_EAGLE_CREATE + 1 &&
          fixture.events[1][0] == AB_MIGHTY_EAGLE_REMOVE_GOALS + 1 &&
          fixture.events[2][0] == AB_MIGHTY_EAGLE_DESTROY_JOINTS + 1 &&
          specialty.kind == AB_DATA_BOOLEAN && !specialty.bits &&
          state.moving.kind == AB_DATA_BOOLEAN && !state.moving.bits &&
          state.destroy_joints.kind == AB_DATA_NIL &&
          real(state.camera_shake.bits) == 50);
    return true;
}

static bool words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index)
        CHECK(fscanf(file, "%" SCNx32, output + index) == 1 &&
              fgetc(file) == (index + 1 == count ? '\n' : ' '));
    return true;
}

static void scalar(AbDataValue value, uint32_t output[2]) {
    output[0] = value.kind == AB_DATA_NUMBER    ? 3u
                : value.kind == AB_DATA_BOOLEAN ? 1u
                                                : 0u;
    output[1] = output[0] ? value.bits : 0;
}

static bool reference(FILE *file) {
    char line[96];
    CHECK(fgets(line, sizeof(line), file) &&
          !strcmp(line, "AB_ANDROID_MIGHTY_EAGLE_TIMER_V1 1024\n"));
    size_t calls = 0;
    for (uint32_t sample = 0; sample < AB_MIGHTY_EAGLE_TIMER_CASES; ++sample) {
        uint32_t input[6], generated[6], output[16], actual[16], count;
        mighty_timer_case(sample, generated);
        CHECK(words(file, input, 6) && !memcmp(input, generated, sizeof(input)));
        uint32_t flags = input[5];
        AbMightyEagleTimer state = {
            .timer = flags & 1 ? number(real(input[0])) : (AbDataValue){0},
            .moving = {.kind = AB_DATA_BOOLEAN, .bits = (flags & 2) != 0},
            .sound_played = {.kind = AB_DATA_BOOLEAN, .bits = (flags & 4) != 0},
            .bait_x = flags & 8 ? number(real(input[3])) : (AbDataValue){0},
            .bait_y = flags & 16 ? number(real(input[4])) : (AbDataValue){0},
            .camera_shake = flags & 32 ? number(real(input[2])) : (AbDataValue){0},
            .destroy_joints = flags & 128 ? number(0)
                              : flags & 64
                                  ? (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1}
                                  : (AbDataValue){0}};
        AbDataValue specialty = {.kind = AB_DATA_BOOLEAN, .bits = 1};
        Fixture fixture = {.state = &state, .mutate = (flags & 256) != 0};
        CHECK(ab_mighty_eagle_timer_update(&state, &specialty, real(input[1]), host,
                                           &fixture, NULL));
        AbDataValue fields[] = {
            state.timer,        state.moving,       state.bait_x,         state.bait_y,
            state.sound_played, state.camera_shake, state.destroy_joints, specialty};
        for (size_t index = 0; index < 8; ++index)
            scalar(fields[index], actual + index * 2);
        CHECK(words(file, output, 16) && words(file, &count, 1) && count <= 5);
        if (memcmp(output, actual, sizeof(output)) || count != fixture.count) {
            fprintf(stderr, "Native Eagle timer differs at case %u\n", sample);
            return false;
        }
        for (size_t event = 0; event < count; ++event) {
            uint32_t expected[3];
            CHECK(words(file, expected, 3) &&
                  !memcmp(expected, fixture.events[event], sizeof(expected)));
        }
        calls += count;
    }
    CHECK(fgets(line, sizeof(line), file) &&
          !strcmp(line, "AB mighty eagle timer reference complete\n") &&
          fgetc(file) == EOF && !ferror(file));
    printf("1024 original Eagle timer states and %zu ordered boundaries match.\n",
           calls);
    return true;
}

int main(int argc, char **argv) {
    if (argc > 2)
        return 2;
    bool okay = edges();
    if (okay && argc == 2) {
        FILE *file = fopen(argv[1], "rb");
        okay = file && reference(file);
        if (file && fclose(file) != 0)
            okay = false;
    }
    return okay ? 0 : 1;
}
