#include "ab/level_raycast.h"
#include "game_raycast_cases.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(value)                                                                   \
    do {                                                                               \
        if (!(value)) {                                                                \
            fprintf(stderr, "Game raycast check failed at %d: %s\n", __LINE__,         \
                    #value);                                                           \
            return false;                                                              \
        }                                                                              \
    } while (0)

#include "world_raycast_fixture.h"

typedef struct {
    WorldRayFixture query;
    AbLevelObject objects[WORLD_RAY_BODIES];
    bool removed[WORLD_RAY_BODIES];
    AbLevelBodies bindings;
} GameRayFixture;

static bool create_scene(GameRayFixture *fixture, uint32_t scene, FILE *reference) {
    CHECK(open_fixture(&fixture->query));
    fixture->bindings = (AbLevelBodies){
        .objects = fixture->objects, .capacity = WORLD_RAY_BODIES, .count = 0};
    for (uint32_t index = 0; index < WORLD_RAY_BODIES; ++index) {
        uint32_t row[WORLD_RAY_BODY_WORDS];
        world_raycast_body(scene, index, row);
        if (reference) {
            for (size_t word = 0; word < WORLD_RAY_BODY_WORDS; ++word) {
                uint32_t expected;
                CHECK(fscanf(reference, "%" SCNx32, &expected) == 1 &&
                      expected == row[word]);
            }
        }
        CHECK(create_body(&fixture->query, row));
        GameRayName name = game_raycast_name(index);
        fixture->objects[index] =
            (AbLevelObject){.name = {(const uint8_t *)name.data, name.size},
                            .physics = {.body = index}};
        fixture->query.world.storage.fixtures[index].user_data =
            (scene + index) % 5u ? (uintptr_t)(fixture->objects + index) : 0;
        ++fixture->bindings.count;
    }
    fixture->query.world.locked = (scene & 16u) != 0;
    return true;
}

static bool compare_words(FILE *file, const uint32_t *actual, size_t count,
                          uint32_t scene, uint32_t query) {
    for (size_t index = 0; index < count; ++index) {
        uint32_t expected;
        CHECK(fscanf(file, "%" SCNx32, &expected) == 1);
        if (actual[index] != expected) {
            fprintf(stderr,
                    "Game raycast scene %u query %u word %zu: %08" PRIx32
                    " != %08" PRIx32 "\n",
                    scene, query, index, actual[index], expected);
            return false;
        }
    }
    return true;
}

#if defined(AB_TEST_BIRD_IMPACT_WORLD)
#include "bird_impact_world.h"
#endif

static bool compare_scene(FILE *file, GameRayFixture *fixture, uint32_t scene,
                          size_t *hits, size_t *unnamed) {
    CHECK(create_scene(fixture, scene, file));
    for (uint32_t query = 0; query < 64; ++query) {
        uint32_t input[WORLD_RAY_INPUT_WORDS];
        game_raycast_case(scene, query, input);
        input[6] = UINT32_C(0x3f800000);
        input[7] = 0;
        CHECK(compare_words(file, input, WORLD_RAY_INPUT_WORDS, scene, query));
        AbLevelRaycast hit = {.hit = true, .point = {17, 18}, .normal = {19, 20}};
        AbVec2 first = {from_bits(input[2]), from_bits(input[3])};
        AbVec2 second = {from_bits(input[4]), from_bits(input[5])};
        CHECK(ab_level_bodies_raycast(&fixture->query.world, &fixture->bindings,
                                      fixture->removed, first, second, &hit, NULL));
        uint32_t output[GAME_RAY_OUTPUT_WORDS] = {0};
        output[0] = hit.hit ? 6u : 1u;
        output[1] = hit.hit;
        if (hit.hit) {
            ++*hits;
            if (hit.name.kind == AB_DATA_STRING) {
                CHECK(hit.name.string.size <= 16);
                output[2] = 1;
                output[3] = (uint32_t)hit.name.string.size;
                game_raycast_pack((const char *)hit.name.string.data, output[3],
                                  output + 4);
            } else {
                CHECK(hit.name.kind == AB_DATA_NIL);
                ++*unnamed;
            }
            output[8] = bits(hit.point.x);
            output[9] = bits(hit.point.y);
            output[10] = bits(hit.normal.x);
            output[11] = bits(hit.normal.y);
        } else {
            CHECK(hit.point.x == 17 && hit.point.y == 18 && hit.normal.x == 19 &&
                  hit.normal.y == 20);
        }
        CHECK(compare_words(file, output, GAME_RAY_OUTPUT_WORDS, scene, query));
    }
#if defined(AB_TEST_BIRD_IMPACT_WORLD)
    CHECK(compare_impact_world(file, fixture, scene));
#endif
    fixture->query.world.locked = false;
    for (size_t index = 0; index < WORLD_RAY_BODIES; ++index) {
        CHECK(ab_world_destroy_body(&fixture->query.world, index, NULL));
        fixture->removed[index] = true;
    }
    uint32_t remaining[] = {0, (uint32_t)fixture->query.world.body_count};
    CHECK(compare_words(file, remaining, 2, scene, 64));
    return true;
}

static bool reference(FILE *file) {
    char header[80];
#if defined(AB_TEST_BIRD_IMPACT_WORLD_NUMERIC)
    const char *start = "AB_ANDROID_BIRD_IMPACT_WORLD_NUMERIC_V1 32 64 12 4\n";
    const char *finish = "AB bird impact world numeric reference complete";
#elif defined(AB_TEST_BIRD_IMPACT_WORLD)
    const char *start = "AB_ANDROID_BIRD_IMPACT_WORLD_V1 32 64 12 4\n";
    const char *finish = "AB bird impact world reference complete";
#elif defined(AB_TEST_GAME_RAYCAST_NUMERIC)
    const char *start = "AB_ANDROID_GAME_RAYCAST_NUMERIC_V1 32 64 12\n";
    const char *finish = "AB game raycast numeric reference complete";
#else
    const char *start = "AB_ANDROID_GAME_RAYCAST_V1 32 64 12\n";
    const char *finish = "AB game raycast reference complete";
#endif
    CHECK(fgets(header, sizeof(header), file) && !strcmp(header, start));
    size_t hits = 0, unnamed = 0;
    for (uint32_t scene = 0; scene < 32; ++scene) {
        GameRayFixture fixture = {0};
        bool okay = compare_scene(file, &fixture, scene, &hits, &unnamed);
        ab_world_close(&fixture.query.world);
        CHECK(okay);
    }
    CHECK(fscanf(file, " %79[^\n]", header) == 1 && !strcmp(header, finish));
    CHECK(fgetc(file) == '\n' && fgetc(file) == EOF && !ferror(file));
    printf("Matched 46016 game raycast words, 2048 queries, %zu hits, %zu unnamed\n",
           hits, unnamed);
#if defined(AB_TEST_BIRD_IMPACT_WORLD)
    printf("Matched %zu coupled prediction words, %zu predictions, %zu hits, "
           "%zu rays, %zu boundaries\n",
           impact_world_matched, impact_world_predictions, impact_world_hits,
           impact_world_rays, impact_world_boundaries);
#endif
    return true;
}

static bool controls(GameRayFixture *fixture) {
    CHECK(open_fixture(&fixture->query));
    uint32_t row[WORLD_RAY_BODY_WORDS];
    world_raycast_body(0, 0, row);
    CHECK(create_body(&fixture->query, row));
    fixture->bindings = (AbLevelBodies){
        .objects = fixture->objects, .capacity = WORLD_RAY_BODIES, .count = 1};
    fixture->objects[0].name = (AbSpan){(const uint8_t *)"bound", 5};
    fixture->objects[0].physics.body = 0;
    AbWorldFixture *shape = fixture->query.world.storage.fixtures;
    shape->user_data = (uintptr_t)fixture->objects;
    AbLevelRaycast hit = {0};
    AbVec2 first = {-5, -2}, second = {-1, -2};
    CHECK(ab_level_bodies_raycast(&fixture->query.world, &fixture->bindings,
                                  fixture->removed, first, second, &hit, NULL));
    CHECK(hit.hit && hit.name.kind == AB_DATA_STRING && hit.name.string.size == 5);
    AbLevelRaycast saved = hit;
    fixture->removed[0] = true;
    CHECK(!ab_level_bodies_raycast(&fixture->query.world, &fixture->bindings,
                                   fixture->removed, first, second, &hit, NULL));
    CHECK(!memcmp(&hit, &saved, sizeof(hit)));
    fixture->removed[0] = false;
    shape->user_data = 1;
    CHECK(!ab_level_bodies_raycast(&fixture->query.world, &fixture->bindings, NULL,
                                   first, second, &hit, NULL));
    CHECK(!memcmp(&hit, &saved, sizeof(hit)));
    shape->user_data = (uintptr_t)fixture->objects;
    fixture->objects[0].physics.body = 1;
    CHECK(!ab_level_bodies_raycast(&fixture->query.world, &fixture->bindings, NULL,
                                   first, second, &hit, NULL));
    CHECK(!memcmp(&hit, &saved, sizeof(hit)));
    shape->user_data = 0;
    CHECK(ab_level_bodies_raycast(&fixture->query.world, &fixture->bindings, NULL,
                                  first, second, &hit, NULL));
    CHECK(hit.hit && hit.name.kind == AB_DATA_NIL);
    fixture->query.world.locked = false;
    CHECK(ab_world_destroy_body(&fixture->query.world, 0, NULL));
    saved = hit;
    CHECK(ab_level_bodies_raycast(&fixture->query.world, &fixture->bindings, NULL,
                                  first, second, &hit, NULL));
    CHECK(!hit.hit && !memcmp(&hit.point, &saved.point, sizeof(hit.point)) &&
          !memcmp(&hit.normal, &saved.normal, sizeof(hit.normal)));
    return true;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        GameRayFixture fixture = {0};
        bool okay = controls(&fixture);
        ab_world_close(&fixture.query.world);
        return okay ? 0 : 1;
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
