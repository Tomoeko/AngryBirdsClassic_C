#include "ab/collision_damage.h"

#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Failed at line %d: %s\n", __LINE__, #condition);          \
            return false;                                                              \
        }                                                                              \
    } while (0)

static float from_bits(uint32_t bits) {
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static uint32_t to_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static bool edge_checks(void) {
    AbLegacyCollisionBody bodies[] = {
        {.velocity = {30, 40},
         .mass = 2,
         .strength = 6,
         .defence = 2,
         .has_strength = true,
         .has_defence = true},
        {.mass = 1,
         .strength = 100,
         .defence = 5,
         .has_strength = true,
         .has_defence = true},
    };
    AbLegacyBlockDamage result;
    CHECK(ab_legacy_block_damage(bodies, &result));
    CHECK(result.force == 10 && result.remaining_strength[0] == 0 &&
          result.remaining_strength[1] == 95 && result.callback_damage == 11 &&
          result.mark_dead[0] && !result.mark_dead[1]);
    bodies[0].no_collision_damage = true;
    CHECK(ab_legacy_block_damage(bodies, &result));
    CHECK(result.remaining_strength[0] == 0 && result.remaining_strength[1] == 100 &&
          result.callback_damage == 6);
    bodies[0].no_collision_damage = false;
    bodies[0].ignore_ground_damage = true;
    bodies[1].ground_or_static = true;
    CHECK(ab_legacy_block_damage(bodies, &result));
    CHECK(result.remaining_strength[0] == 6 && result.remaining_strength[1] == 100 &&
          result.callback_damage == 0 && !result.mark_dead[0]);
    bodies[1].ground_or_static = false;
    bodies[0].ignore_all_damage = true;
    CHECK(ab_legacy_block_damage(bodies, &result));
    CHECK(result.remaining_strength[0] == 6 && result.callback_damage == 5);
    bodies[0].ignore_all_damage = false;
    bodies[0].has_strength = false;
    bodies[0].strength = NAN;
    bodies[0].has_defence = false;
    bodies[0].defence = NAN;
    CHECK(ab_legacy_block_damage(bodies, &result));
    CHECK(result.remaining_strength[0] == 0 && result.callback_damage == 5);
    bodies[0].has_strength = true;
    bodies[0].strength = -1;
    CHECK(ab_legacy_block_damage(bodies, &result));
    CHECK(result.remaining_strength[0] == 0 && result.callback_damage == 4 &&
          result.mark_dead[0]);
    AbLegacyBlockDamage unchanged = result;
    bodies[0].mass = -1;
    CHECK(!ab_legacy_block_damage(bodies, &result));
    bodies[0].mass = FLT_MAX;
    bodies[0].velocity.x = FLT_MAX;
    CHECK(!ab_legacy_block_damage(bodies, &result));
    CHECK(!ab_legacy_block_damage(NULL, &result));
    CHECK(!ab_legacy_block_damage(bodies, NULL));
    CHECK(memcmp(&result, &unchanged, sizeof(result)) == 0);
    return true;
}

static bool read_words(FILE *file, uint32_t *words, size_t count) {
    for (size_t field = 0; field < count; ++field) {
        CHECK(fscanf(file, "%8" SCNx32, words + field) == 1);
    }
    CHECK(fgetc(file) == '\n');
    return true;
}

static bool compare_words(const uint32_t *actual, const uint32_t *expected,
                          size_t count, size_t item, const char *section) {
    for (size_t field = 0; field < count; ++field) {
        if (actual[field] != expected[field]) {
            fprintf(stderr,
                    "Block damage %zu %s field %zu: %08" PRIx32 " != %08" PRIx32 "\n",
                    item, section, field, actual[field], expected[field]);
            return false;
        }
    }
    return true;
}

#include "bird_damage.h"
#include "bird_route.h"

static bool native_checks(FILE *file) {
    char marker[80];
    CHECK(fgets(marker, sizeof(marker), file));
    if (strcmp(marker, "AB_ANDROID_BIRD_DAMAGE_V1 4096\n") == 0) {
        return native_bird_checks(file);
    }
    if (strcmp(marker, "AB_ANDROID_BIRD_ROUTE_V1 4096\n") == 0) {
        return native_bird_route_checks(file);
    }
    CHECK(strcmp(marker, "AB_ANDROID_BLOCK_DAMAGE_V1 4096\n") == 0);
    size_t deaths[2] = {0};
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[18], expected[9];
        CHECK(read_words(file, input, 18));
        AbLegacyCollisionBody bodies[2];
        for (size_t index = 0; index < 2; ++index) {
            const uint32_t *body = input + index * 7;
            bodies[index] = (AbLegacyCollisionBody){
                .velocity = {from_bits(body[0]), from_bits(body[1])},
                .mass = from_bits(body[2]),
                .strength = from_bits(body[3]),
                .defence = from_bits(body[4]),
                .has_strength = !(body[5] & 8),
                .has_defence = !(body[5] & 16),
                .ignore_ground_damage = (body[5] & 1) != 0,
                .ignore_all_damage = (body[5] & 2) != 0,
                .no_collision_damage = (body[5] & 4) != 0,
                .ground_or_static = (body[5] & 96) != 0,
            };
        }
        AbLegacyBlockDamage result;
        CHECK(ab_legacy_block_damage(bodies, &result));
        const uint32_t actual[] = {
            bodies[0].has_strength ? 3u : 0u,
            to_bits(result.remaining_strength[0]),
            bodies[1].has_strength ? 3u : 0u,
            to_bits(result.remaining_strength[1]),
            result.mark_dead[0],
            result.mark_dead[1],
        };
        CHECK(read_words(file, expected, 6));
        CHECK(compare_words(actual, expected, 6, item, "strength/dead"));
        uint32_t count;
        CHECK(read_words(file, &count, 1) && count == 3);
        for (size_t event = 0; event < 3; ++event) {
            uint32_t record[9] = {event == 2 ? 2u : 1u, event == 1 ? 2u : 1u,
                                  event == 2 ? 2u : 0u, to_bits(result.force)};
            if (event == 2) {
                record[4] = to_bits(result.callback_damage);
                memcpy(record + 5, input + 14, 4 * sizeof(*input));
            }
            CHECK(read_words(file, expected, 9));
            CHECK(compare_words(record, expected, 9, item, "boundary callback"));
        }
        deaths[0] += result.mark_dead[0];
        deaths[1] += result.mark_dead[1];
    }
    CHECK(fgets(marker, sizeof(marker), file));
    CHECK(strcmp(marker, "AB block damage reference complete\n") == 0);
    CHECK(fgetc(file) == EOF && !ferror(file));
    printf("All 4096 original-Lua legacy block collisions match, including "
           "12288 boundary calls and %zu/%zu dead-block insertions.\n",
           deaths[0], deaths[1]);
    return true;
}

int main(int argc, char **argv) {
    if (argc > 2) {
        fprintf(stderr, "Usage: ab_damage_tests [NATIVE_BLOCK_DAMAGE_TRACE]\n");
        return 2;
    }
    if (!edge_checks() || !bird_edge_checks() || !bird_route_edges()) {
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
