#include "ab/physics_fixture.h"

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

_Static_assert(sizeof(AbAabb) == 4 * sizeof(uint32_t),
               "Native fixture comparison requires four bounds words");

static float from_bits(uint32_t bits) {
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static bool edge_checks(AbBroadPhase *phase) {
    AbError error = {0};
    CHECK(ab_broadphase_open((AbBroadLimits){16, 16, 16}, phase, &error));
    AbFixtureProxy proxy = {0};
    AbPhysicsShape shape = {.kind = AB_SHAPE_CIRCLE, .data.circle = {1, {0, 0}}};
    AbRigidTransform origin = {{0, 0}, 0, 1}, moved = {{0.01f, 0}, 0, 1};
    CHECK(ab_fixture_proxy_create(&proxy, &shape, origin, phase, 1, &error));
    CHECK(!ab_fixture_proxy_create(&proxy, &shape, origin, phase, 1, &error));
    for (size_t touch = 0; touch < 15; ++touch) {
        CHECK(ab_broadphase_touch(phase, proxy.proxy, &error));
    }
    CHECK(ab_fixture_proxy_sync(&proxy, &shape, origin, moved, phase, &error));
    CHECK(proxy.bounds.upper.x == 1.01f && phase->move_count == 16);
    AbFixtureProxy unchanged = proxy;
    moved.position.x = 10;
    CHECK(!ab_fixture_proxy_sync(&proxy, &shape, origin, moved, phase, &error));
    CHECK(memcmp(&proxy, &unchanged, sizeof(proxy)) == 0);
    shape.data.circle.radius = NAN;
    CHECK(!ab_fixture_proxy_sync(&proxy, &shape, origin, origin, phase, &error));
    CHECK(memcmp(&proxy, &unchanged, sizeof(proxy)) == 0);
    CHECK(ab_fixture_proxy_destroy(&proxy, phase));
    CHECK(!proxy.active && proxy.proxy == -1 && phase->proxy_count == 0);
    CHECK(ab_fixture_proxy_destroy(&proxy, phase));
    CHECK(ab_fixture_proxy_sync(&proxy, NULL, origin, moved, NULL, &error));
    CHECK(!ab_fixture_proxy_create(&proxy, &shape, origin, phase, 1, &error));
    shape.kind = AB_SHAPE_POLYGON;
    shape.data.polygon = (AbPolygon){.count = 9};
    CHECK(!ab_fixture_proxy_create(&proxy, &shape, origin, phase, 1, &error));
    ab_broadphase_close(phase);
    return true;
}

static bool read_words(FILE *file, uint32_t *words, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%8" SCNx32, words + index) == 1);
    }
    CHECK(fgetc(file) == '\n');
    return true;
}

static bool compare_words(const uint32_t *actual, const uint32_t *expected,
                          size_t count, size_t operation, const char *section) {
    for (size_t field = 0; field < count; ++field) {
        if (actual[field] != expected[field]) {
            fprintf(stderr,
                    "Fixture operation %zu %s field %zu: %08" PRIx32 " != %08" PRIx32
                    "\n",
                    operation, section, field, actual[field], expected[field]);
            return false;
        }
    }
    return true;
}

static AbRigidTransform read_transform(const uint32_t words[4]) {
    return (AbRigidTransform){{from_bits(words[0]), from_bits(words[1])},
                              from_bits(words[2]),
                              from_bits(words[3])};
}

