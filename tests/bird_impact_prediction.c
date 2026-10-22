#include "ab/bird_impact_prediction.h"
#include "bird_impact_prediction_cases.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(value)                                                                   \
    do {                                                                               \
        if (!(value)) {                                                                \
            fprintf(stderr, "Impact prediction check failed at %d: %s\n", __LINE__,    \
                    #value);                                                           \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    uint32_t raw[PREDICT_INPUT_WORDS];
    AbBirdImpactInput input;
    AbBirdImpactHost host;
    uint32_t events[PREDICT_MAX_EVENTS][PREDICT_EVENT_WORDS];
    size_t event_count, rays, radii, printed;
} PredictFixture;

static const char *const names[] = {"first", "second", "third"};

static float from_bits(uint32_t bits) {
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static uint32_t bits(float value) {
    uint32_t word;
    memcpy(&word, &value, sizeof(word));
    return word;
}

static AbDataValue name(size_t ordinal) {
    const char *value = names[ordinal % 3];
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)value, strlen(value)}};
}

static void state_words(const PredictFixture *fixture, uint32_t output[5]) {
    const AbBirdImpactInput *input = &fixture->input;
    output[0] = bits(input->position.x);
    output[1] = bits(input->position.y);
    output[2] = bits(input->velocity.x);
    output[3] = bits(input->velocity.y);
    output[4] = bits(input->gravity);
}

static uint32_t *event(PredictFixture *fixture, uint32_t kind, size_t ordinal) {
    if (fixture->event_count >= PREDICT_MAX_EVENTS) {
        return NULL;
    }
    uint32_t *output = fixture->events[fixture->event_count++];
    output[0] = kind;
    output[1] = (uint32_t)ordinal;
    state_words(fixture, output + 6);
    return output;
}

static bool radius(void *context, float *output, AbError *error) {
    (void)error;
    PredictFixture *fixture = context;
    uint32_t *record = event(fixture, 2, ++fixture->radii);
    CHECK(record);
    record[2] = 501;
    record[3] = 1;
    record[11] = fixture->raw[fixture->radii & 1u ? 12 : 13];
    if (fixture->radii == 1) {
        if (fixture->raw[1] & 1u) {
            fixture->input.position.x =
                from_bits(fixture->raw[4] ^ UINT32_C(0x80000000));
            fixture->input.position.y =
                from_bits(fixture->raw[5] ^ UINT32_C(0x80000000));
            fixture->input.velocity =
                (AbVec2){from_bits(fixture->raw[9]), from_bits(fixture->raw[10])};
            fixture->input.gravity = from_bits(fixture->raw[11]);
        }
        if (fixture->raw[1] & 2u) {
            fixture->host.radius = NULL;
        }
    }
    *output = from_bits(record[11]);
    return true;
}

static bool raycast(void *context, AbVec2 first, AbVec2 second,
                    AbBirdImpactRayHit *output, AbError *error) {
    (void)error;
    PredictFixture *fixture = context;
    uint32_t *record = event(fixture, 1, ++fixture->rays);
    CHECK(record);
    record[2] = bits(first.x);
    record[3] = bits(first.y);
    record[4] = bits(second.x);
    record[5] = bits(second.y);
    record[11] = bird_impact_prediction_hit(fixture->raw[14], (uint32_t)fixture->rays);
    uint32_t point[2];
    bird_impact_prediction_point(fixture->raw[0], (uint32_t)fixture->rays, point);
    *output = (AbBirdImpactRayHit){record[11] != 0,
                                   name(fixture->rays),
                                   {from_bits(point[0]), from_bits(point[1])}};
    if (fixture->rays == 1 && (fixture->raw[1] & 4u)) {
        fixture->host.raycast = NULL;
    }
    return true;
}

static bool print_count(void *context, size_t count, AbError *error) {
    (void)error;
    PredictFixture *fixture = context;
    CHECK(count == fixture->rays && event(fixture, 3, count));
    fixture->printed = count;
    fixture->input.gravity = from_bits(fixture->raw[11] ^ UINT32_C(0x80000000));
    return true;
}

static void prepare(PredictFixture *fixture) {
    fixture->input = (AbBirdImpactInput){
        .position = {from_bits(fixture->raw[4]), from_bits(fixture->raw[5])},
        .velocity = {from_bits(fixture->raw[6]), from_bits(fixture->raw[7])},
        .gravity = from_bits(fixture->raw[8]),
        .steps = from_bits(fixture->raw[3]),
        .steps_present = fixture->raw[2] == 2};
    fixture->host = (AbBirdImpactHost){fixture, 4096, radius, raycast,
                                       fixture->raw[1] & 8u ? NULL : print_count};
}

