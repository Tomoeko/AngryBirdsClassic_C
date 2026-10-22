#include "ab/random.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Random check failed at line %d: %s\n", __LINE__,          \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static float number(uint32_t bits) {
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static uint32_t raw(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static bool edge_checks(void) {
    AbRandom48 state;
    CHECK(!ab_random48_seed_time(NULL, 0));
    state.value = 99;
    CHECK(!ab_random48_seed_time(&state, -1) && state.value == 99);
    CHECK(ab_random48_seed_time(&state, 16777217) &&
          state.value == UINT64_C(0x1000000330e));
    CHECK(ab_random48_seed_time(&state, INT32_MAX) &&
          state.value == UINT64_C(0x7fffffff330e));
    CHECK(ab_random48_seed_time(&state, INT32_MIN) &&
          state.value == UINT64_C(0x80000000330e));
    CHECK(ab_random48_seed(&state, UINT32_MAX));
    CHECK(state.value == UINT64_C(0xffffffff330e));
    CHECK(!ab_random48_seed(NULL, 1));
    state.value = UINT64_C(0x1000000000000);
    uint32_t word = 99;
    CHECK(!ab_random48_next(&state, &word) && word == 99 &&
          state.value == UINT64_C(0x1000000000000));
    float output = -99;
    CHECK(ab_script_random(&state, 0, 0, 0, &output) ==
          AB_SCRIPT_RANDOM_INVALID_STORAGE);
    CHECK(output == -99 && state.value == UINT64_C(0x1000000000000));
    CHECK(ab_random48_seed(&state, 1));
    AbRandom48 initial = state, expected = state;
    CHECK(ab_random48_next(&expected, &word));
    CHECK(ab_script_random(&state, 1, 0, 0, &output) ==
          AB_SCRIPT_RANDOM_EMPTY_INTERVAL);
    CHECK(state.value == expected.value && output == -99);
    state = initial;
    CHECK(ab_script_random(&state, 3, 1, 36, &output) ==
          AB_SCRIPT_RANDOM_ARGUMENT_COUNT);
    CHECK(state.value == expected.value && output == -99);
    CHECK(ab_script_random(&state, 0, 0, 0, NULL) == AB_SCRIPT_RANDOM_INVALID_STORAGE);
    CHECK(state.value == expected.value);
    const uint64_t mask = UINT64_C(0xffffffffffff);
    uint64_t desired = UINT64_C(0x7ffffffe) << 17;
    state.value = ((desired - 11) * UINT64_C(0xdfe05bcb1365)) & mask;
    CHECK(ab_script_random(&state, 1, 1, 0, &output) == AB_SCRIPT_RANDOM_OK);
    CHECK(output == 2 && state.value == desired);
    desired = UINT64_C(0x7fffffff) << 17;
    state.value = ((desired - 11) * UINT64_C(0xdfe05bcb1365)) & mask;
    CHECK(ab_script_random(&state, 1, 1, 0, &output) == AB_SCRIPT_RANDOM_OK);
    CHECK(output == 1 && state.value == desired);
    return true;
}

static bool read_words(FILE *file, uint32_t *words, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%8" SCNx32, words + index) == 1);
    }
    return true;
}

static bool compare_reference(FILE *file) {
    char header[80];
    CHECK(fgets(header, sizeof(header), file));
    CHECK(strcmp(header, "AB_ANDROID_RANDOM_V1 4096 65536\n") == 0);
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[21];
        CHECK(read_words(file, input, 21));
        AbRandom48 state;
        CHECK(ab_random48_seed(&state, input[0]));
        CHECK((uint32_t)state.value == input[1] && state.value >> 32 == input[2]);
        for (size_t draw = 0; draw < 16; ++draw) {
            uint32_t actual;
            CHECK(ab_random48_next(&state, &actual));
            CHECK(actual == input[3 + draw]);
        }
        CHECK((uint32_t)state.value == input[19] && state.value >> 32 == input[20]);
    }
    size_t succeeded = 0, interval_errors = 0, count_errors = 0;
    for (size_t item = 0; item < 65536; ++item) {
        uint32_t input[5], expected[5];
        CHECK(read_words(file, input, 5));
        CHECK(read_words(file, expected, 5));
        CHECK(input[1] <= 65535 && input[2] <= 3 && expected[1] <= 2);
        AbRandom48 state = {(uint64_t)input[1] << 32 | input[0]};
        AbRandom48 clone = state;
        uint32_t draw;
        CHECK(ab_random48_next(&clone, &draw) && draw == expected[0]);
        float first = number(input[3]), second = number(input[4]);
        CHECK(isfinite(first) && isfinite(second) && first >= -0x1p31f &&
              first < 0x1p31f && second >= -0x1p31f && second < 0x1p31f);
        float output = number(UINT32_C(0xdeadbeef));
        AbScriptRandomResult result = ab_script_random(&state, input[2], (int32_t)first,
                                                       (int32_t)second, &output);
        uint32_t actual[] = {(uint32_t)result, raw(output), (uint32_t)state.value,
                             (uint32_t)(state.value >> 32)};
        for (size_t field = 0; field < 4; ++field) {
            if (actual[field] != expected[1 + field]) {
                fprintf(stderr,
                        "Random case %zu field %zu: %08" PRIx32 " expected %08" PRIx32
                        "\n",
                        item, field, actual[field], expected[1 + field]);
                return false;
            }
        }
        succeeded += result == AB_SCRIPT_RANDOM_OK;
        interval_errors += result == AB_SCRIPT_RANDOM_EMPTY_INTERVAL;
        count_errors += result == AB_SCRIPT_RANDOM_ARGUMENT_COUNT;
    }
    CHECK(fgetc(file) == '\n');
    CHECK(fgets(header, sizeof(header), file));
    CHECK(strcmp(header, "AB random reference complete\n") == 0);
    CHECK(fgetc(file) == EOF && !ferror(file));
    printf("Matched 65536 native generator draws and 65536 script calls: %zu values, "
           "%zu interval errors, %zu argument-count errors\n",
           succeeded, interval_errors, count_errors);
    return true;
}

