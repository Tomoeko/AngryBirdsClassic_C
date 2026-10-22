#ifndef AB_ORACLE_WORLD_STEPS_PROBE_H
#define AB_ORACLE_WORLD_STEPS_PROBE_H

#include <stdbool.h>

static uint32_t world_step_number(float value) {
    FloatBits result = {.number = value};
    return result.bits;
}

static uint32_t world_contact_key(const uint32_t *contact) {
    const uint32_t *first = (const uint32_t *)(uintptr_t)contact[12];
    const uint32_t *second = (const uint32_t *)(uintptr_t)contact[13];
    return (first[10] << 16) | second[10];
}

#if defined(AB_ORACLE_WORLD_EVENTS)
#include "world_events.h"
#endif
#if defined(AB_ORACLE_WORLD_OPERATIONS)
#include "world_operations.h"
#endif

static void print_step_body(const uint32_t *body) {
    uint32_t output[38];
    for (size_t field = 0; field < 38; ++field) {
        output[field] = body[field];
    }
    output[22] = UINT32_C(0xdead0000);
    for (size_t field = 23; field <= 24; ++field) {
        output[field] =
            body[field] ? ((const uint32_t *)(uintptr_t)body[field])[37] : 0;
    }
    output[25] = body[25] ? ((const uint32_t *)(uintptr_t)body[25])[10] : 0;
    if (body[27]) {
        const uint32_t *edge = (const uint32_t *)(uintptr_t)body[27];
        output[27] = ((const uint32_t *)(uintptr_t)edge[1])[16];
    }
    if (body[28]) {
        const uint32_t *edge = (const uint32_t *)(uintptr_t)body[28];
        output[28] = world_contact_key((const uint32_t *)(uintptr_t)edge[1]);
    }
    print_words(output, 38);
}

static void print_step_contacts(const uint32_t *world) {
    const uint32_t *contact = (const uint32_t *)(uintptr_t)world[0x19214 / 4];
    size_t count = 0;
    while (contact) {
        uint32_t output[24] = {0};
        uint32_t key = world_contact_key(contact);
        output[0] = key >> 16;
        output[1] = key & 0xffff;
        output[2] = contact[1];
        size_t points = contact[31];
        if (points > 2 || ++count > 28) {
            fail("Invalid controlled world contact list\n");
        }
        if (points) {
            for (size_t field = 0; field < points * 5; ++field) {
                output[3 + field] = contact[16 + field];
            }
            for (size_t field = 10; field < 16; ++field) {
                output[3 + field] = contact[16 + field];
            }
        }
        output[19] = contact[34];
        output[20] = contact[35];
        output[21] = contact[36];
#if defined(AB_ORACLE_WORLD_CONTINUOUS)
        output[22] = contact[32];
        output[23] = contact[1] & 32 ? contact[33] : 0;
        print_words(output, 24);
#else
        print_words(output, 22);
#endif
        contact = (const uint32_t *)(uintptr_t)contact[3];
    }
    if (count != world[0x19218 / 4]) {
        fail("Controlled world contact count disagrees\n");
    }
}

static void print_step_world(const uint32_t *world, const uint32_t *body_pointers,
                             size_t body_count) {
    const uint32_t *phase = world + 0x191d8 / 4;
    const uint32_t summary[] = {world[0x191d4 / 4], world[0x1924c / 4],
                                phase[0],           phase[7],
                                phase[10],          world[0x19218 / 4]};
    print_words(summary, 6);
    for (size_t body = 0; body < body_count; ++body) {
        print_step_body((const uint32_t *)(uintptr_t)body_pointers[body]);
    }
    print_step_contacts(world);
#if defined(AB_ORACLE_WORLD_OPERATIONS)
    print_step_fixtures(world);
#endif
#if defined(AB_ORACLE_WORLD_EVENTS)
    print_world_events();
#endif
}

