#include "ab/ambient_particle.h"
#include "ab/ambient_particle_data.h"
#include "ab/coordinates.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Ambient particle failed at %d: %s\n", __LINE__,           \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static float from_bits(uint32_t bits) {
    float result;
    memcpy(&result, &bits, sizeof(result));
    return result;
}

static uint32_t to_bits(float number) {
    uint32_t result;
    memcpy(&result, &number, sizeof(result));
    return result;
}

static AbSpan effect(size_t index) {
    const char *const names[] = {"particle1", "particle2", "particle3"};
    return (AbSpan){(const uint8_t *)names[index], strlen(names[index])};
}

typedef struct {
    float scale;
    AbVec2 offset, point;
    uint32_t transforms, emissions, events[3][6];
    const AbAmbientParticle *observe;
    bool fail, coordinates, default_conversion;
} Host;

static bool transform(void *context, AbVec2 point, AbVec2 *output, AbError *error) {
    Host *host = context;
    CHECK(!host->observe || !host->observe->timer_present);
    ++host->transforms;
    host->point = point;
    if (host->coordinates) {
        CHECK(
            ab_coordinate_point(NULL, AB_SPACE_PHYSICS, AB_SPACE_WORLD, point, output));
    } else {
        *output = (AbVec2){point.x * host->scale + host->offset.x,
                           point.y * host->scale + host->offset.y};
    }
    return !host->fail || ab_fail(error, "Test ambient conversion failure");
}

static bool emit(void *context, const AbAmbientParticleEmission *emission,
                 AbError *error) {
    Host *host = context;
    (void)error;
    CHECK(host->emissions < 3 &&
          (host->default_conversion || host->transforms == host->emissions + 1));
    uint32_t *event = host->events[host->emissions++];
    for (size_t index = 0; index < 3; ++index) {
        AbSpan name = effect(index);
        if (name.size == emission->particles.size &&
            memcmp(name.data, emission->particles.data, name.size) == 0) {
            event[0] = (uint32_t)index + 1;
        }
    }
    CHECK(event[0]);
    event[1] = to_bits(emission->amount);
    event[2] = to_bits(host->point.x);
    event[3] = to_bits(host->point.y);
    event[4] = to_bits(emission->position.x);
    event[5] = to_bits(emission->position.y);
    return true;
}

static bool edge_checks(void) {
    AbAmbientParticle particle = {
        .particles = effect(0), .offset = {1, 2}, .amount = 3, .delay_present = true};
    AbVec2 position = {3, 4};
    Host host = {.scale = 1, .observe = &particle};
    AbAmbientParticleHost binding = {&host, transform, emit};
    CHECK(ab_ambient_particles_update(&particle, 1, 0, 0, &position, NULL, binding,
                                      NULL) &&
          !particle.timer_present && host.transforms == 1 && host.emissions == 1 &&
          host.events[0][1] == to_bits(3) && host.events[0][2] == to_bits(4) &&
          host.events[0][3] == to_bits(6));
    host = (Host){.default_conversion = true};
    CHECK(ab_ambient_particles_update(
              &particle, 1, 0, 0, &position, NULL,
              (AbAmbientParticleHost){.context = &host, .emit = emit}, NULL) &&
          host.emissions == 1 && !host.transforms && host.events[0][4] == to_bits(80) &&
          host.events[0][5] == to_bits(120));
    particle.timer = 1;
    particle.timer_present = true;
    CHECK(ab_ambient_particles_update(&particle, 1, 0, NAN, NULL, NULL,
                                      (AbAmbientParticleHost){0}, NULL) &&
          particle.timer_present && particle.timer == 1);
    AbRandom48 random, expected;
    CHECK(ab_random48_seed(&random, 17));
    expected = random;
    uint32_t ignored;
    CHECK(ab_random48_next(&expected, &ignored));
    particle.timer_present = false;
    particle.delay_present = false;
    host = (Host){.scale = 1, .observe = &particle, .fail = true};
    CHECK(!ab_ambient_particles_update(&particle, 1, 0, 0, &position, &random, binding,
                                       NULL) &&
          random.value == expected.value && !particle.timer_present &&
          host.transforms == 1 && !host.emissions);
    CHECK(ab_ambient_particles_update(NULL, 0, NAN, NAN, NULL, NULL,
                                      (AbAmbientParticleHost){0}, NULL));
    CHECK(
        !ab_ambient_particles_update(NULL, 1, 0, 0, &position, &random, binding, NULL));
    return true;
}

static bool read_words(FILE *file, uint32_t *words, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, words + index) == 1);
    }
    CHECK(fgetc(file) == '\n');
    return true;
}

static AbDataValue data_string(const char *text) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)text, strlen(text)}};
}

static AbDataValue data_number(float value) {
    return (AbDataValue){.kind = AB_DATA_NUMBER, .bits = to_bits(value)};
}

