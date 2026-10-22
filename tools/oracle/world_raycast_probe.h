#ifndef AB_ORACLE_WORLD_RAYCAST_H
#define AB_ORACLE_WORLD_RAYCAST_H

#include "../../tests/world_raycast_cases.h"
#include <stdbool.h>

typedef struct {
    uint32_t *vtable;
    uint32_t nearest[6];
} WorldRayObserver;

static uint32_t world_ray_native_input[8], world_ray_native_events[WORLD_RAY_BODIES][8];
static uint32_t world_ray_native_count;
static float (*world_ray_native_report)(void *, void *, const void *, const void *,
                                        float);
static void (*world_ray_native_world)(const void *, void *, const void *, const void *);
static void (*world_ray_native_tree)(const void *, void *, const void *);

static void world_ray_native_noop(void *object) {
    (void)object;
}

static float world_ray_native_observe(void *context, void *fixture, const void *point,
                                      const void *normal, float fraction) {
    WorldRayObserver *observer = context;
    if (world_ray_native_count >= WORLD_RAY_BODIES) {
        fail("Native world ray reports exceed fixture count\n");
    }
    const uint32_t *native_fixture = fixture;
    const uint32_t *body = (const uint32_t *)(uintptr_t)native_fixture[2];
    const uint32_t *position = point, *direction = normal;
    uint32_t *event = world_ray_native_events[world_ray_native_count++];
    FloatBits value = {.number = fraction};
    event[0] = native_fixture[10];
    event[1] = body[37];
    event[2] = position[0];
    event[3] = position[1];
    event[4] = direction[0];
    event[5] = direction[1];
    event[6] = value.bits;
    float retained =
        world_ray_native_report(observer->nearest, fixture, point, normal, fraction);
    FloatBits returned = {.number = retained};
    if (returned.bits != value.bits) {
        fail("Original nearest callback changed its fraction\n");
    }
    event[7] = world_raycast_return(world_ray_native_input[7], event[0], value.bits);
    FloatBits result = {.bits = event[7]};
    return result.number;
}

static void world_ray_native_run(const uint32_t *world, void *callback,
                                 const uint32_t input[8]) {
    if (input[6] == UINT32_C(0x3f800000)) {
        world_ray_native_world(world, callback, input + 2, input + 4);
    } else {
        /* Original b2World constructs this same two-pointer wrapper. Invoke its
         * unchanged instantiated tree method for explicit non-unit limits. */
        const void *tree = (const uint8_t *)world + 0x191d8;
        uint32_t wrapper[] = {(uint32_t)(uintptr_t)tree, (uint32_t)(uintptr_t)callback};
        world_ray_native_tree(tree, wrapper, input + 2);
    }
}