static void prepare_step_body(uint32_t definition[13], size_t scene, size_t index) {
    for (size_t field = 0; field < 13; ++field) {
        definition[field] = 0;
    }
    definition[0] = index ? (scene == 7 && index == 1 ? 1u : 2u) : 0;
    definition[9] = 0x00000101;
    definition[10] = 1;
    definition[11] = (uint32_t)index + 1;
    definition[12] = 0x3f800000;
    if (!index) {
        definition[2] = 0xbf800000;
        return;
    }
    size_t style = scene % 4;
    float x = style == 1 ? 0 : ((float)index - 2.0f) * 1.2f;
    float y =
        style == 1 ? 1.01f + ((float)index - 1.0f) * 1.05f : 2.0f + (float)index * 1.2f;
    definition[1] = world_step_number(x);
    definition[2] = world_step_number(y);
    definition[3] = style == 3 ? world_step_number(0.1f * (float)index) : 0;
    definition[4] = style == 2 ? world_step_number(index % 2 ? 2.0f : -2.0f) : 0;
    definition[7] = scene % 2 ? world_step_number(0.02f) : 0;
    definition[8] = definition[7];
    if (style == 3 && index % 2) {
        definition[9] |= 0x00010000;
    }
    if (scene == 10 && index == 2) {
        definition[9] &= ~UINT32_C(0xff);
    }
#if defined(AB_ORACLE_WORLD_CONTINUOUS)
    if (scene >= 8) {
        definition[9] |= 0x01000000;
        definition[4] = world_step_number(index % 2 ? 15.0f : -15.0f);
        definition[5] = world_step_number(-60.0f);
        definition[6] = world_step_number(3.0f * (float)index);
    }
#endif
}

static void prepare_step_fixture(uint32_t specification[13], size_t scene, size_t body,
                                 size_t fixture, bool extra) {
    size_t style = scene % 4;
    bool polygon = !body || style == 1 || (style >= 2 && body % 2 == 0);
    if (extra) {
        polygon = false;
    }
    specification[0] = (uint32_t)body + 1;
    specification[1] = polygon ? 2u : 0u;
    specification[2] = polygon ? world_step_number(0.01f) : world_step_number(0.5f);
    specification[3] = polygon      ? world_step_number(body ? 0.5f : 10.0f)
                       : extra      ? world_step_number(-0.5f)
                       : style == 3 ? world_step_number(0.25f)
                                    : 0;
    specification[4] = polygon      ? world_step_number(body ? 0.5f : 1.0f)
                       : style == 3 ? world_step_number(0.1f)
                                    : 0;
    specification[5] = world_step_number(0.3f);
    specification[6] = scene % 5 == 0 ? world_step_number(0.8f) : 0;
    specification[7] = body ? world_step_number(1.0f) : 0;
    specification[8] = scene == 6 && body == 1 ? 1u : 0;
    specification[9] = scene == 14 ? (uint32_t)(1u << (body % 2)) : 1u;
    specification[10] = scene == 12 ? 0 : 0xffff;
    specification[11] = scene == 12 ? 1u : scene == 13 ? 0xffffu : 0;
    specification[12] = (uint32_t)fixture + 1;
}

#if defined(AB_ORACLE_WORLD_LIFECYCLE)
#include "world_lifecycle_probe.h"
#endif
#if defined(AB_ORACLE_WORLD_JOINT_LIFECYCLE)
#include "world_joint_lifecycle_probe.h"
#endif

