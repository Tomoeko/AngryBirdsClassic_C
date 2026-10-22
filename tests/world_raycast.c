#include "ab/world_raycast.h"
#include "world_raycast_cases.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(value)                                                                   \
    do {                                                                               \
        if (!(value)) {                                                                \
            fprintf(stderr, "World raycast check failed at %d: %s\n", __LINE__,        \
                    #value);                                                           \
            return false;                                                              \
        }                                                                              \
    } while (0)

#include "world_raycast_fixture.h"

static bool observe(void *context, const AbWorldRaycastHit *hit, float *fraction,
                    AbError *error) {
    WorldRayFixture *fixture = context;
    CHECK(fixture->count < WORLD_RAY_BODIES);
    uint32_t *event = fixture->events[fixture->count++];
    event[0] = (uint32_t)fixture->world.storage.fixtures[hit->fixture].user_data;
    event[1] = (uint32_t)fixture->world.storage.bodies[hit->body].motion.user_data;
    event[2] = bits(hit->point.x);
    event[3] = bits(hit->point.y);
    event[4] = bits(hit->normal.x);
    event[5] = bits(hit->normal.y);
    event[6] = bits(hit->fraction);
    event[7] = world_raycast_return(fixture->input[7], event[0], event[6]);
    fixture->last = *hit;
    *fraction = from_bits(event[7]);
    return !fixture->fail_callback ||
           ab_fail(error, "Controlled world ray callback failure");
}

static bool words(FILE *file, const uint32_t *actual, size_t count, uint32_t scene,
                  uint32_t query) {
    for (size_t index = 0; index < count; ++index) {
        uint32_t expected;
        CHECK(fscanf(file, "%" SCNx32, &expected) == 1);
        if (actual[index] != expected) {
            fprintf(stderr,
                    "World raycast scene %u query %u word %zu: %08" PRIx32
                    " != %08" PRIx32 "\n",
                    scene, query, index, actual[index], expected);
            return false;
        }
    }
    return true;
}

static bool compare_query(FILE *file, WorldRayFixture *fixture, uint32_t scene,
                          uint32_t query, size_t *matched, size_t *callbacks,
                          size_t *hits) {
    world_raycast_case(scene, query, fixture->input);
    CHECK(words(file, fixture->input, WORLD_RAY_INPUT_WORDS, scene, query));
    AbRay ray = {{from_bits(fixture->input[2]), from_bits(fixture->input[3])},
                 {from_bits(fixture->input[4]), from_bits(fixture->input[5])},
                 from_bits(fixture->input[6])};
    fixture->count = 0;
    fixture->last = (AbWorldRaycastHit){0};
    CHECK(ab_world_raycast(&fixture->world, ray, observe, fixture, NULL, NULL));
    uint32_t output[WORLD_RAY_OUTPUT_WORDS] = {0};
    output[0] = (uint32_t)fixture->count;
    if (fixture->count) {
        const uint32_t *last = fixture->events[fixture->count - 1];
        output[1] = last[0];
        output[2] = bits(fixture->last.point.x);
        output[3] = bits(fixture->last.point.y);
        output[4] = bits(fixture->last.normal.x);
        output[5] = bits(fixture->last.normal.y);
        output[6] = bits(fixture->last.fraction);
        output[7] = last[7];
    }
    AbWorldRaycastHit nearest = {.fixture = SIZE_MAX,
                                 .body = SIZE_MAX,
                                 .point = {17, 18},
                                 .normal = {19, 20},
                                 .fraction = 21};
    bool found = true;
    CHECK(ab_world_raycast_nearest(&fixture->world, ray, &nearest, &found, NULL));
    output[8] = found;
    if (found) {
        output[9] =
            (uint32_t)fixture->world.storage.fixtures[nearest.fixture].user_data;
        output[10] =
            (uint32_t)fixture->world.storage.bodies[nearest.body].motion.user_data;
        output[11] = bits(nearest.point.x);
        output[12] = bits(nearest.point.y);
        output[13] = bits(nearest.normal.x);
        output[14] = bits(nearest.normal.y);
        output[15] = bits(nearest.fraction);
    } else {
        CHECK(nearest.fixture == SIZE_MAX && nearest.body == SIZE_MAX &&
              nearest.point.x == 17 && nearest.point.y == 18 &&
              nearest.normal.x == 19 && nearest.normal.y == 20 &&
              nearest.fraction == 21);
    }
    CHECK(words(file, output, WORLD_RAY_OUTPUT_WORDS, scene, query));
    for (size_t event = 0; event < fixture->count; ++event) {
        CHECK(words(file, fixture->events[event], WORLD_RAY_EVENT_WORDS, scene, query));
    }
    CHECK(!fixture->world.failed && fixture->world.locked == ((scene & 16u) != 0));
    *matched += WORLD_RAY_INPUT_WORDS + WORLD_RAY_OUTPUT_WORDS +
                fixture->count * WORLD_RAY_EVENT_WORDS;
    *callbacks += fixture->count;
    *hits += found;
    return true;
}

static bool compare_scene(FILE *file, uint32_t scene, WorldRayFixture *fixture,
                          size_t *matched, size_t *callbacks, size_t *hits) {
    CHECK(open_fixture(fixture));
    for (uint32_t index = 0; index < WORLD_RAY_BODIES; ++index) {
        uint32_t row[13];
        world_raycast_body(scene, index, row);
        CHECK(words(file, row, WORLD_RAY_BODY_WORDS, scene, index));
        CHECK(create_body(fixture, row));
        *matched += WORLD_RAY_BODY_WORDS;
    }
    fixture->world.locked = (scene & 16u) != 0;
    for (uint32_t query = 0; query < 64; ++query) {
        CHECK(compare_query(file, fixture, scene, query, matched, callbacks, hits));
    }
    return true;
}

static bool reference(FILE *file) {
    char marker[80];
    CHECK(fgets(marker, sizeof(marker), file) &&
          !strcmp(marker, "AB_ANDROID_WORLD_RAYCAST_V1 32 64 12\n"));
    size_t matched = 0, callbacks = 0, hits = 0;
    for (uint32_t scene = 0; scene < 32; ++scene) {
        WorldRayFixture fixture = {0};
        bool okay = compare_scene(file, scene, &fixture, &matched, &callbacks, &hits);
        ab_world_close(&fixture.world);
        CHECK(okay);
    }
    CHECK(fscanf(file, " %79[^\n]", marker) == 1 &&
          !strcmp(marker, "AB world raycast reference complete"));
    CHECK(fgetc(file) == '\n' && fgetc(file) == EOF && !ferror(file));
    printf("All 2048 original world rays match: %zu words, %zu ordered hit callbacks, "
           "%zu nearest hits.\n",
           matched, callbacks, hits);
    return true;
}

static bool control_fixture(WorldRayFixture *fixture) {
    CHECK(open_fixture(fixture));
    AbRay ray = {{-10, -2}, {10, -2}, 1};
    float fraction = 17;
    CHECK(ab_world_raycast(&fixture->world, ray, NULL, NULL, &fraction, NULL) &&
          fraction == 1);
    uint32_t row[13];
    world_raycast_body(0, 0, row);
    CHECK(create_body(fixture, row));
    fixture->fail_callback = true;
    fraction = 17;
    CHECK(!ab_world_raycast(&fixture->world, ray, observe, fixture, &fraction, NULL));
    CHECK(fixture->count == 1 && fraction == 17 && !fixture->world.failed &&
          fixture->world.body_count == 1 && fixture->world.fixture_count == 1);
    AbWorldRaycastHit nearest = {0};
    bool found = false;
    CHECK(ab_world_raycast_nearest(&fixture->world, ray, &nearest, &found, NULL) &&
          found);
    CHECK(nearest.body == 0 && nearest.fixture == 0 && nearest.fraction > 0);
    return true;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        WorldRayFixture fixture = {0};
        bool okay = control_fixture(&fixture);
        ab_world_close(&fixture.world);
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
