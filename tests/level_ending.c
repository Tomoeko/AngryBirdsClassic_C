#include "ab/level_flow.h"

#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Level ending failed at line %d: %s\n", __LINE__,          \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

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

static bool edge_checks(void) {
    AbLevelEndingDelay state = {.timer = 1, .moving_objects = true};
    AbLevelEndingGate result;
    CHECK(ab_level_ending_delay(&state, 0.1f, false, true, &result));
    CHECK(result.next.timer == 1 && result.next.extensions == 0);
    state.timer = number(UINT32_C(0x3f7fffff));
    CHECK(ab_level_ending_delay(&state, 0.1f, false, true, &result));
    CHECK(result.next.timer == 2 && result.next.extensions == 1);
    state.timer = 0;
    state.extensions = 3;
    CHECK(ab_level_ending_delay(&state, 0.1f, false, true, &result));
    CHECK(result.queue_level_ended && result.query_win_condition &&
          result.level_complete && result.next.timed_out);
    CHECK(ab_level_ending_delay(&result.next, 0.1f, true, false, &result));
    CHECK(result.queue_level_ended && !result.query_win_condition &&
          result.level_complete);
    state.movement_delay_present = true;
    state.movement_delay = 0;
    state.default_delay = -0.5f;
    CHECK(ab_level_ending_delay(&state, 0, false, false, &result));
    CHECK(!result.next.movement_delay_present && result.next.do_not_wait_literal_true &&
          result.next.delay_set && result.next.timer == -0.5f &&
          result.queue_level_ended && !result.level_complete);
    result.next.timer = 19;
    state.timer = NAN;
    CHECK(!ab_level_ending_delay(&state, 0, false, false, &result));
    CHECK(result.next.timer == 19);
    state.timer = 0;
    state.movement_delay = -FLT_MAX;
    CHECK(!ab_level_ending_delay(&state, FLT_MAX, false, false, &result));
    CHECK(!ab_level_ending_delay(NULL, 0, false, false, &result));
    CHECK(!ab_level_ending_delay(&state, 0, false, false, NULL));
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
    CHECK(strcmp(marker, "AB_ANDROID_LEVEL_ENDING_V1 4096\n") == 0);
    size_t queries = 0, queued = 0, extensions = 0, delays = 0;
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[14], expected[9], count;
        CHECK(read_words(file, input, 14) && read_words(file, expected, 9));
        CHECK(input[2] <= 1 && input[3] <= 4 && input[4] <= 1 && input[6] <= 4 &&
              input[7] <= 4 && input[9] <= 4 && input[11] <= 4 && input[12] <= 1);
        AbLevelEndingDelay state = {.timer = number(input[0]),
                                    .movement_delay = number(input[5]),
                                    .default_delay = number(input[8]),
                                    .extensions = number(input[10]),
                                    .movement_delay_present = input[4] != 0,
                                    .do_not_wait_literal_true = input[6] == 2,
                                    .delay_set = input[7] >= 2,
                                    .moving_objects = input[9] >= 2,
                                    .timed_out = input[11] >= 2};
        AbLevelEndingGate result = {.next = state};
        if (input[3] < 2) {
            if (!input[2]) {
                state.timer = state.timer - number(input[1]);
            }
            CHECK(ab_level_ending_delay(&state, number(input[1]), input[2] != 0,
                                        input[12] != 0, &result));
        }
        uint32_t actual[] = {
            word(result.next.timer),
            result.next.movement_delay_present,
            result.next.movement_delay_present ? word(result.next.movement_delay) : 0,
            result.next.do_not_wait_literal_true,
            result.next.delay_set,
            word(result.next.extensions),
            result.next.timed_out,
            result.level_complete,
            result.query_win_condition && result.next.timed_out};
        for (size_t field = 0; field < 9; ++field) {
            if (actual[field] != expected[field]) {
                fprintf(stderr,
                        "Native ending %zu field %zu: %08" PRIx32 " != %08" PRIx32 "\n",
                        item, field, actual[field], expected[field]);
                return false;
            }
        }
        uint32_t events[2];
        size_t event_count = 0;
        if (result.query_win_condition) {
            events[event_count++] = 1;
        }
        if (result.queue_level_ended) {
            events[event_count++] = 2;
        }
        CHECK(read_words(file, &count, 1) && count == event_count);
        for (size_t event = 0; event < event_count; ++event) {
            uint32_t native;
            CHECK(read_words(file, &native, 1) && native == events[event]);
        }
        queries += result.query_win_condition;
        queued += result.queue_level_ended;
        extensions += word(result.next.extensions) != input[10];
        delays += result.next.delay_set && input[7] < 2;
    }
    CHECK(fgets(marker, sizeof(marker), file));
    CHECK(strcmp(marker, "AB level ending reference complete\n") == 0);
    CHECK(fgetc(file) == EOF && !ferror(file));
    printf("All 4096 original ending-delay cases match: %zu win queries, "
           "%zu queued events, %zu extensions, %zu initial delays.\n",
           queries, queued, extensions, delays);
    return true;
}

int main(int argc, char **argv) {
    if (argc > 2) {
        fprintf(stderr, "Usage: ab_level_ending_tests [NATIVE_ENDING_TRACE]\n");
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
