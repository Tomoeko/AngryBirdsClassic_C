#include "ab/particles.h"
#include "ab/target_resources.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Particle check at %d: %s\n", __LINE__, #condition);       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static AbSpan span(const char *text) {
    return (AbSpan){(const uint8_t *)text, strlen(text)};
}

static uint32_t bits(float value) {
    uint32_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static uint32_t sprite_id(AbSpan sprite) {
    const char *const names[] = {"RED_FEATHER_1", "RED_FEATHER_2", "RED_FEATHER_3",
                                 "SMOKE_BUFF_1",  "SMOKE_BUFF_2",  "INGAME_BIRDS_1",
                                 "SMOKE_CLOUD_1", "SMOKE_CLOUD_2", "SMOKE_CLOUD_3",
                                 "SMOKE_CLOUD_4", "SMOKE_CLOUD_5", "SMOKE_CLOUD_6"};
    for (uint32_t index = 0; index < 12; ++index) {
        if (sprite.size == strlen(names[index]) &&
            !memcmp(sprite.data, names[index], sprite.size))
            return index + 1;
    }
    return 0;
}

/* Match the probe's explicit prepared non-null ordinary resource boundary.
 * Asset decoding/rendering has separate evidence; this control checks the
 * particle engine's names, state, numeric sequence and lifecycle. */
static bool sprite(void *context, AbSpan name, uintptr_t *output, AbError *error) {
    (void)context;
    CHECK(sprite_id(name));
    (void)error;
    *output = 1;
    return true;
}

static bool read_words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index)
        CHECK(fscanf(file, "%" SCNx32, output + index) == 1);
    return true;
}

#include "particle_draw.h"
#include "classic_frame_particles.h"

static bool compare_snapshot(AbParticles *owner, FILE *file, uint32_t scene,
                             uint32_t phase, uint32_t seconds, bool level) {
    uint32_t header[4];
    CHECK(read_words(file, header, 4) && header[0] == scene && header[1] == phase &&
          header[2] == seconds &&
          header[3] == (level ? ab_particles_level_count(owner)
                              : ab_particles_count(owner, false)));
    for (size_t index = 0; index < header[3]; ++index) {
        const AbParticle *particle = level ? ab_particles_level_at(owner, index)
                                           : ab_particles_at(owner, false, index);
        CHECK(particle);
        uint32_t expected[17], actual[] = {sprite_id(particle->sprite),
                                           bits(particle->position.x),
                                           bits(particle->position.y),
                                           bits(particle->velocity.x),
                                           bits(particle->velocity.y),
                                           (uint32_t)particle->layer,
                                           bits(particle->angle),
                                           bits(particle->angular_velocity),
                                           bits(particle->begin_scale),
                                           bits(particle->end_scale),
                                           bits(particle->scale),
                                           particle->sprite_handle != 0,
                                           0,
                                           bits(particle->time),
                                           bits(particle->lifetime),
                                           particle->frame,
                                           particle->animated};
        CHECK(read_words(file, expected, 17));
        for (size_t word = 0; word < 17; ++word) {
            if (actual[word] != expected[word]) {
                fprintf(stderr,
                        "Particle scene %u phase %u row %zu word %zu: "
                        "%08" PRIx32 " != %08" PRIx32 "\n",
                        scene, phase, index, word, actual[word], expected[word]);
                return false;
            }
        }
    }
    return true;
}