static bool compare_live_reference(FILE *file) {
    size_t count;
    CHECK(fscanf(file, "AB_LIVE_RANDOM_V1 %zu", &count) == 1 && count > 0 &&
          count <= 256);
    size_t direct = 0, shared = 0;
    for (size_t item = 0; item < count; ++item) {
        uint32_t input[9];
        CHECK(read_words(file, input, 9));
        CHECK(input[1] <= 65535 && input[2] <= 2 && input[7] <= 65535 &&
              input[8] <= 255);
        AbRandom48 state = {(uint64_t)input[1] << 32 | input[0]};
        float first = number(input[3]), second = number(input[4]);
        CHECK(isfinite(first) && isfinite(second) && first >= -0x1p31f &&
              first < 0x1p31f && second >= -0x1p31f && second < 0x1p31f);
        float output;
        CHECK(ab_script_random(&state, input[2], (int32_t)first, (int32_t)second,
                               &output) == AB_SCRIPT_RANDOM_OK);
        CHECK(raw(output) == input[5]);
        /* The capture includes one window with additional shared-generator
         * advances. Their count is explicit input; their callers/order are open. */
        for (size_t extra = 0; extra < input[8]; ++extra) {
            uint32_t discarded;
            CHECK(ab_random48_next(&state, &discarded));
        }
        CHECK((uint32_t)state.value == input[6] && state.value >> 32 == input[7]);
        direct += input[8] == 0;
        shared += input[8] != 0;
    }
    CHECK(fgetc(file) == '\n' && fgetc(file) == EOF && !ferror(file));
    printf("Matched %zu live values and %zu direct state pairs; %zu shared windows "
           "retain explicit unobserved-consumption counts\n",
           count, direct, shared);
    return true;
}

static bool compare_startup_seed(FILE *file) {
    char header[80];
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB_ANDROID_RANDOM_STARTUP_SEED_V1 65536\n") == 0);
    uint32_t clock[4];
    CHECK(read_words(file, clock, 4) && clock[0] == UINT32_C(0x770ef4));
    int32_t before, after;
    memcpy(&before, clock + 1, sizeof(before));
    memcpy(&after, clock + 3, sizeof(after));
    CHECK(before != -1 && after != -1 &&
          (raw((float)before) == clock[2] || raw((float)after) == clock[2]));
    AbRandom48 state = {0};
    for (size_t item = 0; item < 65536; ++item) {
        uint32_t input[8];
        CHECK(read_words(file, input, 8) && (input[2] == 0 || input[2] == 2));
        int32_t epoch;
        memcpy(&epoch, input, sizeof(epoch));
        CHECK(raw((float)epoch) == input[1]);
        uint64_t previous = state.value;
        bool okay = ab_random48_seed_time(&state, epoch);
        CHECK(okay == (input[2] == 0) && (okay || state.value == previous));
        CHECK((uint32_t)state.value == input[3] && state.value >> 32 == input[4]);
        uint32_t draw;
        CHECK(ab_random48_next(&state, &draw) && draw == input[5]);
        CHECK((uint32_t)state.value == input[6] && state.value >> 32 == input[7]);
    }
    CHECK(fgetc(file) == '\n' && fgets(header, sizeof(header), file) &&
          strcmp(header, "AB startup seed reference complete\n") == 0);
    CHECK(fgetc(file) == EOF && !ferror(file));
    puts("Matched 65536 native startup seed conversions/states/draws and an original "
         "zero-argument os.time boundary");
    return true;
}

int main(int argc, char **argv) {
    if (!edge_checks()) {
        return 1;
    }
    if (argc == 1) {
        return 0;
    }
    bool live = argc == 3 && strcmp(argv[1], "--live") == 0;
    bool startup = argc == 3 && strcmp(argv[1], "--startup-seed") == 0;
    if (argc != 2 && !live && !startup) {
        return 2;
    }
    FILE *file = fopen(argv[live || startup ? 2 : 1], "rb");
    if (!file) {
        return 1;
    }
    bool okay = startup ? compare_startup_seed(file)
                : live  ? compare_live_reference(file)
                        : compare_reference(file);
    if (fclose(file)) {
        okay = false;
    }
    return okay ? 0 : 1;
}