static bool data_checks(void) {
    AbDataEntry fields[] = {{data_string("particles"), data_string("particle1")},
                            {data_string("amount"), data_number(2)},
                            {data_string("x"), data_number(3)},
                            {data_string("y"), data_number(4)},
                            {data_string("delay"), data_number(0)},
                            {data_string("timer"), {.kind = AB_DATA_BOOLEAN}}};
    AbDataTable definition = {fields, 6, 6, NULL};
    AbDataEntry entry = {data_number(1), {.kind = AB_DATA_TABLE, .table = &definition}};
    AbDataTable component = {&entry, 1, 1, NULL};
    AbAmbientParticle particle;
    size_t order[] = {0}, count = 99;
    CHECK(ab_ambient_particle_data(&component, order, &particle, 1, &count, NULL) &&
          count == 1 && particle.amount == 2 && particle.offset.x == 3 &&
          particle.offset.y == 4 && !particle.timer_present && particle.delay_present &&
          to_bits(particle.delay) == 0);
    count = 99;
    order[0] = 1;
    CHECK(!ab_ambient_particle_data(&component, order, &particle, 1, &count, NULL) &&
          count == 99);
    order[0] = 0;
    fields[4].value = data_string("0");
    CHECK(!ab_ambient_particle_data(&component, order, &particle, 1, &count, NULL));
    CHECK(!ab_ambient_particle_data(&component, order, &particle, 0, &count, NULL));
    component.count = 0;
    CHECK(ab_ambient_particle_data(&component, NULL, NULL, 0, &count, NULL) && !count);
    return true;
}

static bool native_checks(FILE *file, bool coordinates) {
    char header[80];
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, coordinates
                             ? "AB_ANDROID_AMBIENT_PARTICLE_COORDINATES_V1 4096\n"
                             : "AB_ANDROID_AMBIENT_PARTICLE_V1 4096\n") == 0);
    size_t emissions = 0;
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[39];
        CHECK(read_words(file, input, 39) && input[0] <= 3);
        AbAmbientParticle particles[3] = {0};
        for (size_t index = 0; index < input[0]; ++index) {
            size_t base = 12 + index * 9;
            CHECK(input[base] <= 2 && input[base + 2] <= 2);
            particles[index] = (AbAmbientParticle){
                .particles = effect(index),
                .offset = {from_bits(input[base + 6]), from_bits(input[base + 7])},
                .amount = from_bits(input[base + 8]),
                .timer_present = input[base] == 2,
                .timer = from_bits(input[base + 1]),
                .delay_present = input[base + 2] == 2,
                .delay = from_bits(input[base + 3]),
                .min_delay = from_bits(input[base + 4]),
                .max_delay = from_bits(input[base + 5])};
        }
        AbVec2 position = {from_bits(input[3]), from_bits(input[4])};
        AbRandom48 random;
        CHECK(ab_random48_seed(&random, input[1]));
        for (size_t step = 0; step < 4; ++step) {
            uint32_t expected[24], actual[24] = {0};
            CHECK(read_words(file, expected, 24));
            Host host = {.scale = from_bits(input[5]),
                         .offset = {from_bits(input[6]), from_bits(input[7])},
                         .coordinates = coordinates};
            AbError error = {0};
            if (!ab_ambient_particles_update(
                    particles, input[0], from_bits(input[8 + step]),
                    from_bits(input[2]), &position, &random,
                    (AbAmbientParticleHost){&host, transform, emit}, &error)) {
                fprintf(stderr, "Native ambient particle %zu/%zu: %s\n", item, step,
                        error.message);
                return false;
            }
            actual[0] = host.transforms;
            actual[1] = host.emissions;
            emissions += host.emissions;
            for (size_t index = 0; index < input[0]; ++index) {
                if (particles[index].timer_present) {
                    actual[2] |= 1u << index;
                    actual[3 + index] = to_bits(particles[index].timer);
                }
            }
            for (size_t index = 0; index < host.emissions; ++index) {
                for (size_t field = 0; field < 6; ++field) {
                    actual[6 + index * 6 + field] = host.events[index][field];
                }
            }
            for (size_t field = 0; field < 24; ++field) {
                if (actual[field] != expected[field]) {
                    fprintf(stderr,
                            "Ambient particle %zu/%zu field %zu: %08" PRIx32
                            " instead of %08" PRIx32 "\n",
                            item, step, field, actual[field], expected[field]);
                    return false;
                }
            }
        }
        uint32_t expected, actual;
        CHECK(read_words(file, &expected, 1) && ab_random48_next(&random, &actual) &&
              actual == expected);
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, coordinates
                             ? "AB ambient particle coordinates reference complete\n"
                             : "AB ambient particle reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("All 4096 native ambient-particle sequences / 16384 updates match: %zu "
           "%s conversions and emissions\n",
           emissions, coordinates ? "original CoordinateTransforms" : "controlled");
    return true;
}

int main(int argc, char **argv) {
    bool coordinates = argc == 3 && strcmp(argv[1], "--coordinates") == 0;
    if (argc > 3 || (argc == 3 && !coordinates) || !edge_checks() || !data_checks()) {
        return 1;
    }
    if (argc >= 2) {
        FILE *file = fopen(argv[coordinates ? 2 : 1], "rb");
        bool okay = file && native_checks(file, coordinates);
        if (file && fclose(file) != 0) {
            okay = false;
        }
        return okay ? 0 : 1;
    }
    puts(
        "Ambient particle timers, ordered conversion/emission and failure checks pass");
    return 0;
}