static bool compare(AbParticles *owner, AbEngineRandom *random, FILE *file) {
    char header[64];
    CHECK(fgets(header, sizeof(header), file));
    size_t scenes = 0;
    if (!strcmp(header, "AB_ANDROID_PARTICLE_BOX_V1 2 6\n"))
        scenes = 2;
    if (!strcmp(header, "AB_ANDROID_PARTICLE_COMMON_V1 3 6\n"))
        scenes = 3;
    bool level_drawing = !strcmp(header, "AB_ANDROID_PARTICLE_LEVEL_DRAW_V1 3 6\n");
    bool menu_drawing = !strcmp(header, "AB_ANDROID_PARTICLE_MENU_DRAW_V1 3 6\n");
    bool drawing = menu_drawing || level_drawing ||
                   !strcmp(header, "AB_ANDROID_PARTICLE_DRAW_V1 3 6\n");
    bool level = level_drawing || !strcmp(header, "AB_ANDROID_PARTICLE_LEVEL_V1 3 6\n");
    if (drawing || level)
        scenes = 3;
    CHECK(scenes);
    const uint32_t durations[] = {0, 0, 0x3e800000, 0x3f400000, 0, 0x3e000000};
    const uint32_t animated_durations[] = {0,          0x3d75c28f, 0x3d75c28f,
                                           0x3df5c28f, 0x3de147ae, 0x3c23d70a};
    const char *const names[] = {"redBuff", "smokeBuffBirdsDefault", "smokeBuff"};
    for (uint32_t scene = 0; scene < scenes; ++scene) {
        CHECK(ab_engine_random_seed(random, scene) &&
              !ab_particles_count(owner, false) && !ab_particles_level_count(owner));
        AbParticleEmission emission = {.definition = span(names[scene]),
                                       .amount = 1.5f,
                                       .position = {10, -20},
                                       .size = {4, 6},
                                       .angle = 0.3f,
                                       .alternate = menu_drawing};
        if (level) {
            AbParticleEmission zero = emission;
            zero.amount = 0;
            CHECK(ab_particles_emit_level(owner, &zero, NULL) &&
                  ab_particles_emit_level(owner, &emission, NULL));
        } else {
            CHECK(ab_particles_emit(owner, &emission, NULL));
        }
        for (uint32_t phase = 0; phase < 6; ++phase) {
            float seconds;
            uint32_t duration =
                scene == 2 ? animated_durations[phase] : durations[phase];
            memcpy(&seconds, &duration, sizeof(seconds));
            if (phase)
                CHECK(level ? ab_particles_update_level(owner, seconds, NULL)
                            : ab_particles_update(owner, seconds, menu_drawing, NULL));
            if (level_drawing) {
                for (int32_t layer = 0; layer < 3; ++layer)
                    CHECK(compare_draw_snapshot(owner, file, scene, phase, duration,
                                                true, false, layer));
            } else {
                CHECK(drawing ? compare_draw_snapshot(owner, file, scene, phase,
                                                      duration, false, menu_drawing, 0)
                              : compare_snapshot(owner, file, scene, phase, duration,
                                                 level));
            }
        }
        if (menu_drawing)
            CHECK(ab_particles_update(owner, 100, true, NULL) &&
                  !ab_particles_count(owner, true));
        if (level) {
            uint32_t expected;
            double following;
            CHECK(read_words(file, &expected, 1) &&
                  ab_engine_random_next(random, &following) &&
                  bits((float)following) == expected &&
                  !ab_particles_count(owner, false) &&
                  !ab_particles_count(owner, true));
        }
    }
    char marker[64];
    CHECK(fscanf(file, "\n%63[^\n]", marker) == 1 &&
          !strcmp(marker, menu_drawing    ? "AB particle menu draw reference complete"
                          : level_drawing ? "AB particle level draw reference complete"
                          : level         ? "AB particle level reference complete"
                          : drawing       ? "AB particle draw reference complete"
                          : scenes == 2   ? "AB particle box reference complete"
                                          : "AB particle common reference complete"));
    int trailing;
    while ((trailing = fgetc(file)) != EOF)
        CHECK(trailing == '\n' || trailing == '\r');
    CHECK(!ferror(file));
    puts("Owned red-feather/smoke particle creation, 17 state words and strict "
         "lifecycle match original ARMv7 snapshots through six phases");
    return true;
}

