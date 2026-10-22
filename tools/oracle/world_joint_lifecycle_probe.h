#ifndef AB_ORACLE_WORLD_JOINT_LIFECYCLE_H
#define AB_ORACLE_WORLD_JOINT_LIFECYCLE_H

#include "../../tests/world_joint_cycle.h"
#include "joint_native_state.h"

static uint32_t cycle_joints[6];
static bool cycle_initialized[6];

static void cycle_joint_goodbye(void *listener, const void *pointer) {
    (void)listener;
    if (world_event_count >= 128) {
        fail("Native joint destruction event capacity exceeded\n");
    }
    const uint32_t *joint = pointer;
    const uint32_t *first = (const uint32_t *)(uintptr_t)joint[12];
    const uint32_t *second = (const uint32_t *)(uintptr_t)joint[13];
    const uint32_t *world = (const uint32_t *)(uintptr_t)first[22];
    uint32_t *output = world_events[world_event_count++];
    for (size_t field = 0; field < 36; ++field) {
        output[field] = 0;
    }
    output[0] = 6;
    output[1] = joint[16];
    output[2] = first[37];
    output[3] = second[37];
    output[4] = world[0x19234 / 4];
    cycle_joints[joint[16] - 1] = 0;
}

static void print_cycle_joints(const uint32_t *world) {
    const uint32_t count = world[0x19234 / 4];
    print_words(&count, 1);
    const uint32_t *joint = (const uint32_t *)(uintptr_t)world[0x1922c / 4];
    size_t visited = 0;
    while (joint) {
        if (++visited > 6 || joint[16] < 1 || joint[16] > 6) {
            fail("Invalid native joint cycle list\n");
        }
        uint32_t kind = joint[1] == 3   ? 0u
                        : joint[1] == 8 ? 1u
                        : joint[1] == 1 ? 2u
                                        : 3u;
        uint32_t flag = ((const uint8_t *)joint)[0x3c];
        cycle_initialized[joint[16] - 1] |= flag != 0;
        const uint32_t *first = (const uint32_t *)(uintptr_t)joint[12];
        const uint32_t *second = (const uint32_t *)(uintptr_t)joint[13];
        const uint32_t metadata[] = {
            joint[16],
            kind,
            first[37],
            second[37],
            joint[2] ? ((const uint32_t *)(uintptr_t)joint[2])[16] : 0,
            joint[3] ? ((const uint32_t *)(uintptr_t)joint[3])[16] : 0,
            flag,
            ((const uint8_t *)joint)[0x3d],
            cycle_initialized[joint[16] - 1] ? 1u : 0u};
        print_words(metadata, 9);
        print_joint_island_state(kind, joint, cycle_initialized[joint[16] - 1]);
        joint = (const uint32_t *)(uintptr_t)joint[3];
    }
    if (visited != count) {
        fail("Native joint cycle count disagrees\n");
    }
}

static void cycle_remove_body(LifecycleFunctions *functions, uint32_t *world,
                              size_t index) {
    for (size_t fixture = 0; fixture < 6; ++fixture) {
        const uint32_t *current =
            (const uint32_t *)(uintptr_t)lifecycle_fixtures[fixture];
        if (current && current[2] == lifecycle_bodies[index]) {
            lifecycle_fixtures[fixture] = 0;
        }
    }
    functions->destroy_body(world, (void *)(uintptr_t)lifecycle_bodies[index]);
    lifecycle_bodies[index] = 0;
}

