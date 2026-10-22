#ifndef AB_ORACLE_GAME_POLYGON_H
#define AB_ORACLE_GAME_POLYGON_H

#include "../../tests/game_polygon_cases.h"
#include "../../tests/game_convex_cases.h"

#include <stdbool.h>

static void probe_game_polygon(void) {
    void (*world_construct)(void *, const void *) =
        (void (*)(void *, const void *))find_function("_ZN7b2WorldC1ERK6b2Vec2");
    void (*world_destroy)(void *) = (void (*)(void *))find_function("_ZN7b2WorldD1Ev");
    void (*construct)(void *, void *) =
        (void (*)(void *, void *))find_function("_ZN10GameObjectC2EP7b2World");
    void (*destroy)(void *) = (void (*)(void *))find_function("_ZN10GameObjectD2Ev");
    void (*create)(void *) =
        (void (*)(void *))find_function("_ZN10GameObject6createEv");
    void (*set_vertices)(void *, const void *) =
        (void (*)(void *, const void *))find_function(
            "_ZN10GameObject11setVerticesERKSt6vectorI6b2Vec2SaIS1_EE");
    void (*destroy_body)(void *, void *) =
        (void (*)(void *, void *))find_function("_ZN7b2World11DestroyBodyEP6b2Body");
    static uint64_t world[16384];
    const uint32_t gravity[] = {0, 0};
    world_construct(world, gravity);
#if defined(AB_ORACLE_GAME_CIRCLE_ZEROS)
    const bool zeros = true;
#else
    const bool zeros = false;
#endif
#if defined(AB_ORACLE_GAME_CIRCLE_ZEROS)
    const bool circle = true, convex = false;
    semihost(4, "AB_ANDROID_GAME_CIRCLE_ZEROS_V1 4096\n");
#elif defined(AB_ORACLE_GAME_CONVEX)
    const bool circle = false, convex = true;
    semihost(4, "AB_ANDROID_GAME_CONVEX_V1 4096\n");
#elif defined(AB_ORACLE_GAME_CIRCLE)
    const bool circle = true;
    const bool convex = false;
    semihost(4, "AB_ANDROID_GAME_CIRCLE_V1 4096\n");
#else
    const bool circle = false;
    const bool convex = false;
    semihost(4, "AB_ANDROID_GAME_POLYGON_V1 4096\n");
#endif
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        uint32_t input[32] = {0}, object[50] = {0};
        if (zeros) {
            game_circle_zero_case(sample, input);
        } else if (convex) {
            game_convex_case(sample, input);
        } else {
            game_polygon_case(sample, input);
        }
        size_t input_count = convex ? 32u : 20u;
        size_t count = convex ? input[20] : 3u;
        print_words(input, input_count);
        FloatBits parameters[32];
        for (size_t field = 0; field < input_count; ++field) {
            parameters[field].bits = input[field];
        }
        float width = parameters[1].number, height = parameters[2].number;
        if (!(input[17] & 1u)) {
            float horizontal = input[3] == 0    ? 1.0f
                               : input[17] & 2u ? parameters[15].number
                                                : 0x1.d70a3ep-1f;
            float vertical = input[3] == 0    ? 1.0f
                             : input[17] & 4u ? parameters[16].number
                                              : 0x1.d70a3ep-1f;
            width = (parameters[12].number * parameters[14].number) * horizontal;
            height = (parameters[13].number * parameters[14].number) * vertical;
        }
        float vertices[16];
        for (size_t vertex = 0; vertex < count; ++vertex) {
            size_t position = vertex < 3 ? 6 + vertex * 2 : 21 + (vertex - 3) * 2;
            vertices[vertex * 2] = parameters[position].number * width - width * 0.5f;
            vertices[vertex * 2 + 1] =
                parameters[position + 1].number * height - height * 0.5f;
        }
        construct(object, world);
        object[0] = 0;
        object[1] = circle ? 0u : 2u;
        object[4] = input[3];
        object[5] = input[4];
        object[6] = input[5];
        object[36] = input[18];
        object[37] = input[19];
        const uint32_t vector[] = {(uint32_t)(uintptr_t)vertices,
                                   (uint32_t)(uintptr_t)(vertices + count * 2),
                                   (uint32_t)(uintptr_t)(vertices + count * 2)};
        if (circle) {
            FloatBits radius = {.number = parameters[1].number};
            if (!(input[17] & 1u)) {
                float factor = input[17] & 2u ? parameters[15].number : 0x1.d70a3ep-1f;
                radius.number =
                    ((parameters[12].number * 0.5f) * parameters[14].number) * factor;
            }
            object[35] = radius.bits;
        } else {
            set_vertices(object, vector);
        }
        create(object);
        const uint32_t *body = (const uint32_t *)(uintptr_t)object[11];
        if (!body || body[26] != 1) {
            fail("Native triangle did not produce exactly one fixture\n");
        }
        const uint32_t *fixture = (const uint32_t *)(uintptr_t)body[25];
        const uint32_t *shape = (const uint32_t *)(uintptr_t)fixture[3];
        if (!circle && ((convex && (shape[37] < 3 || shape[37] > 8)) ||
                        (!convex && shape[37] != count))) {
            fail("Native triangle hull has an unexpected vertex count\n");
        }
        const uint32_t metadata[] = {body[26],   body[0],    object[2], object[3],
                                     object[35], body[7],    body[8],   body[29],
                                     body[30],   body[31],   body[32],  fixture[0],
                                     fixture[4], fixture[5], shape[2]};
        print_words(metadata, 15);
        uint32_t geometry[35] = {circle ? 0u : shape[37], shape[3], shape[4]};
        if (!circle) {
            size_t normal_offset = convex ? 19u : 3 + count * 2;
            for (size_t field = 0; field < (size_t)shape[37] * 2; ++field) {
                geometry[3 + field] = shape[5 + field];
                geometry[normal_offset + field] = shape[21 + field];
            }
        }
        print_words(geometry, convex ? 35u : 15u);
        destroy_body(world, (void *)(uintptr_t)body);
        destroy(object);
    }
    world_destroy(world);
#if defined(AB_ORACLE_GAME_CIRCLE_ZEROS)
    semihost(4, "AB game circle zeros reference complete\n");
#else
    semihost(4, circle   ? "AB game circle reference complete\n"
                : convex ? "AB game convex reference complete\n"
                         : "AB game polygon reference complete\n");
#endif
}

#endif
