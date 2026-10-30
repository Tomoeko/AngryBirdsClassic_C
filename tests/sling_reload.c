#include "ab/slingshot.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Reload failed at line %d: %s\n", __LINE__, #condition);   \
            return false;                                                              \
        }                                                                              \
    } while (0)

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

static bool edge_checks(void) {
    const float gains[][3] = {{10, 0, 1},         {10, 5, 0.5f},    {10, 7.5f, 0.25f},
                              {10, 20, 0.25f},    {10, -5, 1},      {0, 0, 0.25f},
                              {0, 1, 0.25f},      {-0.0f, 1, 1},    {0, -1, 1},
                              {-0.0f, -1, 0.25f}, {-0.0f, 0, 0.25f}};
    for (size_t index = 0; index < sizeof(gains) / sizeof(*gains); ++index) {
        float gain = -1;
        CHECK(ab_slingshot_reload_gain(gains[index][0], gains[index][1], &gain) &&
              gain == gains[index][2]);
    }
    float gain = -1;
    CHECK(ab_slingshot_reload_gain(0x1p-149f, 0x1.fffffep127f, &gain) && gain == 0.25f);
    CHECK(!ab_slingshot_reload_gain(INFINITY, 0, &gain) && gain == 0.25f);
    CHECK(!ab_slingshot_reload_gain(0, NAN, &gain) && gain == 0.25f);
    CHECK(!ab_slingshot_reload_gain(1, 1, NULL));
    AbSlingReloadState state = {.timer = 1};
    AbSlingReloadGate output = {0}, before = output;
    CHECK(!ab_slingshot_reload_gate(NULL, 0, true, &output));
    CHECK(!ab_slingshot_reload_gate(&state, NAN, true, &output));
    CHECK(memcmp(&output, &before, sizeof(output)) == 0);
    state.timer = INFINITY;
    CHECK(!ab_slingshot_reload_gate(&state, 0, true, &output));
    CHECK(memcmp(&output, &before, sizeof(output)) == 0);
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
                          size_t count, size_t item) {
    for (size_t field = 0; field < count; ++field) {
        if (actual[field] != expected[field]) {
            fprintf(stderr, "Reload %zu field %zu: %08" PRIx32 " != %08" PRIx32 "\n",
                    item, field, actual[field], expected[field]);
            return false;
        }
    }
    return true;
}

static bool native_checks(FILE *file) {
    char marker[80];
    CHECK(fgets(marker, sizeof(marker), file));
    CHECK(strcmp(marker, "AB_ANDROID_SLING_RELOAD_V1 4096\n") == 0);
    size_t queries = 0, launches = 0, absent = 0;
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[5], expected[7], count = 0;
        CHECK(read_words(file, input, 5) && input[3] <= 7 && input[4] <= 2);
        CHECK(read_words(file, expected, 7) && read_words(file, &count, 1) &&
              count <= 3);
        AbSlingReloadState state = {.timer = from_bits(input[0]),
                                    .current_bird_index = from_bits(input[2]),
                                    .bird_fired = (input[3] & 1) != 0,
                                    .fill_in_next_bird = (input[3] & 2) != 0,
                                    .allow_reset_to_bird_camera = (input[3] & 4) != 0};
        float seconds = from_bits(input[1]);
        AbSlingReloadGate result;
        CHECK(ab_slingshot_reload_gate(&state, seconds, input[4] != 0, &result));
        uint32_t events[3][3];
        size_t event_count = 0;
        if (result.query_next_bird) {
            memcpy(events[event_count++],
                   (uint32_t[]){4, bits(result.queried_bird_index), 0},
                   sizeof(events[0]));
        }
        float timer = 0.25f;
        if (result.next.fill_in_next_bird) {
            AbSlingTransferInput transfer = {.start = {2, 3},
                                             .target = {-4, -5},
                                             .height = -1,
                                             .phase_angle = from_bits(0x4016cbe4),
                                             .time = timer,
                                             .rotation = 0.75f,
                                             .seconds = seconds};
            AbSlingTransferFrame frame;
            CHECK(ab_slingshot_transfer_frame(&transfer, &frame) && !frame.complete);
            timer = frame.next_time;
            memcpy(events[event_count++],
                   (uint32_t[]){1, bits(frame.position.x), bits(frame.position.y)},
                   sizeof(events[0]));
            memcpy(events[event_count++], (uint32_t[]){2, bits(frame.rotation), 0},
                   sizeof(events[0]));
        }
        uint32_t actual[] = {bits(result.next.timer),
                             result.next.bird_fired,
                             result.next.fill_in_next_bird,
                             result.next.allow_reset_to_bird_camera,
                             result.clear_launch_cue ? 0u : 3u,
                             result.clear_launch_cue ? 0u : UINT32_C(0x42280000),
                             bits(timer)};
        CHECK(compare_words(actual, expected, 7, item) && event_count == count);
        for (size_t event = 0; event < event_count; ++event) {
            uint32_t reference[3];
            CHECK(read_words(file, reference, 3));
            CHECK(compare_words(events[event], reference, 3, item));
        }
        queries += result.query_next_bird;
        launches += result.clear_launch_cue;
        absent += result.query_next_bird && input[4] == 0;
    }
    CHECK(fgets(marker, sizeof(marker), file));
    CHECK(strcmp(marker, "AB sling reload reference complete\n") == 0);
    CHECK(fgetc(file) == EOF && !ferror(file));
    printf("All 4096 original reload updates match: %zu next-bird queries, %zu "
           "launch-cue resets and %zu missing-next results.\n",
           queries, launches, absent);
    return true;
}

int main(int argc, char **argv) {
    if (argc > 2) {
        fprintf(stderr, "Usage: ab_sling_reload_tests [NATIVE_RELOAD_TRACE]\n");
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