static void probe_world_raycast(void) {
    void (*construct_world)(void *, const void *) =
        (void (*)(void *, const void *))find_function("_ZN7b2WorldC1ERK6b2Vec2");
    void (*destroy_world)(void *) = (void (*)(void *))find_function("_ZN7b2WorldD1Ev");
    void *(*create_body)(void *, const void *) =
        (void *(*)(void *, const void *))find_function(
            "_ZN7b2World10CreateBodyEPK9b2BodyDef");
    void *(*create_fixture)(void *, const void *) =
        (void *(*)(void *, const void *))find_function(
            "_ZN6b2Body13CreateFixtureEPK12b2FixtureDef");
    void (*set_box)(void *, float, float) = (void (*)(
        void *, float, float))find_function("_ZN14b2PolygonShape8SetAsBoxEff");
    void (*construct_nearest)(void *) =
        (void (*)(void *))find_function("_ZN26RaycastFindNearestCallbackC1Ev");
    void (*destroy_nearest)(void *) =
        (void (*)(void *))find_function("_ZN26RaycastFindNearestCallbackD1Ev");
    void (*get_nearest)(const void *, void **, void *, void *) =
        (void (*)(const void *, void **, void *, void *))find_function(
            "_ZNK26RaycastFindNearestCallback9GetResultEPP9b2FixtureP6b2Vec2S4_");
    world_ray_native_report =
        (float (*)(void *, void *, const void *, const void *, float))find_function(
            "_ZN26RaycastFindNearestCallback13ReportFixtureEP9b2FixtureRK6b2Vec2S4_f");
    world_ray_native_world =
        (void (*)(const void *, void *, const void *, const void *))find_function(
            "_ZNK7b2World7RayCastEP17b2RayCastCallbackRK6b2Vec2S4_");
    world_ray_native_tree = (void (*)(const void *, void *, const void *))find_function(
        "_ZNK13b2DynamicTree7RayCastI21b2WorldRayCastWrapperEEvPT_RK14b2RayCastInput");
    const uint32_t *circle_vtable =
        (const uint32_t *)find_symbol("_ZTV13b2CircleShape", 1);
    const uint32_t *polygon_vtable =
        (const uint32_t *)find_symbol("_ZTV14b2PolygonShape", 1);
    uint32_t observer_vtable[] = {(uint32_t)(uintptr_t)world_ray_native_noop,
                                  (uint32_t)(uintptr_t)world_ray_native_noop,
                                  (uint32_t)(uintptr_t)world_ray_native_observe};
    static uint64_t storage[16384];
    uint32_t *world = (uint32_t *)storage;
    semihost(4, "AB_ANDROID_WORLD_RAYCAST_V1 32 64 12\n");
    for (uint32_t scene = 0; scene < 32; ++scene) {
        uint32_t gravity[2] = {0};
        construct_world(world, gravity);
        for (uint32_t index = 0; index < WORLD_RAY_BODIES; ++index) {
            uint32_t row[WORLD_RAY_BODY_WORDS];
            world_raycast_body(scene, index, row);
            print_words(row, WORLD_RAY_BODY_WORDS);
            uint32_t body_definition[13] = {0};
            body_definition[0] = row[1];
            body_definition[1] = row[2];
            body_definition[2] = row[3];
            body_definition[3] = row[4];
            body_definition[9] = UINT32_C(0x00000101);
            body_definition[10] = row[5];
            body_definition[11] = row[0];
            body_definition[12] = UINT32_C(0x3f800000);
            void *body = create_body(world, body_definition);
            if (!body) {
                fail("Native world ray body creation failed\n");
            }
            uint32_t shape[38] = {0};
            shape[0] =
                (uint32_t)(uintptr_t)((row[6] ? polygon_vtable : circle_vtable) + 2);
            shape[1] = row[6];
            shape[2] = row[7];
            if (row[6]) {
                FloatBits width = {.bits = row[8]}, height = {.bits = row[9]};
                set_box(shape, width.number, height.number);
            } else {
                shape[3] = row[8];
                shape[4] = row[9];
            }
            uint32_t fixture_definition[] = {
                (uint32_t)(uintptr_t)shape, row[0], UINT32_C(0x3e4ccccd), 0, 0,
                row[10] | row[11] << 16,    row[12]};
            if (!create_fixture(body, fixture_definition)) {
                fail("Native world ray fixture creation failed\n");
            }
        }
        if (scene & 16u) {
            world[0x191d4 / 4] |= 2u;
        }
        for (uint32_t query = 0; query < 64; ++query) {
            world_raycast_case(scene, query, world_ray_native_input);
            print_words(world_ray_native_input, WORLD_RAY_INPUT_WORDS);
            WorldRayObserver observer = {.vtable = observer_vtable, .nearest = {0}};
            construct_nearest(observer.nearest);
            world_ray_native_count = 0;
            world_ray_native_run(world, &observer, world_ray_native_input);
            uint32_t output[WORLD_RAY_OUTPUT_WORDS] = {0};
            output[0] = world_ray_native_count;
            if (observer.nearest[1]) {
                void *fixture = NULL;
                get_nearest(observer.nearest, &fixture, output + 2, output + 4);
                output[1] = ((const uint32_t *)fixture)[10];
                output[6] = world_ray_native_events[world_ray_native_count - 1u][6];
                output[7] = world_ray_native_events[world_ray_native_count - 1u][7];
            }
            destroy_nearest(observer.nearest);
            uint32_t nearest[6] = {0};
            construct_nearest(nearest);
            world_ray_native_run(world, nearest, world_ray_native_input);
            if (nearest[1]) {
                void *fixture_pointer = NULL;
                get_nearest(nearest, &fixture_pointer, output + 11, output + 13);
                const uint32_t *fixture = fixture_pointer;
                const uint32_t *body = (const uint32_t *)(uintptr_t)fixture[2];
                output[8] = 1;
                output[9] = fixture[10];
                output[10] = body[37];
                const uint32_t *shape = (const uint32_t *)(uintptr_t)fixture[3];
                const uint32_t *vtable = (const uint32_t *)(uintptr_t)shape[0];
                bool (*shape_ray)(const void *, void *, const void *, const void *,
                                  int) =
                    (bool (*)(const void *, void *, const void *, const void *, int))(
                        uintptr_t)vtable[5];
                uint32_t hit[3] = {0};
                if (!shape_ray(shape, hit, world_ray_native_input + 2, body + 3, 0)) {
                    fail("Original nearest fixture does not retain its ray hit\n");
                }
                output[15] = hit[2];
            }
            destroy_nearest(nearest);
            print_words(output, WORLD_RAY_OUTPUT_WORDS);
            for (uint32_t event = 0; event < world_ray_native_count; ++event) {
                print_words(world_ray_native_events[event], WORLD_RAY_EVENT_WORDS);
            }
        }
        world[0x191d4 / 4] &= ~2u;
        destroy_world(world);
    }
    semihost(4, "AB world raycast reference complete\n");
}

#endif
