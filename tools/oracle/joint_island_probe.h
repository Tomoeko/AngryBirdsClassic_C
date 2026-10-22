#ifndef AB_ORACLE_JOINT_ISLAND_H
#define AB_ORACLE_JOINT_ISLAND_H

#include "../../tests/joint_island_cases.h"
#include "island_contacts.h"

#include <stdbool.h>

#include "joint_native_state.h"

static void probe_joint_island(void) {
    typedef void (*ConstructAllocator)(void *);
    typedef void (*ConstructIsland)(void *, int, int, int, void *, void *);
    typedef void (*DestroyIsland)(void *);
    typedef void (*SolveIsland)(void *, void *, const void *, const void *, unsigned);
    typedef void (*ConstructJoint)(void *, const void *);
    ConstructAllocator construct_allocator =
        (ConstructAllocator)find_function("_ZN16b2StackAllocatorC1Ev");
    ConstructIsland construct_island = (ConstructIsland)find_function(
        "_ZN8b2IslandC1EiiiP16b2StackAllocatorP17b2ContactListener");
    DestroyIsland destroy_island = (DestroyIsland)find_function("_ZN8b2IslandD1Ev");
    SolveIsland solve = (SolveIsland)find_function(
        "_ZN8b2Island5SolveEP9b2ProfileRK10b2TimeStepRK6b2Vec2b");
    ConstructJoint constructors[] = {
        (ConstructJoint)find_function("_ZN15b2DistanceJointC2EPK18b2DistanceJointDef"),
        (ConstructJoint)find_function("_ZN11b2WeldJointC2EPK14b2WeldJointDef"),
        (ConstructJoint)find_function("_ZN15b2RevoluteJointC2EPK18b2RevoluteJointDef"),
        (ConstructJoint)find_function(
            "_ZN16b2PrismaticJointC2EPK19b2PrismaticJointDef"),
    };
    const uint32_t *translation = (const uint32_t *)find_symbol("b2_maxTranslation", 1);
    const uint32_t *translation_squared =
        (const uint32_t *)find_symbol("b2_maxTranslationSquared", 1);
    static uint64_t allocator[16384];
    uint32_t random = UINT32_C(0x6d12b9f4);
    const uint32_t seconds[] = {0, UINT32_C(0x3d088889), UINT32_C(0x3c888889),
                                UINT32_C(0x3e000000)};
    const uint32_t ratios[] = {0, UINT32_C(0x3f000000), UINT32_C(0x3f800000),
                               UINT32_C(0x40000000)};
    semihost(4, "AB_ANDROID_JOINT_ISLAND_V1 4096\n");
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        uint32_t controls[] = {sample,
                               2 + sample % 3,
                               (sample / 16) % 4,
                               1 + sample % 3,
                               seconds[(sample / 4) % 4],
                               random_float(&random, 126, 10, 1),
                               random_float(&random, 128, 10, 1),
                               (sample >> 6) & 1u,
                               (const uint32_t[]){0, 1, 3, 10}[(sample / 32) % 4],
                               (const uint32_t[]){0, 1, 4, 10}[(sample / 8) % 4],
                               sample & 1u,
                               ratios[(sample / 2) % 4],
                               *translation,
                               *translation_squared};
        if (sample % 7 == 0) {
            controls[5] = controls[6] = 0;
        }
        uint32_t bodies[4][64] = {{0}}, joint_input[3][41];
        uint32_t guarded_joints[3][68], before_joints[3][64];
        uint32_t definitions[3][18] = {{0}}, before_definitions[3][18];
        OracleIslandContact contacts[3] = {0};
        uint32_t island[13] = {0}, profile[8] = {0};
        construct_allocator(allocator);
        construct_island(island, 4, 3, 3, allocator, NULL);
        uint32_t *body_pointers = (uint32_t *)(uintptr_t)island[2];
        uint32_t *contact_pointers = (uint32_t *)(uintptr_t)island[3];
        uint32_t *joint_pointers = (uint32_t *)(uintptr_t)island[4];
        island[7] = controls[1];
        island[8] = controls[3];
        island[9] = controls[2];
        print_words(controls, 14);
        for (size_t body = 0; body < controls[1]; ++body) {
            uint32_t *current = bodies[body];
            current[0] = body ? (sample / 32 + (uint32_t)body) % 3 : 2;
            current[1] = UINT32_C(0xa500002e);
            if ((sample + body) % 11 == 0) {
                current[1] &= ~UINT32_C(4);
            }
            current[2] = (uint32_t)body;
            current[6] = UINT32_C(0x3f800000);
            current[11] = current[3] = random_float(&random, 126, 12, 1);
            current[12] = current[4] = random_float(&random, 126, 12, 1);
            current[9] = random_float(&random, 124, 12, 1);
            current[10] = random_float(&random, 124, 12, 1);
            current[13] = random_float(&random, 122, 12, 1);
            for (size_t field = 16; field <= 21; ++field) {
                current[field] = random_float(&random, 126, 12, 1);
            }
            if (sample % 7 == 0) {
                for (size_t field = 16; field <= 21; ++field) {
                    current[field] = 0;
                }
            }
            current[29] = current[31] = UINT32_C(0x3f800000);
            current[30] = current[0] == 2 ? UINT32_C(0x3f800000) : 0;
            current[32] = current[0] == 2 && sample % 5 ? UINT32_C(0x3f800000) : 0;
            current[33] = random_float(&random, 122, 8, 0);
            current[34] = random_float(&random, 122, 8, 0);
            current[35] = UINT32_C(0x3f800000);
            current[36] = (sample + body) % 3 ? 0 : UINT32_C(0x3e800000);
            body_pointers[body] = (uint32_t)(uintptr_t)current;
            print_words(current, 64);
        }
        for (size_t contact = 0; contact < controls[2]; ++contact) {
            prepare_island_contact(contacts + contact, body_pointers, controls[1],
                                   sample, contact, &random);
            contact_pointers[contact] = (uint32_t)(uintptr_t)contacts[contact].native;
            print_words(contacts[contact].data, 23);
        }
        for (uint32_t index = 0; index < controls[3]; ++index) {
            uint32_t *input = joint_input[index];
            joint_island_input(sample, index, controls[1], input);
            uint32_t kind = input[0], *data = input + 3;
            uint32_t *definition = definitions[index];
            prepare_joint_definition(input, sample, index, body_pointers, definition);
            for (size_t field = 0; field < 18; ++field) {
                before_definitions[index][field] = definition[field];
            }
            uint32_t *guarded = guarded_joints[index];
            for (size_t field = 0; field < 68; ++field) {
                guarded[field] = UINT32_C(0x4f123456);
            }
            uint32_t *joint = guarded + 2;
            constructors[kind](joint, definition);
            size_t impulse = (const size_t[]){25, 26, 21, 26}[kind];
            for (size_t field = 0; field < (kind == 0 ? 1u : 3u); ++field) {
                joint[impulse + field] = data[19 + field];
            }
            if (kind >= 2) {
                joint[kind == 2 ? 24 : 29] = data[28];
                joint[kind == 2 ? 56 : 35] = data[31];
            }
            for (size_t field = 0; field < 64; ++field) {
                before_joints[index][field] = joint[field];
            }
            joint_pointers[index] = (uint32_t)(uintptr_t)joint;
            print_words(input, 41);
        }
        uint32_t timestep[] = {controls[4], 0,           controls[11],
                               controls[9], controls[8], controls[10]};
        solve(island, profile, timestep, controls + 5, controls[7]);
        for (size_t body = 0; body < controls[1]; ++body) {
            print_words(bodies[body], 64);
        }
        for (size_t contact = 0; contact < controls[2]; ++contact) {
            print_words(contacts[contact].native + 16, 16);
        }
        for (size_t index = 0; index < controls[3]; ++index) {
            uint32_t kind = joint_input[index][0];
            uint32_t *guarded = guarded_joints[index], *joint = guarded + 2;
            size_t indices = (const size_t[]){27, 29, 32, 36}[kind];
            if (joint[indices] != joint_input[index][1] ||
                joint[indices + 1] != joint_input[index][2]) {
                fail("Original joint island cached different endpoint indices\n");
            }
            for (size_t field = 0; field < 64; ++field) {
                bool mutable =
                    kind == 0 ? field == 19 || field == 24 || field == 25 ||
                                    (field >= 27 && field <= 43)
                    : kind == 1 ? field == 19 || (field >= 25 && field <= 51)
                    : kind == 2
                        ? (field >= 21 && field <= 24) || (field >= 32 && field <= 56)
                        : (field >= 26 && field <= 29) || field >= 35;
                if (!mutable && joint[field] != before_joints[index][field]) {
                    fail("Original joint island changed a preserved joint field\n");
                }
            }
            for (size_t field = 0; field < 18; ++field) {
                if (definitions[index][field] != before_definitions[index][field]) {
                    fail("Original joint island changed a joint definition\n");
                }
            }
            if (guarded[0] != UINT32_C(0x4f123456) ||
                guarded[1] != UINT32_C(0x4f123456) ||
                guarded[66] != UINT32_C(0x4f123456) ||
                guarded[67] != UINT32_C(0x4f123456)) {
                fail("Original joint island exceeded joint storage\n");
            }
            print_joint_island_state(kind, joint, true);
        }
        destroy_island(island);
    }
    semihost(4, "AB joint island reference complete\n");
}

#endif