static bool words(FILE *file, const uint32_t *actual, size_t count, uint32_t sample) {
    for (size_t index = 0; index < count; ++index) {
        uint32_t expected;
        CHECK(fscanf(file, "%" SCNx32, &expected) == 1);
        if (actual[index] != expected) {
            fprintf(stderr,
                    "Impact prediction case %u word %zu: %08" PRIx32 " != %08" PRIx32
                    "\n",
                    sample, index, actual[index], expected);
            return false;
        }
    }
    return true;
}

static bool reference(FILE *file) {
    char marker[80];
    CHECK(fgets(marker, sizeof(marker), file) &&
          !strcmp(marker, "AB_ANDROID_BIRD_IMPACT_PREDICTION_V1 128\n"));
    size_t matched = 0, callbacks = 0, failures = 0;
    for (uint32_t sample = 0; sample < 128; ++sample) {
        PredictFixture fixture = {0};
        bird_impact_prediction_case(sample, fixture.raw);
        prepare(&fixture);
        CHECK(words(file, fixture.raw, PREDICT_INPUT_WORDS, sample));
        AbBirdImpactPrediction result = {0};
        AbError error = {0};
        uint32_t output[PREDICT_OUTPUT_WORDS] = {0};
        output[0] =
            !ab_bird_impact_predict(&fixture.input, &fixture.host, &result, &error);
        if (!output[0]) {
            output[1] = result.point_present;
            if (result.point_present) {
                output[2] = bits(result.point.x);
                output[3] = bits(result.point.y);
            }
            if (result.name.kind == AB_DATA_STRING) {
                output[4] = 4;
                for (size_t index = 0; index < 3; ++index) {
                    if (result.name.string.size == strlen(names[index]) &&
                        !memcmp(result.name.string.data, names[index],
                                result.name.string.size)) {
                        output[5] = (uint32_t)index + 1u;
                    }
                }
                CHECK(output[5]);
            } else {
                CHECK(result.name.kind == AB_DATA_NIL);
            }
            output[6] = bits(result.time);
            output[7] = (uint32_t)result.raycasts;
        }
        output[8] = (uint32_t)fixture.event_count;
        state_words(&fixture, output + 9);
        CHECK(words(file, output, PREDICT_OUTPUT_WORDS, sample));
        for (size_t index = 0; index < fixture.event_count; ++index) {
            CHECK(words(file, fixture.events[index], PREDICT_EVENT_WORDS, sample));
        }
        matched += PREDICT_INPUT_WORDS + PREDICT_OUTPUT_WORDS +
                   fixture.event_count * PREDICT_EVENT_WORDS;
        callbacks += fixture.event_count;
        failures += output[0];
    }
    CHECK(fscanf(file, " %79[^\n]", marker) == 1 &&
          !strcmp(marker, "AB bird impact prediction reference complete"));
    CHECK(fgetc(file) == '\n' && fgetc(file) == EOF && !ferror(file));
    printf("All 128 original impact predictions match: %zu words, %zu checked "
           "callback boundaries, %zu partial failures.\n",
           matched, callbacks, failures);
    return true;
}

static bool controls(void) {
    PredictFixture fixture = {0};
    bird_impact_prediction_case(0, fixture.raw);
    prepare(&fixture);
    fixture.host.maximum_raycasts = 1;
    AbBirdImpactPrediction result = {
        .time = 17, .raycasts = 999, .point_present = true};
    CHECK(!ab_bird_impact_predict(&fixture.input, &fixture.host, &result, NULL));
    CHECK(result.time == 17 && result.raycasts == 999 && result.point_present &&
          fixture.rays == 1 && fixture.radii == 1 && !fixture.printed);
    fixture = (PredictFixture){0};
    bird_impact_prediction_case(0, fixture.raw);
    fixture.raw[14] = 1;
    prepare(&fixture);
    CHECK(ab_bird_impact_predict(&fixture.input, &fixture.host, &result, NULL));
    uint32_t expected[2];
    bird_impact_prediction_point(0, 10, expected);
    CHECK(result.point_present && result.raycasts == 12 && fixture.radii == 8 &&
          result.time == 0 && bits(result.point.x) == expected[0] &&
          bits(result.point.y) == expected[1]);
    return true;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        return controls() ? 0 : 1;
    }
    if (argc != 2) {
        return 2;
    }
    FILE *file = fopen(argv[1], "rb");
    if (!file) {
        return 1;
    }
    bool okay = reference(file);
    if (fclose(file)) {
        okay = false;
    }
    return okay ? 0 : 1;
}