static bool controls(void) {
    AbParticles *owner = NULL;
    AbEngineRandom random = {0};
    AbParticleConfig config = {.random = &random,
                               .capacity = 1,
                               .storage_bytes = 4096,
                               .admission = {40, 125, 0.5f}};
    CHECK(!ab_particles_open(config, NULL, NULL));
    config.storage_bytes = 1;
    CHECK(!ab_particles_open(config, &owner, NULL) && !owner);
    config.storage_bytes = 4096;
    CHECK(ab_particles_open(config, &owner, NULL) &&
          ab_particles_bytes(owner) <= 4096 && !random.initialized &&
          !ab_particles_count(owner, false) && !ab_particles_at(owner, false, 0));
    AbParticleEmission unused = {.amount = 0};
    CHECK(ab_particles_emit(owner, &unused, NULL) && !random.initialized);
    CHECK(ab_particles_emit_level(owner, &unused, NULL) && random.initialized &&
          !ab_particles_level_count(owner) && !ab_particles_level_at(owner, 0) &&
          ab_particles_update_level(owner, 0, NULL));
    ab_particles_close(owner);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 1 && argc != 3) {
        fputs("Usage: ab_particle_tests [APK NATIVE_CORPUS]\n", stderr);
        return 2;
    }
    bool okay = controls();
    if (!okay || argc == 1)
        return okay ? 0 : 1;
    AbBuffer archive_bytes = {0}, bytecode[3] = {{0}};
    AbZip archive = {0};
    AbLuaChunk chunks[3] = {0};
    AbLuaData data[3] = {{0}};
    AbParticles *owner = NULL;
    AbEngineRandom random = {0};
    AbParticleSource sources[3] = {0};
    uint8_t key[32] = {0};
    const char *const paths[] = {
        "assets/data/scripts/particles/redBuff.lua",
        "assets/data/scripts/particles/smokeBuffBirdsDefault.lua",
        "assets/data/scripts/particles/smokeBuff.lua"};
    const char *const names[] = {"redBuff", "smokeBuffBirdsDefault", "smokeBuff"};
    AbError error = {0};
    FILE *file = fopen(argv[2], "rb");
    okay = file && ab_file_read(argv[1], 64 * 1024 * 1024, &archive_bytes, &error) &&
           ab_zip_open((AbSpan){archive_bytes.data, archive_bytes.size}, &archive,
                       &error) &&
           ab_target_resource_key(&archive, 16 * 1024 * 1024, key, &error);
    for (size_t index = 0; okay && index < 3; ++index) {
        okay = ab_target_packed_read(&archive, paths[index], (AbSpan){key, 32},
                                     1024 * 1024, bytecode + index, &error) &&
               ab_lua_read((AbSpan){bytecode[index].data, bytecode[index].size},
                           1024 * 1024, chunks + index, &error) &&
               ab_lua_data(chunks + index, 1024 * 1024, data + index, &error);
        sources[index] = (AbParticleSource){span(names[index]), data[index].globals};
    }
    memset(key, 0, sizeof(key));
    AbParticleConfig config = {.sources = sources,
                               .source_count = 3,
                               .capacity = 8,
                               .level_capacity = 8,
                               .storage_bytes = 8192,
                               .random = &random,
                               .admission = {40, 125, 0.5f},
                               .sprite = sprite};
    okay = okay && ab_particles_open(config, &owner, &error) &&
           compare(owner, &random, file);
    if (okay) {
        ab_particles_close(owner);
        owner = NULL;
        okay =
            ab_particles_open(config, &owner, &error) && frame_particle_checks(owner);
    }
    if (!okay && error.message[0])
        fprintf(stderr, "%s\n", error.message);
    ab_particles_close(owner);
    for (size_t index = 0; index < 3; ++index) {
        ab_lua_data_close(data + index);
        ab_lua_close(chunks + index);
        ab_buffer_free(bytecode + index);
    }
    ab_zip_close(&archive);
    ab_buffer_free(&archive_bytes);
    if (file && fclose(file))
        okay = false;
    return okay ? 0 : 1;
}
