#ifndef AB_ORACLE_DISTANCE_JOINT_H
#define AB_ORACLE_DISTANCE_JOINT_H

#include "../../tests/distance_cases.h"

static void print_distance_state(const uint32_t *joint, const uint32_t velocities[6]) {
    uint32_t output[24];
    for (size_t field = 0; field < 4; ++field) {
        output[field] = joint[31 + field];
        output[4 + field] = joint[35 + field];
        output[8 + field] = joint[39 + field];
    }
    output[12] = joint[29];
    output[13] = joint[30];
    output[14] = joint[43];
    output[15] = joint[24];
    output[16] = joint[19];
    output[17] = joint[25];
    for (size_t field = 0; field < 6; ++field) {
        output[18 + field] = velocities[field];
    }
    print_words(output, 24);
}

static void probe_distance_joint(void) {
    typedef void (*Call)(void *, const void *);
    typedef int (*Position)(void *, const void *);
    Call construct =
        (Call)find_function("_ZN15b2DistanceJointC2EPK18b2DistanceJointDef");
    Call initialize = (Call)find_function(
        "_ZN15b2DistanceJoint23InitVelocityConstraintsERK12b2SolverData");
    Call velocity = (Call)find_function(
        "_ZN15b2DistanceJoint24SolveVelocityConstraintsERK12b2SolverData");
    Position position = (Position)find_function(
        "_ZN15b2DistanceJoint24SolvePositionConstraintsERK12b2SolverData");
    semihost(4, "AB_ANDROID_DISTANCE_JOINT_V1 4096 8\n");
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        uint32_t input[32];
        distance_case(sample, input);
        uint32_t bodies[2][64] = {{0}}, before_bodies[2][64];
        for (size_t body = 0; body < 2; ++body) {
            size_t index = body * 5;
            bodies[body][0] = 2;
            bodies[body][2] = (uint32_t)body;
            bodies[body][7] = input[index];
            bodies[body][8] = input[index + 1];
            bodies[body][14] = input[index + 2];
            bodies[body][30] = input[index + 3];
            bodies[body][32] = input[index + 4];
            for (size_t field = 0; field < 64; ++field) {
                before_bodies[body][field] = bodies[body][field];
            }
        }
        uint32_t definition[] = {3,
                                 0,
                                 (uint32_t)(uintptr_t)bodies[0],
                                 (uint32_t)(uintptr_t)bodies[1],
                                 0,
                                 input[10],
                                 input[11],
                                 input[12],
                                 input[13],
                                 input[14],
                                 input[15],
                                 input[16]};
        uint32_t before_definition[12];
        for (size_t field = 0; field < 12; ++field) {
            before_definition[field] = definition[field];
        }
        uint32_t guarded[68];
        for (size_t field = 0; field < 68; ++field) {
            guarded[field] = UINT32_C(0x4f123456);
        }
        uint32_t *joint = guarded + 2;
        construct(joint, definition);
        joint[25] = input[19];
        uint32_t before_joint[64];
        for (size_t field = 0; field < 64; ++field) {
            before_joint[field] = joint[field];
        }
        uint32_t positions[] = {input[28], input[29], input[2],
                                input[30], input[31], input[7]};
        uint32_t before_positions[6], velocities[6];
        for (size_t field = 0; field < 6; ++field) {
            before_positions[field] = positions[field];
            velocities[field] = input[22 + field];
        }
        uint32_t solver[] = {input[17],
                             0,
                             input[18],
                             1,
                             1,
                             sample & 1u,
                             (uint32_t)(uintptr_t)positions,
                             (uint32_t)(uintptr_t)velocities};
        uint32_t before_solver[8];
        for (size_t field = 0; field < 8; ++field) {
            before_solver[field] = solver[field];
        }
        initialize(joint, solver);
        for (size_t field = 0; field < 64; ++field) {
            if (field != 19 && field != 24 && field != 25 &&
                (field < 27 || field > 43) && joint[field] != before_joint[field]) {
                fail("Original distance initialization changed a preserved field\n");
            }
        }
        if (joint[27] != 0 || joint[28] != 1 || joint[35] != input[0] ||
            joint[36] != input[1] || joint[37] != input[5] || joint[38] != input[6] ||
            joint[39] != input[3] || joint[40] != input[8] || joint[41] != input[4] ||
            joint[42] != input[9]) {
            fail("Original distance body cache disagrees\n");
        }
        uint32_t iterations = sample < 192 ? sample % 8 + 1 : sample % 9;
        uint32_t metadata[] = {sample, sample & 1u, iterations};
        print_words(metadata, 3);
        print_words(input, 32);
        print_distance_state(joint, velocities);
        uint32_t initialized[64];
        for (size_t field = 0; field < 64; ++field) {
            initialized[field] = joint[field];
        }
        for (uint32_t pass = 0; pass < iterations; ++pass) {
            velocity(joint, solver);
            print_distance_state(joint, velocities);
            for (size_t field = 0; field < 64; ++field) {
                if (field != 25 && joint[field] != initialized[field]) {
                    fail("Original distance velocity changed cached fields\n");
                }
            }
        }
        uint32_t before_velocities[6];
        for (size_t field = 0; field < 6; ++field) {
            before_velocities[field] = velocities[field];
            if (positions[field] != before_positions[field]) {
                fail("Original distance velocity changed positions\n");
            }
        }
        print_words(positions, 6);
        for (size_t field = 0; field < 64; ++field) {
            initialized[field] = joint[field];
        }
        for (uint32_t pass = 0; pass < iterations; ++pass) {
            int result = position(joint, solver);
            if (result < 0 || result > 1) {
                fail("Original distance position did not return a boolean\n");
            }
            uint32_t output[7] = {(uint32_t)result};
            for (size_t field = 0; field < 6; ++field) {
                output[field + 1] = positions[field];
                if (velocities[field] != before_velocities[field]) {
                    fail("Original distance position changed velocities\n");
                }
            }
            print_words(output, 7);
            for (size_t field = 0; field < 64; ++field) {
                if (joint[field] != initialized[field]) {
                    fail("Original distance position changed joint state\n");
                }
            }
        }
        for (size_t body = 0; body < 2; ++body) {
            for (size_t field = 0; field < 64; ++field) {
                if (bodies[body][field] != before_bodies[body][field]) {
                    fail("Original distance solver changed a source body\n");
                }
            }
        }
        for (size_t field = 0; field < 12; ++field) {
            if (definition[field] != before_definition[field]) {
                fail("Original distance solver changed its definition\n");
            }
        }
        for (size_t field = 0; field < 8; ++field) {
            if (solver[field] != before_solver[field]) {
                fail("Original distance solver changed solver controls\n");
            }
        }
        if (guarded[0] != UINT32_C(0x4f123456) || guarded[1] != UINT32_C(0x4f123456) ||
            guarded[66] != UINT32_C(0x4f123456) ||
            guarded[67] != UINT32_C(0x4f123456)) {
            fail("Original distance solver exceeded joint bounds\n");
        }
    }
    semihost(4, "AB distance joint reference complete\n");
}

#endif
