#include "ab/level_complete.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Level complete failed at line %d: %s\n", __LINE__,        \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    AbLevelCompleteState *state;
    const uint32_t *input;
    uint32_t events[7][10];
    size_t count;
} CompleteHost;

static float number(uint32_t bits) {
    float result;
    memcpy(&result, &bits, sizeof(result));
    return result;
}

static uint32_t word(float value) {
    uint32_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static void snapshot(const AbLevelCompleteState *state, uint32_t output[8]) {
    const uint32_t values[] = {word(state->current_bird_index),
                               word(state->birds_shot),
                               word(state->remaining_bird_index),
                               word(state->bonus_timer),
                               word(state->ending_timer),
                               state->completed,
                               state->completing,
                               state->eagle_launched};
    memcpy(output, values, sizeof(values));
}

static bool host_call(void *context, AbLevelCompleteCall *call, AbError *error) {
    (void)error;
    CompleteHost *host = context;
    AbLevelCompleteState *state = host->state;
    CHECK(host->count < 7);
    uint32_t *event = host->events[host->count++];
    event[0] = (uint32_t)call->operation;
    event[1] =
        call->operation == AB_LEVEL_COMPLETE_QUERY_BIRD ? word(call->bird_index) : 0;
    snapshot(state, event + 2);
    if (call->operation == AB_LEVEL_COMPLETE_CAMERA) {
        CHECK(call->text && strcmp(call->text, "LevelEndSequence") == 0);
    } else if (call->operation == AB_LEVEL_COMPLETE_RESOLVE_AUDIO) {
        CHECK(call->text && strcmp(call->text, "clear") == 0);
    } else if (call->operation == AB_LEVEL_COMPLETE_PLAY_AUDIO) {
        CHECK(call->text && strcmp(call->text, "sound") == 0);
    }
    uint32_t mask = host->input[14], flags = host->input[17];
    if (mask & (UINT32_C(1) << ((uint32_t)call->operation - 1))) {
        switch (call->operation) {
            case AB_LEVEL_COMPLETE_NOTIFY:
            case AB_LEVEL_COMPLETE_CANCEL_DRAG:
                state->current_bird_index = number(host->input[15]);
                state->birds_shot = number(host->input[16]);
                state->current_bird_present = (flags & 1) != 0;
                state->current_bird_shot_literal_true = (flags & 2) != 0;
                state->special_mode = (flags & 4) != 0;
                state->disable_bird_score = (flags & 8) != 0;
                break;
            case AB_LEVEL_COMPLETE_QUERY_BIRD:
                state->remaining_bird_index = number(host->input[15]);
                break;
            case AB_LEVEL_COMPLETE_CAMERA:
                state->bonus_timer = number(host->input[16]);
                state->completed = false;
                break;
            case AB_LEVEL_COMPLETE_RESOLVE_AUDIO:
                state->eagle_launched = (flags & 16) != 0;
                state->has_level_hook = (flags & 32) != 0;
                break;
            case AB_LEVEL_COMPLETE_PLAY_AUDIO:
                state->eagle_launched = (flags & 16) == 0;
                state->completed = false;
                break;
            case AB_LEVEL_COMPLETE_SCRIPT:
                state->ending_timer = number(host->input[15]);
                state->completing = false;
                break;
        }
    }
    if (call->operation == AB_LEVEL_COMPLETE_QUERY_BIRD) {
        call->bird_present = host->input[13] != 0;
    } else if (call->operation == AB_LEVEL_COMPLETE_RESOLVE_AUDIO) {
        call->text = "sound";
    }
    return true;
}

static bool fail_host(void *context, AbLevelCompleteCall *call, AbError *error) {
    size_t *count = context;
    ++*count;
    if (call->operation == AB_LEVEL_COMPLETE_CANCEL_DRAG) {
        return ab_fail(error, "Expected cancellation failure");
    }
    return true;
}

static bool accept_host(void *context, AbLevelCompleteCall *call, AbError *error) {
    (void)call;
    (void)error;
    size_t *count = context;
    ++*count;
    return true;
}

static bool edge_checks(void) {
    AbLevelCompleteState state = {0};
    AbError error = {0};
    size_t calls = 0;
    CHECK(!ab_level_complete_begin(&state, fail_host, &calls, &error));
    CHECK(calls == 2 && !state.completed);
    CHECK(!ab_level_complete_begin(NULL, fail_host, &calls, &error));
    CHECK(!ab_level_complete_begin(&state, NULL, &calls, &error));
    state.current_bird_index = NAN;
    calls = 0;
    CHECK(!ab_level_complete_begin(&state, accept_host, &calls, &error));
    CHECK(calls == 2 && state.completed);
    return true;
}

static bool read_words(FILE *file, uint32_t *output, size_t count) {
    for (size_t field = 0; field < count; ++field) {
        CHECK(fscanf(file, "%8" SCNx32, output + field) == 1);
    }
    CHECK(fgetc(file) == '\n');
    return true;
}

static bool native_checks(FILE *file) {
    char marker[80];
    CHECK(fgets(marker, sizeof(marker), file));
    CHECK(strcmp(marker, "AB_ANDROID_LEVEL_COMPLETE_V1 4096\n") == 0);
    size_t calls = 0, cameras = 0, scripts = 0;
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[18], expected[8], count;
        CHECK(read_words(file, input, 18) && read_words(file, expected, 8));
        CHECK(input[5] <= 1 && input[10] <= 1 && input[14] <= 127 && input[17] <= 63);
        for (size_t field = 6; field < 14; ++field) {
            CHECK(input[field] <= 4);
        }
        AbLevelCompleteState state = {.clear_audio_group = "clear",
                                      .current_bird_index = number(input[0]),
                                      .birds_shot = number(input[1]),
                                      .remaining_bird_index = number(input[2]),
                                      .bonus_timer = number(input[3]),
                                      .ending_timer = number(input[4]),
                                      .current_bird_present = input[5] != 0,
                                      .current_bird_shot_literal_true = input[6] == 2,
                                      .special_mode = input[7] >= 2,
                                      .disable_bird_score = input[8] >= 2,
                                      .eagle_launched = input[9] >= 2,
                                      .has_level_hook = input[10] != 0,
                                      .completed = input[11] >= 2,
                                      .completing = input[12] >= 2};
        CompleteHost host = {.state = &state, .input = input};
        AbError error = {0};
        CHECK(ab_level_complete_begin(&state, host_call, &host, &error));
        uint32_t actual[8];
        snapshot(&state, actual);
        for (size_t field = 0; field < 8; ++field) {
            if (actual[field] != expected[field]) {
                fprintf(stderr,
                        "Native completion %zu field %zu: %08" PRIx32 " != %08" PRIx32
                        "\n",
                        item, field, actual[field], expected[field]);
                return false;
            }
        }
        CHECK(read_words(file, &count, 1) && count == host.count);
        for (size_t event = 0; event < host.count; ++event) {
            uint32_t native[10];
            CHECK(read_words(file, native, 10));
            if (memcmp(native, host.events[event], sizeof(native)) != 0) {
                fprintf(stderr, "Native completion %zu event %zu mismatch\n", item,
                        event);
                return false;
            }
            cameras += native[0] == AB_LEVEL_COMPLETE_CAMERA;
            scripts += native[0] == AB_LEVEL_COMPLETE_SCRIPT;
        }
        calls += host.count;
    }
    CHECK(fgets(marker, sizeof(marker), file));
    CHECK(strcmp(marker, "AB level complete reference complete\n") == 0);
    CHECK(fgetc(file) == EOF && !ferror(file));
    printf("All 4096 original completion transitions match: %zu ordered boundaries, "
           "%zu camera requests, %zu level hooks.\n",
           calls, cameras, scripts);
    return true;
}

int main(int argc, char **argv) {
    if (argc > 2) {
        fprintf(stderr, "Usage: ab_level_complete_tests [NATIVE_COMPLETION_TRACE]\n");
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
