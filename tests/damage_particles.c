#include "ab/damage_particles.h"
#include "damage_particles_cases.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Damage particles failed at %d: %s\n", __LINE__,           \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    AbDataEntry descriptor_entries[2], alternate_entries[2], raw_entry;
    AbDataTable descriptor, alternate, raw;
    AbVec2 position;
    uint32_t input[10], events[2][6], count, fail_at;
    size_t words;
} ParticleHost;

static AbDataValue text(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)value, strlen(value)}};
}

static uint32_t bits(float value) {
    uint32_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static float number(uint32_t value) {
    float result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static void prepare(ParticleHost *host, uint32_t sample) {
    *host = (ParticleHost){0};
    damage_particles_case(sample, host->input);
    host->position = (AbVec2){number(host->input[2]), number(host->input[3])};
    host->descriptor_entries[0] =
        (AbDataEntry){text("name"), text(host->input[9] ? "second" : "first")};
    host->descriptor_entries[1] =
        (AbDataEntry){text("amount"), {.kind = AB_DATA_NUMBER, .bits = host->input[6]}};
    host->descriptor = (AbDataTable){host->descriptor_entries, 2, 2, NULL};
    host->alternate_entries[0] = (AbDataEntry){text("name"), text("alternate")};
    host->alternate_entries[1] = (AbDataEntry){
        text("amount"), {.kind = AB_DATA_NUMBER, .bits = UINT32_C(0x40e00000)}};
    host->alternate = (AbDataTable){host->alternate_entries, 2, 2, NULL};
    AbDataValue descriptor = {0};
    switch (host->input[1]) {
        case 1:
            descriptor =
                (AbDataValue){.kind = AB_DATA_TABLE, .table = &host->descriptor};
            break;
        case 2:
            descriptor.kind = AB_DATA_BOOLEAN;
            break;
        case 3:
            descriptor = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1};
            break;
        case 4:
            descriptor.kind = AB_DATA_NUMBER;
            break;
        case 5:
            descriptor = text("invalid");
            break;
        default:
            break;
    }
    host->raw_entry = (AbDataEntry){text("damageStateChangedParticles"), descriptor};
    host->raw = (AbDataTable){&host->raw_entry, 1, 1, NULL};
}

static bool transform(void *context, AbVec2 point, AbVec2 *output, AbError *error) {
    ParticleHost *host = context;
    CHECK(!host->count);
    uint32_t *event = host->events[host->count++];
    event[0] = 1;
    event[1] = bits(point.x);
    event[2] = bits(point.y);
    event[3] = host->input[4];
    event[4] = host->input[5];
    if (host->fail_at == 1) {
        return ab_fail(error, "Controlled particle transform failure");
    }
    if (host->input[1] == 1) {
        if (host->input[8] & 1u) {
            host->descriptor_entries[0].value = text("second");
        }
        if (host->input[8] & 2u) {
            host->descriptor_entries[1].value.bits = host->input[7];
        }
        if (host->input[8] & 4u) {
            host->raw_entry.value =
                (AbDataValue){.kind = AB_DATA_TABLE, .table = &host->alternate};
        }
    }
    *output = (AbVec2){number(host->input[4]), number(host->input[5])};
    return true;
}

static bool emit(void *context, const AbAmbientParticleEmission *emission,
                 AbError *error) {
    ParticleHost *host = context;
    CHECK(host->count < 2 && emission->particles.data);
    uint32_t *event = host->events[host->count++];
    event[0] = 2;
    if (emission->particles.size == 5 &&
        memcmp(emission->particles.data, "first", 5) == 0) {
        event[1] = 1;
    } else {
        CHECK(emission->particles.size == 6 &&
              memcmp(emission->particles.data, "second", 6) == 0);
        event[1] = 2;
    }
    event[2] = bits(emission->amount);
    event[3] = bits(emission->position.x);
    event[4] = bits(emission->position.y);
    return host->fail_at != 2 || ab_fail(error, "Controlled particle emission failure");
}

static bool compare(FILE *file, ParticleHost *host, const char *section,
                    const uint32_t *actual, size_t count) {
    for (size_t field = 0; field < count; ++field) {
        uint32_t expected;
        CHECK(fscanf(file, "%" SCNx32, &expected) == 1);
        if (expected != actual[field]) {
            fprintf(stderr,
                    "Particle case %" PRIu32 " %s[%zu]: C %08" PRIx32
                    ", native %08" PRIx32 "\n",
                    host->input[0], section, field, actual[field], expected);
            return false;
        }
    }
    CHECK(fgetc(file) == '\n');
    host->words += count;
    return true;
}

static bool native_checks(FILE *file) {
    char line[100];
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB_ANDROID_DAMAGE_PARTICLES_V1 4096\n") == 0);
    size_t words = 0, conversions = 0, emissions = 0, failures = 0;
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        ParticleHost host;
        prepare(&host, sample);
        CHECK(compare(file, &host, "input", host.input, 10));
        AbAmbientParticleHost callbacks = {&host, transform, emit};
        bool okay =
            ab_damage_state_particles(&host.raw, &host.position, callbacks, NULL);
        uint32_t result[] = {!okay, host.count};
        CHECK(compare(file, &host, "result", result, 2));
        failures += !okay;
        for (size_t event = 0; event < host.count; ++event) {
            CHECK(compare(file, &host, "event", host.events[event], 6));
            conversions += host.events[event][0] == 1;
            emissions += host.events[event][0] == 2;
        }
        words += host.words;
    }
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB damage particles reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf(
        "All 4,096 original damage-particle helpers match: %zu words, %zu "
        "conversions, %zu emissions and %zu ordered malformed-descriptor failures.\n",
        words, conversions, emissions, failures);
    return true;
}

static bool edge_checks(void) {
    ParticleHost host;
    prepare(&host, 0);
    CHECK(ab_damage_state_particles(&host.raw, NULL, (AbAmbientParticleHost){0}, NULL));
    for (uint32_t failure = 1; failure <= 2; ++failure) {
        prepare(&host, 1);
        host.fail_at = failure;
        AbError error = {0};
        CHECK(!ab_damage_state_particles(
                  &host.raw, &host.position,
                  (AbAmbientParticleHost){&host, transform, emit}, &error) &&
              host.count == failure && error.message[0]);
    }
    prepare(&host, 1);
    host.position = (AbVec2){1, -2};
    CHECK(ab_damage_state_particles(&host.raw, &host.position,
                                    (AbAmbientParticleHost){&host, NULL, emit}, NULL) &&
          host.count == 1 && host.events[0][0] == 2 && host.events[0][3] == bits(20) &&
          host.events[0][4] == bits(-40));
    prepare(&host, 1);
    host.descriptor_entries[1].value.bits = UINT32_C(0x7fc00000);
    CHECK(!ab_damage_state_particles(&host.raw, &host.position,
                                     (AbAmbientParticleHost){&host, transform, emit},
                                     NULL) &&
          host.count == 1);
    return true;
}

int main(int argc, char **argv) {
    if (argc > 2) {
        return 2;
    }
    bool okay = edge_checks();
    FILE *file = argc == 2 ? fopen(argv[1], "rb") : NULL;
    if (argc == 2) {
        okay = okay && file && native_checks(file);
    }
    if (file && fclose(file) != 0) {
        okay = false;
    }
    return okay ? 0 : 1;
}