static void cycle_apply(LifecycleFunctions *functions, uint32_t *world,
                        const uint32_t operation[4], uint32_t scene, uint32_t frame) {
    typedef void *(*CreateJoint)(void *, const void *);
    typedef void (*DestroyJoint)(void *, void *);
    CreateJoint create =
        (CreateJoint)find_function("_ZN7b2World11CreateJointEPK10b2JointDef");
    DestroyJoint destroy =
        (DestroyJoint)find_function("_ZN7b2World12DestroyJointEP7b2Joint");
    uint32_t slot = operation[1];
    switch (operation[0]) {
        case 0:
            break;
        case 1:
        case 3: {
            uint32_t input[41], definition[18] = {0};
            world_joint_cycle_input(scene, frame, operation, input);
            prepare_joint_definition(input, scene * 256 + frame, slot, lifecycle_bodies,
                                     definition);
            definition[1] = slot + 1;
            definition[4] = (scene + slot) & 1u;
            print_words(input, 41);
            if (operation[0] == 3) {
                world[0x191d4 / 4] |= 2u;
            }
            uint32_t *joint = create(world, definition);
            world[0x191d4 / 4] &= ~UINT32_C(2);
            if ((operation[0] == 3) != (joint == NULL)) {
                fail("Native locked joint creation disagrees\n");
            }
            if (joint) {
                cycle_joints[slot] = (uint32_t)(uintptr_t)joint;
                cycle_initialized[slot] = false;
            }
            break;
        }
        case 2:
        case 4:
        case 11: {
            uint32_t *joint = (uint32_t *)(uintptr_t)cycle_joints[slot];
            if (!joint) {
                fail("Native cycle references a missing joint\n");
            }
            if (operation[0] == 11) {
                uint32_t *first = (uint32_t *)(uintptr_t)joint[12];
                uint32_t *second = (uint32_t *)(uintptr_t)joint[13];
                first[1] &= ~UINT32_C(2);
                first[36] = UINT32_C(0x3e800000);
                second[1] |= 2u;
                second[36] = UINT32_C(0x3e000000);
            }
            if (operation[0] == 4) {
                world[0x191d4 / 4] |= 2u;
            }
            destroy(world, joint);
            world[0x191d4 / 4] &= ~UINT32_C(2);
            if (operation[0] != 4) {
                cycle_joints[slot] = 0;
            }
            break;
        }
        case 5:
            cycle_remove_body(functions, world, slot);
            break;
        case 6: {
            uint32_t definition[13], specification[13];
            prepare_step_body(definition, scene, slot);
            print_words(definition, 13);
            uint32_t *body = functions->create_body(world, definition);
            body[2] = 0;
            lifecycle_bodies[slot] = (uint32_t)(uintptr_t)body;
            prepare_step_fixture(specification, scene, slot, slot, false);
            print_words(specification, 13);
            lifecycle_create_fixture(functions, specification);
            break;
        }
        case 7:
        case 8:
            functions->set_active((void *)(uintptr_t)lifecycle_bodies[slot],
                                  operation[0] == 8);
            break;
        case 9:
            world[0x191d4 / 4] |= 2u;
            functions->destroy_body(world, (void *)(uintptr_t)lifecycle_bodies[slot]);
            world[0x191d4 / 4] &= ~UINT32_C(2);
            break;
        case 10:
            for (size_t index = 0; index < 6; ++index) {
                if (cycle_joints[index]) {
                    destroy(world, (void *)(uintptr_t)cycle_joints[index]);
                    cycle_joints[index] = 0;
                }
            }
            break;
        default:
            fail("Unknown native joint cycle operation\n");
    }
}