static void probe_world_steps(void) {
#if defined(AB_ORACLE_WORLD_JOINT_LIFECYCLE)
    probe_world_joint_lifecycle();
    return;
#endif
#if defined(AB_ORACLE_WORLD_LIFECYCLE)
    probe_world_lifecycle();
    return;
#endif
    typedef void (*ConstructWorld)(void *, const void *);
    typedef void (*WorldOperation)(void *);
    typedef void *(*CreateBody)(void *, const void *);
    typedef void *(*CreateFixture)(void *, const void *);
    typedef void (*SetBox)(void *, float, float);
    typedef void (*StepWorld)(void *, float, int, int);
    ConstructWorld construct = (ConstructWorld)find_function("_ZN7b2WorldC1ERK6b2Vec2");
    WorldOperation close_world = (WorldOperation)find_function("_ZN7b2WorldD1Ev");
    CreateBody create_body =
        (CreateBody)find_function("_ZN7b2World10CreateBodyEPK9b2BodyDef");
    CreateFixture create_fixture =
        (CreateFixture)find_function("_ZN6b2Body13CreateFixtureEPK12b2FixtureDef");
    SetBox set_box = (SetBox)find_function("_ZN14b2PolygonShape8SetAsBoxEff");
    StepWorld step = (StepWorld)find_function("_ZN7b2World4StepEfii");
    const uint32_t *circle_vtable =
        (const uint32_t *)find_symbol("_ZTV13b2CircleShape", 1);
    const uint32_t *polygon_vtable =
        (const uint32_t *)find_symbol("_ZTV14b2PolygonShape", 1);
    static uint64_t world_storage[16384];
    uint32_t *world = (uint32_t *)world_storage;
#if defined(AB_ORACLE_WORLD_EVENTS)
    uint32_t listener_vtable[] = {0,
                                  0,
                                  (uint32_t)(uintptr_t)world_begin_event,
                                  (uint32_t)(uintptr_t)world_end_event,
                                  (uint32_t)(uintptr_t)world_pre_event,
                                  (uint32_t)(uintptr_t)world_post_event};
    uint32_t listener[] = {(uint32_t)(uintptr_t)listener_vtable};
#if defined(AB_ORACLE_WORLD_CONTINUOUS)
#if defined(AB_ORACLE_WORLD_OPERATIONS)
    prepare_world_operations();
    semihost(4, "AB_ANDROID_WORLD_OPERATIONS_V1 16 256\n");
#elif defined(AB_ORACLE_WORLD_MUTATIONS)
    semihost(4, "AB_ANDROID_WORLD_REJECTED_IMPACTS_V1 16 256\n");
#else
    semihost(4, "AB_ANDROID_WORLD_CONTINUOUS_EVENTS_V1 16 256\n");
#endif
#else
    semihost(4, "AB_ANDROID_WORLD_EVENTS_V1 16 256\n");
#endif
#else
#if defined(AB_ORACLE_WORLD_CONTINUOUS)
    semihost(4, "AB_ANDROID_WORLD_CONTINUOUS_V1 16 256\n");
#else
    semihost(4, "AB_ANDROID_WORLD_STEPS_V1 16 256\n");
#endif
#endif
    for (size_t scene = 0; scene < 16; ++scene) {
        size_t body_count = 2 + scene % 4;
        uint32_t scene_controls[] = {(uint32_t)body_count,
                                     (uint32_t)(body_count + (scene == 15 ? 1 : 0)),
                                     (uint32_t)(scene % 7 != 0),
                                     (uint32_t)(scene % 3 != 0),
                                     (uint32_t)(scene % 4 != 0),
                                     scene >= 8 ? 0x3f000000u : 0,
                                     0xc1200000};
        uint32_t body_pointers[5] = {0};
        construct(world, scene_controls + 5);
#if defined(AB_ORACLE_WORLD_OPERATIONS)
        step_fixture_count = scene_controls[1];
#endif
#if defined(AB_ORACLE_WORLD_EVENTS)
        world_event_count = 0;
        world[0x19220 / 4] = (uint32_t)(uintptr_t)listener;
#endif
        ((uint8_t *)world)[0x19240] = (uint8_t)scene_controls[2];
        ((uint8_t *)world)[0x19250] = (uint8_t)scene_controls[3];
        ((uint8_t *)world)[0x19251] = 0;
#if defined(AB_ORACLE_WORLD_CONTINUOUS)
        ((uint8_t *)world)[0x19251] = 1;
#endif
        if (!scene_controls[4]) {
            world[0x191d4 / 4] &= ~UINT32_C(4);
        }
        print_words(scene_controls, 7);
        for (size_t body = 0; body < body_count; ++body) {
            uint32_t definition[13];
            prepare_step_body(definition, scene, body);
            print_words(definition, 13);
            uint32_t *created = create_body(world, definition);
            if (!created) {
                fail("Native world body creation failed\n");
            }
            created[2] = 0;
            body_pointers[body] = (uint32_t)(uintptr_t)created;
        }
        for (size_t fixture = 0; fixture < scene_controls[1]; ++fixture) {
            bool extra = fixture == body_count;
            size_t body = extra ? 1 : fixture;
            uint32_t specification[13], shape[38] = {0};
            prepare_step_fixture(specification, scene, body, fixture, extra);
            shape[0] = (uint32_t)(uintptr_t)((specification[1] ? polygon_vtable
                                                               : circle_vtable) +
                                             2);
            shape[1] = specification[1];
            shape[2] = specification[2];
            if (specification[1]) {
                FloatBits width = {.bits = specification[3]};
                FloatBits height = {.bits = specification[4]};
                set_box(shape, width.number, height.number);
            } else {
                shape[3] = specification[3];
                shape[4] = specification[4];
            }
            uint32_t definition[] = {
                (uint32_t)(uintptr_t)shape,
                specification[12],
                specification[5],
                specification[6],
                specification[7],
                specification[8] | (specification[9] << 16),
                specification[10] | (specification[11] << 16),
            };
            print_words(specification, 13);
            void *created_fixture =
                create_fixture((void *)(uintptr_t)body_pointers[body], definition);
            if (!created_fixture) {
                fail("Native world fixture creation failed\n");
            }
#if defined(AB_ORACLE_WORLD_OPERATIONS)
            step_fixture_pointers[fixture] = (uint32_t)(uintptr_t)created_fixture;
#endif
        }
        print_step_world(world, body_pointers, body_count);
        for (size_t frame = 0; frame < 256; ++frame) {
#if defined(AB_ORACLE_WORLD_OPERATIONS)
            uint32_t operation[5];
            prepare_world_operation(operation, frame, body_count);
            print_words(operation, 5);
            world_event_count = 0;
            apply_world_operation(operation, body_pointers);
            print_step_world(world, body_pointers, body_count);
#endif
            uint32_t controls[] = {frame % 53 == 0 ? 0u
                                   : frame >= 128  ? 0x3c888889u
                                                   : 0x3d088889u,
                                   10,
                                   10,
                                   frame % 60 == 20 ? (uint32_t)body_count : 0,
                                   0x3f000000,
                                   0x3f800000,
                                   0x3e800000};
            print_words(controls, 7);
#if defined(AB_ORACLE_WORLD_EVENTS)
            world_event_count = 0;
#endif
            if (controls[3]) {
                uint32_t *body = (uint32_t *)(uintptr_t)body_pointers[controls[3] - 1];
                if (body[0] == 2) {
                    if (!(body[1] & 2)) {
                        body[1] |= 2;
                        body[36] = 0;
                    }
                    for (size_t field = 0; field < 3; ++field) {
                        body[19 + field] =
                            add_float(body[19 + field], controls[4 + field]);
                    }
                }
            }
            FloatBits seconds = {.bits = controls[0]};
            step(world, seconds.number, 10, 10);
            print_step_world(world, body_pointers, body_count);
        }
        close_world(world);
    }
#if defined(AB_ORACLE_WORLD_EVENTS)
#if defined(AB_ORACLE_WORLD_CONTINUOUS)
#if defined(AB_ORACLE_WORLD_OPERATIONS)
    semihost(4, "AB world operations reference complete\n");
#elif defined(AB_ORACLE_WORLD_MUTATIONS)
    semihost(4, "AB world rejected impacts reference complete\n");
#else
    semihost(4, "AB world continuous events reference complete\n");
#endif
#else
    semihost(4, "AB world events reference complete\n");
#endif
#else
#if defined(AB_ORACLE_WORLD_CONTINUOUS)
    semihost(4, "AB world continuous reference complete\n");
#else
    semihost(4, "AB world steps reference complete\n");
#endif
#endif
}

#endif
