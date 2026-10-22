#ifndef AB_ORACLE_JOINT_GEOMETRY_H
#define AB_ORACLE_JOINT_GEOMETRY_H

#include <stdbool.h>

#include "../../tests/joint_geometry_cases.h"

static void probe_joint_geometry(void) {
    typedef void (*SharedAnchor)(void *, void *, void *, const void *);
    typedef void (*SeparateInputs)(void *, void *, void *, const void *, const void *);
    SharedAnchor weld = (SharedAnchor)find_function(
        "_ZN14b2WeldJointDef10InitializeEP6b2BodyS1_RK6b2Vec2");
    SharedAnchor revolute = (SharedAnchor)find_function(
        "_ZN18b2RevoluteJointDef10InitializeEP6b2BodyS1_RK6b2Vec2");
    SeparateInputs distance = (SeparateInputs)find_function(
        "_ZN18b2DistanceJointDef10InitializeEP6b2BodyS1_RK6b2Vec2S4_");
    SeparateInputs prismatic = (SeparateInputs)find_function(
        "_ZN19b2PrismaticJointDef10InitializeEP6b2BodyS1_RK6b2Vec2S4_");
    semihost(4, "AB_ANDROID_JOINT_GEOMETRY_V1 4096 4\n");
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        uint32_t input[16], bodies[2][64], before[2][64];
        joint_geometry_case(sample, input);
        for (size_t body = 0; body < 2; ++body) {
            for (size_t field = 0; field < 64; ++field) {
                bodies[body][field] = UINT32_C(0x4f123456);
            }
            for (size_t field = 0; field < 4; ++field) {
                bodies[body][3 + field] = input[body * 5 + field];
            }
            bodies[body][14] = input[body * 5 + 4];
            for (size_t field = 0; field < 64; ++field) {
                before[body][field] = bodies[body][field];
            }
        }
        print_words(&sample, 1);
        print_words(input, 16);
        for (size_t kind = 0; kind < 4; ++kind) {
            uint32_t definition[40], output[8] = {0};
            for (size_t field = 0; field < 40; ++field) {
                definition[field] = UINT32_C(0x4f654321);
            }
            if (kind == 0) {
                distance(definition, bodies[0], bodies[1], input + 10, input + 12);
            } else if (kind == 1) {
                weld(definition, bodies[0], bodies[1], input + 10);
            } else if (kind == 2) {
                revolute(definition, bodies[0], bodies[1], input + 10);
            } else {
                prismatic(definition, bodies[0], bodies[1], input + 10, input + 14);
            }
            size_t last = kind == 3 ? 11 : 9;
            for (size_t field = 0; field < 40; ++field) {
                if (field == 2 || field == 3 || (field >= 5 && field <= last)) {
                    continue;
                }
                if (definition[field] != UINT32_C(0x4f654321)) {
                    fail("Original joint initialization changed an unused word\n");
                }
            }
            if (definition[2] != (uint32_t)(uintptr_t)bodies[0] ||
                definition[3] != (uint32_t)(uintptr_t)bodies[1]) {
                fail("Original joint initialization returned different bodies\n");
            }
            for (size_t field = 0; field < 4; ++field) {
                output[field] = definition[5 + field];
            }
            if (kind == 0) {
                output[7] = definition[9];
            } else if (kind == 3) {
                output[4] = definition[9];
                output[5] = definition[10];
                output[6] = definition[11];
            } else {
                output[6] = definition[9];
            }
            print_words(output, 8);
            for (size_t body = 0; body < 2; ++body) {
                for (size_t field = 0; field < 64; ++field) {
                    if (bodies[body][field] != before[body][field]) {
                        fail("Original joint initialization changed a body\n");
                    }
                }
            }
        }
    }
    semihost(4, "AB joint geometry reference complete\n");
}

#endif
