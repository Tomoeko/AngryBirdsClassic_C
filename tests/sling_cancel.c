#include "ab/slingshot_control.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Cancel failed at line %d: %s\n", __LINE__, #condition);   \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    AbSlingBandState band;
    AbShotState shot;
    AbShotBird bird;
    uint32_t events[2][14];
    size_t event_count;
    AbSlingControlOperation fail_at;
    bool laser_playing;
} CancelCase;

static float from_bits(uint32_t bits) {
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static uint32_t bits(float value) {
    uint32_t output;
    memcpy(&output, &value, sizeof(output));
    return output;
}

static void snapshot(const CancelCase *test, uint32_t output[11]) {
    const AbSlingBandState *band = &test->band;
    const float values[] = {band->rubber_band.x, band->rubber_band.y,
                            band->angle,         band->distance,
                            band->wobble,        band->previous_distance};
    for (size_t field = 0; field < 6; ++field) {
        output[field] = bits(values[field]);
    }
    output[6] = test->shot.selection_played;
    output[7] = band->scope_sound_eligible;
    output[8] = test->shot.dragged != NULL;
    output[9] = bits(test->bird.position.x);
    output[10] = bits(test->bird.position.y);
}

static bool host(void *context, AbSlingControlCall *call, AbError *error) {
    CancelCase *test = context;
    CHECK(test->event_count < 2);
    uint32_t *event = test->events[test->event_count++];
    event[0] = call->operation;
    event[1] = bits(call->position.x);
    event[2] = bits(call->position.y);
    snapshot(test, event + 3);
    if (call->operation == test->fail_at) {
        return ab_fail(error, "Requested cancellation boundary failure");
    }
    switch (call->operation) {
        case AB_SLING_CONTROL_SET_POSITION:
            CHECK(call->bird == &test->bird);
            call->bird->position = call->position;
            return true;
        case AB_SLING_CONTROL_QUERY_LASER:
            CHECK(!call->bird);
            call->audio_playing = test->laser_playing;
            return true;
        default:
            CHECK(false);
    }
}

static void prepare(CancelCase *test, const uint32_t input[12]) {
    test->band =
        (AbSlingBandState){.rest = {from_bits(input[0]), from_bits(input[1])},
                           .rubber_band = {from_bits(input[2]), from_bits(input[3])},
                           .angle = from_bits(input[4]),
                           .distance = from_bits(input[5]),
                           .wobble = from_bits(input[6]),
                           .previous_distance = from_bits(input[7]),
                           .scope_sound_eligible = input[9] != 0};
    test->bird =
        (AbShotBird){.name = "first", .mass = 1, .position = test->band.rubber_band};
    test->shot = (AbShotState){.dragged = input[10] ? &test->bird : NULL,
                               .selection_played = input[8] != 0,
                               .current_bird_name = "first",
                               .bird_ready = true};
    test->laser_playing = input[11] != 0;
}

static bool edge_checks(void) {
    uint32_t input[12] = {0};
    input[0] = bits(2);
    input[2] = bits(5);
    input[4] = bits(1);
    input[8] = input[10] = input[11] = 1;
    CancelCase test = {0};
    prepare(&test, input);
    test.fail_at = AB_SLING_CONTROL_SET_POSITION;
    AbError error = {0};
    CHECK(!ab_slingshot_cancel_drag(&test.band, &test.shot, host, &test, &error));
    CHECK(test.event_count == 1 && test.band.rubber_band.x == 5 && test.shot.dragged &&
          test.shot.selection_played);
    test = (CancelCase){0};
    prepare(&test, input);
    test.fail_at = AB_SLING_CONTROL_QUERY_LASER;
    CHECK(!ab_slingshot_cancel_drag(&test.band, &test.shot, host, &test, &error));
    CHECK(test.event_count == 2 && test.band.rubber_band.x == 2 && !test.shot.dragged &&
          !test.shot.selection_played && !test.band.scope_sound_eligible);
    CHECK(test.shot.bird_ready && strcmp(test.shot.current_bird_name, "first") == 0);
    test = (CancelCase){0};
    prepare(&test, input);
    test.band.rest.y = NAN;
    CHECK(!ab_slingshot_cancel_drag(&test.band, &test.shot, host, &test, &error));
    CHECK(test.event_count == 0 && test.band.rubber_band.x == 5);
    CHECK(!ab_slingshot_reset_band(NULL, host, &test, &error));
    return true;
}

static bool read_words(FILE *file, uint32_t *output, size_t count) {
    for (size_t field = 0; field < count; ++field) {
        CHECK(fscanf(file, "%8" SCNx32, output + field) == 1);
    }
    CHECK(fgetc(file) == '\n');
    return true;
}

static bool compare(const uint32_t *actual, const uint32_t *expected, size_t count,
                    size_t item, size_t event) {
    for (size_t field = 0; field < count; ++field) {
        if (actual[field] != expected[field]) {
            fprintf(stderr,
                    "Cancel %zu event %zu field %zu: %08" PRIx32 " != %08" PRIx32 "\n",
                    item, event, field, actual[field], expected[field]);
            return false;
        }
    }
    return true;
}

static bool native_checks(FILE *file) {
    char marker[80];
    CHECK(fgets(marker, sizeof(marker), file));
    CHECK(strcmp(marker, "AB_ANDROID_SLING_CANCEL_V1 4096\n") == 0);
    size_t events = 0;
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[12], expected[14], actual[11], count;
        CHECK(read_words(file, input, 12));
        for (size_t field = 8; field < 12; ++field) {
            CHECK(input[field] <= 1);
        }
        CancelCase test = {0};
        prepare(&test, input);
        AbError error = {0};
        CHECK(ab_slingshot_cancel_drag(&test.band, &test.shot, host, &test, &error));
        snapshot(&test, actual);
        CHECK(read_words(file, expected, 11) &&
              compare(actual, expected, 11, item, SIZE_MAX));
        CHECK(read_words(file, &count, 1) && count == test.event_count);
        for (size_t event = 0; event < count; ++event) {
            CHECK(read_words(file, expected, 14));
            CHECK(compare(test.events[event], expected, 14, item, event));
        }
        events += count;
    }
    CHECK(fgets(marker, sizeof(marker), file));
    CHECK(strcmp(marker, "AB sling cancel reference complete\n") == 0);
    CHECK(fgetc(file) == EOF && !ferror(file));
    printf("All 4096 original drag cancellations, band resets and %zu ordered "
           "position/audio-query boundaries match.\n",
           events);
    return true;
}

int main(int argc, char **argv) {
    if (argc > 2) {
        fprintf(stderr, "Usage: ab_sling_cancel_tests [NATIVE_SLING_CANCEL_TRACE]\n");
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