static void probe_world_joint_lifecycle(void) {
    LifecycleFunctions functions = {
        .create_body = (void *(*)(void *, const void *))find_function(
            "_ZN7b2World10CreateBodyEPK9b2BodyDef"),
        .create_fixture = (void *(*)(void *, const void *))find_function(
            "_ZN6b2Body13CreateFixtureEPK12b2FixtureDef"),
        .destroy_body = (void (*)(void *, void *))find_function(
            "_ZN7b2World11DestroyBodyEP6b2Body"),
        .set_box = (void (*)(void *, float, float))find_function(
            "_ZN14b2PolygonShape8SetAsBoxEff"),
        .set_active = (void (*)(void *, bool))find_function("_ZN6b2Body9SetActiveEb"),
        .circle_vtable = (const uint32_t *)find_symbol("_ZTV13b2CircleShape", 1),
        .polygon_vtable = (const uint32_t *)find_symbol("_ZTV14b2PolygonShape", 1),
    };
    void (*construct)(void *, const void *) =
        (void (*)(void *, const void *))find_function("_ZN7b2WorldC1ERK6b2Vec2");
    void (*close_world)(void *) = (void (*)(void *))find_function("_ZN7b2WorldD1Ev");
    void (*step)(void *, float, int, int) =
        (void (*)(void *, float, int, int))find_function("_ZN7b2World4StepEfii");
    uint32_t listener_vtable[] = {0,
                                  0,
                                  (uint32_t)(uintptr_t)world_begin_event,
                                  (uint32_t)(uintptr_t)world_end_event,
                                  (uint32_t)(uintptr_t)world_pre_event,
                                  (uint32_t)(uintptr_t)world_post_event};
    uint32_t listener[] = {(uint32_t)(uintptr_t)listener_vtable};
    uint32_t destruction_vtable[] = {0, 0, (uint32_t)(uintptr_t)cycle_joint_goodbye,
                                     (uint32_t)(uintptr_t)lifecycle_goodbye};
    uint32_t destruction[] = {(uint32_t)(uintptr_t)destruction_vtable};
    static uint64_t world_storage[16384];
    uint32_t *world = (uint32_t *)world_storage;
    semihost(4, "AB_ANDROID_WORLD_JOINT_LIFECYCLE_V1 16 256\n");
    for (uint32_t scene = 0; scene < 16; ++scene) {
        uint32_t controls[] = {5,
                               5,
                               scene % 7 != 0 ? 1u : 0u,
                               scene % 3 != 0 ? 1u : 0u,
                               scene % 4 != 0 ? 1u : 0u,
                               scene >= 8 ? UINT32_C(0x3f000000) : 0,
                               UINT32_C(0xc1200000)};
        construct(world, controls + 5);
        world[0x19220 / 4] = (uint32_t)(uintptr_t)listener;
        world[0x19244 / 4] = (uint32_t)(uintptr_t)destruction;
        ((uint8_t *)world)[0x19240] = (uint8_t)controls[2];
        ((uint8_t *)world)[0x19250] = (uint8_t)controls[3];
        if (!controls[4]) {
            world[0x191d4 / 4] &= ~UINT32_C(4);
        }
        for (size_t index = 0; index < 6; ++index) {
            lifecycle_fixtures[index] = cycle_joints[index] = 0;
            cycle_initialized[index] = false;
        }
        print_words(controls, 7);
        for (size_t index = 0; index < 5; ++index) {
            uint32_t definition[13];
            prepare_step_body(definition, scene, index);
            print_words(definition, 13);
            uint32_t *body = functions.create_body(world, definition);
            body[2] = 0;
            lifecycle_bodies[index] = (uint32_t)(uintptr_t)body;
        }
        for (size_t index = 0; index < 5; ++index) {
            uint32_t specification[13];
            prepare_step_fixture(specification, scene, index, index, false);
            print_words(specification, 13);
            lifecycle_create_fixture(&functions, specification);
        }
        world_event_count = 0;
        print_lifecycle_world(world);
        print_cycle_joints(world);
        for (uint32_t frame = 0; frame < 256; ++frame) {
            uint32_t operation[4];
            world_joint_cycle_operation(frame, operation);
            print_words(operation, 4);
            world_event_count = 0;
            cycle_apply(&functions, world, operation, scene, frame);
            print_lifecycle_world(world);
            print_cycle_joints(world);
            const uint32_t timestep[] = {frame % 13 == 0 ? 0u : UINT32_C(0x3c888889),
                                         frame % 11 == 0 ? 0u : 8u,
                                         frame % 7 == 0 ? 0u : 3u};
            print_words(timestep, 3);
            FloatBits seconds = {.bits = timestep[0]};
            world_event_count = 0;
            step(world, seconds.number, (int)timestep[1], (int)timestep[2]);
            print_lifecycle_world(world);
            print_cycle_joints(world);
        }
        close_world(world);
    }
    semihost(4, "AB world joint lifecycle reference complete\n");
}

#endif
