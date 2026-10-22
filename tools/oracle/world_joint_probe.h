#ifndef AB_ORACLE_WORLD_JOINT_PROBE_H
#define AB_ORACLE_WORLD_JOINT_PROBE_H

#include "../../tests/joint_island_cases.h"
#include "island_contacts.h"
#include "joint_native_state.h"
#include "world_graph_probe.h"

static void connect_world_joint(uint32_t *joint, const uint32_t input[41],
                                const uint32_t *body_pointers) {
    for (size_t endpoint = 0; endpoint < 2; ++endpoint) {
        uint32_t *body = (uint32_t *)(uintptr_t)body_pointers[input[1 + endpoint]];
        uint32_t *edge = joint + 4 + endpoint * 4;
        edge[0] = body_pointers[input[2 - endpoint]];
        edge[1] = (uint32_t)(uintptr_t)joint;
        edge[2] = 0;
        edge[3] = body[27];
        if (edge[3]) {
            ((uint32_t *)(uintptr_t)edge[3])[2] = (uint32_t)(uintptr_t)edge;
        }
        body[27] = (uint32_t)(uintptr_t)edge;
    }
}

static void probe_world_joint(void) {
    typedef void (*ConstructWorld)(void *, const void *);
    typedef void (*WorldOperation)(void *);
    typedef void (*SolveWorld)(void *, const void *);
    typedef void (*ConstructJoint)(void *, const void *);
    ConstructWorld construct = (ConstructWorld)find_function("_ZN7b2WorldC1ERK6b2Vec2");
    WorldOperation destroy = (WorldOperation)find_function("_ZN7b2WorldD1Ev");
    SolveWorld solve = (SolveWorld)find_function("_ZN7b2World5SolveERK10b2TimeStep");
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
    const uint32_t seconds[] = {0, UINT32_C(0x3d088889), UINT32_C(0x3c888889),
                                UINT32_C(0x3e000000)};
    const uint32_t ratios[] = {0, UINT32_C(0x3f000000), UINT32_C(0x3f800000),
                               UINT32_C(0x40000000)};
    static uint64_t world_storage[16384];
    uint32_t *world = (uint32_t *)world_storage;
    uint32_t random = UINT32_C(0x520e13a7);
    semihost(4, "AB_ANDROID_WORLD_JOINT_V1 4096\n");
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        uint32_t controls[] = {sample,
                               2 + sample % 6,
                               (sample / 6) % 9,
                               (sample / 11) % 7,
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
        uint32_t bodies[7][64] = {{0}}, body_pointers[7] = {0};
        uint32_t inputs[6][41], guarded_joints[6][68], before_joints[6][64];
        OracleIslandContact contacts[8] = {0};
        construct(world, controls + 5);
        world[0x19228 / 4] = (uint32_t)(uintptr_t)bodies;
        world[0x19230 / 4] = controls[1];
        ((uint8_t *)world)[0x19240] = (uint8_t)controls[7];
        for (size_t body = 0; body < controls[1]; ++body) {
            uint32_t *current = bodies[body];
            current[0] = (sample / 32 + (uint32_t)body) % 3;
            current[1] = UINT32_C(0xa5000000) | (random_word(&random) & 0x3f);
            current[2] = random_word(&random);
            current[6] = UINT32_C(0x3f800000);
            current[11] = current[3] = random_float(&random, 126, 12, 1);
            current[12] = current[4] = random_float(&random, 126, 12, 1);
            current[9] = random_float(&random, 124, 12, 1);
            current[10] = random_float(&random, 124, 12, 1);
            current[13] = random_float(&random, 122, 12, 1);
            for (size_t field = 16; field <= 21; ++field) {
                current[field] = random_float(&random, 126, 12, 1);
                if (current[0] == 0 || sample % 7 == 0) {
                    current[field] = 0;
                }
            }
            current[22] = (uint32_t)(uintptr_t)world;
            current[23] = body ? (uint32_t)(uintptr_t)bodies[body - 1] : 0;
            current[24] =
                body + 1 < controls[1] ? (uint32_t)(uintptr_t)bodies[body + 1] : 0;
            current[29] = current[31] = UINT32_C(0x3f800000);
            current[30] = current[0] == 2 ? UINT32_C(0x3f800000) : 0;
            current[32] = current[0] == 2 && sample % 5 ? UINT32_C(0x3f800000) : 0;
            current[33] = random_float(&random, 122, 8, 0);
            current[34] = random_float(&random, 122, 8, 0);
            current[35] = UINT32_C(0x3f800000);
            current[36] = (sample + body) % 3 ? 0 : UINT32_C(0x3e800000);
            body_pointers[body] = (uint32_t)(uintptr_t)current;
        }
        world[0x19218 / 4] = controls[2];
        world[0x19214 / 4] = controls[2] ? (uint32_t)(uintptr_t)contacts[0].native : 0;
        for (size_t cursor = controls[2]; cursor > 0;) {
            size_t index = --cursor;
            OracleIslandContact *contact = contacts + index;
            prepare_island_contact(contact, body_pointers, controls[1], sample, index,
                                   &random);
            if (contact->data[0] == contact->data[1]) {
                contact->data[1] = (contact->data[0] + 1) % controls[1];
                contact->fixtures[1][2] = body_pointers[contact->data[1]];
            }
            contact->native[1] = random_word(&random);
            /* Deactivation removes contacts in the enclosing world. Do not
             * create traversable edges that cannot survive that operation. */
            if (!(bodies[contact->data[0]][1] & 32u) ||
                !(bodies[contact->data[1]][1] & 32u)) {
                contact->native[1] &= ~UINT32_C(2);
            }
            contact->native[3] = index + 1 < controls[2]
                                     ? (uint32_t)(uintptr_t)contacts[index + 1].native
                                     : 0;
            contact->fixtures[0][9] = sample % 11 == index ? UINT32_C(0x10000) : 0;
            connect_world_contact(contact, body_pointers);
        }
        for (uint32_t cursor = controls[3]; cursor > 0;) {
            uint32_t index = --cursor;
            uint32_t *input = inputs[index], definition[18] = {0};
            joint_island_input(sample, index, controls[1], input);
            prepare_joint_definition(input, sample, index, body_pointers, definition);
            uint32_t *guarded = guarded_joints[index], *joint = guarded + 2;
            for (size_t field = 0; field < 68; ++field) {
                guarded[field] = UINT32_C(0x4f123456);
            }
            constructors[input[0]](joint, definition);
            size_t impulse = (const size_t[]){25, 26, 21, 26}[input[0]];
            for (size_t field = 0; field < (input[0] == 0 ? 1u : 3u); ++field) {
                joint[impulse + field] = input[22 + field];
            }
            if (input[0] >= 2) {
                joint[input[0] == 2 ? 24 : 29] = input[31];
                joint[input[0] == 2 ? 56 : 35] = input[34];
            }
            ((uint8_t *)joint)[0x3c] = (uint8_t)((sample + index) & 1u);
            joint[2] = index ? (uint32_t)(uintptr_t)(guarded_joints[index - 1] + 2) : 0;
            joint[3] = index + 1 < controls[3]
                           ? (uint32_t)(uintptr_t)(guarded_joints[index + 1] + 2)
                           : 0;
            connect_world_joint(joint, input, body_pointers);
        }
        world[0x19234 / 4] = controls[3];
        world[0x1922c / 4] =
            controls[3] ? (uint32_t)(uintptr_t)(guarded_joints[0] + 2) : 0;
        print_words(controls, 14);
        for (size_t body = 0; body < controls[1]; ++body) {
            print_world_body(bodies[body]);
        }
        for (size_t index = 0; index < controls[2]; ++index) {
            const uint32_t metadata[] = {contacts[index].native[1],
                                         contacts[index].fixtures[0][9] ? 1u : 0u};
            print_words(metadata, 2);
            print_words(contacts[index].data, 23);
        }
        for (size_t index = 0; index < controls[3]; ++index) {
            uint32_t *joint = guarded_joints[index] + 2;
            const uint32_t flag = ((uint8_t *)joint)[0x3c];
            print_words(&flag, 1);
            print_words(inputs[index], 41);
            for (size_t field = 0; field < 64; ++field) {
                before_joints[index][field] = joint[field];
            }
        }
        uint32_t timestep[] = {controls[4], 0,           controls[11],
                               controls[9], controls[8], controls[10]};
        solve(world, timestep);
        for (size_t body = 0; body < controls[1]; ++body) {
            print_world_body(bodies[body]);
        }
        for (size_t index = 0; index < controls[2]; ++index) {
            print_words(contacts[index].native + 1, 1);
            print_words(contacts[index].native + 16, 16);
        }
        for (size_t index = 0; index < controls[3]; ++index) {
            uint32_t *guarded = guarded_joints[index], *joint = guarded + 2;
            uint32_t kind = inputs[index][0], flag = ((uint8_t *)joint)[0x3c];
            for (size_t field = 0; field < 64; ++field) {
                bool mutable =
                    field == 15 ||
                    (flag &&
                     (kind == 0 ? field == 19 || field == 24 || field == 25 ||
                                      (field >= 27 && field <= 43)
                      : kind == 1 ? field == 19 || (field >= 25 && field <= 51)
                      : kind == 2
                          ? (field >= 21 && field <= 24) || (field >= 32 && field <= 56)
                          : (field >= 26 && field <= 29) || field >= 35));
                if (!mutable && joint[field] != before_joints[index][field]) {
                    fail("Original world solve changed a preserved joint field\n");
                }
            }
            if (guarded[0] != UINT32_C(0x4f123456) ||
                guarded[1] != UINT32_C(0x4f123456) ||
                guarded[66] != UINT32_C(0x4f123456) ||
                guarded[67] != UINT32_C(0x4f123456)) {
                fail("Original world solve exceeded joint storage\n");
            }
            print_words(&flag, 1);
            print_joint_island_state(kind, joint, flag != 0);
        }
        world[0x19228 / 4] = world[0x19230 / 4] = 0;
        world[0x19214 / 4] = world[0x19218 / 4] = 0;
        world[0x1922c / 4] = world[0x19234 / 4] = 0;
        destroy(world);
    }
    semihost(4, "AB world joint reference complete\n");
}

#endif