static bool compare_state(FILE *file, const AbFixtureProxy *proxy,
                          const AbBroadPhase *phase, size_t operation) {
    uint32_t expected[11], actual[10] = {(uint32_t)proxy->active};
    CHECK(read_words(file, expected, 10));
    memcpy(actual + 1, &proxy->bounds, sizeof(proxy->bounds));
    actual[5] = (uint32_t)proxy->proxy;
    if (proxy->active) {
        memcpy(actual + 6, &phase->tree.nodes[proxy->proxy].bounds,
               sizeof(proxy->bounds));
    }
    CHECK(compare_words(actual, expected, 10, operation, "proxy"));
    CHECK(read_words(file, expected, 11));
    uint32_t move_hash = UINT32_C(2166136261);
    for (size_t move = 0; move < phase->move_count; ++move) {
        move_hash = (move_hash ^ (uint32_t)phase->moves[move]) * UINT32_C(16777619);
    }
    const uint32_t summary[] = {
        (uint32_t)phase->tree.root,
        (uint32_t)phase->tree.count,
        (uint32_t)phase->tree.capacity,
        (uint32_t)phase->tree.free_head,
        phase->tree.insertion_count,
        (uint32_t)phase->proxy_count,
        (uint32_t)phase->move_capacity,
        (uint32_t)phase->move_count,
        (uint32_t)phase->pair_capacity,
        (uint32_t)phase->pair_count,
        move_hash,
    };
    CHECK(compare_words(summary, expected, 11, operation, "broadphase"));
    return true;
}

static bool native_checks(FILE *file, AbBroadPhase *phase) {
    char header[128];
    CHECK(fgets(header, sizeof(header), file));
    CHECK(strcmp(header, "AB_ANDROID_FIXTURE_PROXIES_V1 4096\n") == 0);
    AbError error = {0};
    CHECK(ab_broadphase_open((AbBroadLimits){128, 8192, 16}, phase, &error));
    AbFixtureProxy proxies[32] = {0};
    AbPhysicsShape shapes[32] = {0};
    for (size_t slot = 0; slot < 32; ++slot) {
        proxies[slot].proxy = -1;
    }
    for (size_t operation = 0; operation < 4096; ++operation) {
        uint32_t input[14];
        CHECK(read_words(file, input, 14) && input[0] <= 2 && input[1] < 32);
        size_t slot = input[1];
        AbRigidTransform first = read_transform(input + 6);
        AbRigidTransform second = read_transform(input + 10);
        if (input[0] == 0) {
            CHECK(!proxies[slot].active && (input[2] == 0 || input[2] == 2));
            shapes[slot].kind = (AbShapeKind)input[2];
            if (input[2] == AB_SHAPE_CIRCLE) {
                shapes[slot].data.circle = (AbCircle){
                    from_bits(input[3]), {from_bits(input[4]), from_bits(input[5])}};
            } else {
                shapes[slot].data.polygon = ab_polygon_box(
                    from_bits(input[4]), from_bits(input[5]), from_bits(input[3]));
            }
            CHECK(ab_fixture_proxy_create(proxies + slot, shapes + slot, first, phase,
                                          slot + 1, &error));
        } else if (input[0] == 1) {
            CHECK(ab_fixture_proxy_sync(proxies + slot, shapes + slot, first, second,
                                        phase, &error));
        } else {
            CHECK(ab_fixture_proxy_destroy(proxies + slot, phase));
        }
        CHECK(compare_state(file, proxies + slot, phase, operation));
    }
    CHECK(fgets(header, sizeof(header), file));
    CHECK(strcmp(header, "AB fixture proxy reference complete\n") == 0);
    CHECK(fgetc(file) == EOF && !ferror(file));
    puts("All 4096 fixture proxy states and broadphase summaries match.");
    return true;
}

int main(int argc, char **argv) {
    if (argc > 2) {
        fprintf(stderr, "Usage: ab_fixture_tests [NATIVE_FIXTURE_TRACE]\n");
        return 2;
    }
    AbBroadPhase phase = {0};
    bool okay = edge_checks(&phase);
    if (okay && argc == 2) {
        FILE *file = fopen(argv[1], "rb");
        if (!file) {
            perror(argv[1]);
            okay = false;
        } else {
            okay = native_checks(file, &phase);
            if (fclose(file) != 0) {
                okay = false;
            }
        }
    }
    ab_broadphase_close(&phase);
    return okay ? 0 : 1;
}
