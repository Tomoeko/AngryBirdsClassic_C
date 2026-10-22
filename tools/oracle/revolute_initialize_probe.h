#ifndef AB_ORACLE_REVOLUTE_INITIALIZE_H
#define AB_ORACLE_REVOLUTE_INITIALIZE_H

#include "../../tests/revolute_initialize_cases.h"
#if defined(AB_ORACLE_REVOLUTE_POSITION)
#include "../../tests/revolute_position_cases.h"
#endif

static void print_revolute_state(const uint32_t *joint, const uint32_t velocities[6]) {
    uint32_t output[33];
    for (size_t field = 0; field < 12; ++field) {
        output[field] = joint[34 + field];
    }
    for (size_t field = 0; field < 9; ++field) {
        output[12 + field] = joint[46 + field];
    }
    output[21] = joint[55];
    for (size_t field = 0; field < 4; ++field) {
        output[22 + field] = joint[21 + field];
    }
    for (size_t field = 0; field < 6; ++field) {
        output[26 + field] = velocities[field];
    }
    output[32] = joint[56];
    print_words(output, 33);
}

static void probe_revolute_initialize(void) {
    typedef void (*Call)(void *, const void *);
    Call construct =
        (Call)find_function("_ZN15b2RevoluteJointC2EPK18b2RevoluteJointDef");
    Call initialize = (Call)find_function(
        "_ZN15b2RevoluteJoint23InitVelocityConstraintsERK12b2SolverData");
#if defined(AB_ORACLE_REVOLUTE_POSITION)
    typedef int (*Position)(void *, const void *);
    Position solve_position = (Position)find_function(
        "_ZN15b2RevoluteJoint24SolvePositionConstraintsERK12b2SolverData");
    semihost(4, "AB_ANDROID_REVOLUTE_POSITION_V1 4096 8\n");
#elif defined(AB_ORACLE_REVOLUTE_VELOCITY)
    Call solve = (Call)find_function(
        "_ZN15b2RevoluteJoint24SolveVelocityConstraintsERK12b2SolverData");
    semihost(4, "AB_ANDROID_REVOLUTE_VELOCITY_V1 4096 8\n");
#else
    semihost(4, "AB_ANDROID_REVOLUTE_INITIALIZE_V1 4096\n");
#endif
    for (uint32_t sample = 0; sample < 4096; ++sample) {
#if defined(AB_ORACLE_REVOLUTE_POSITION)
        uint32_t input[36];
        revolute_position_case(sample, input);
#else
        uint32_t input[32];
        revolute_initialize_case(sample, input);
#endif
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
        uint32_t warm = sample & 1u, motor = (sample >> 1) & 1u;
#if defined(AB_ORACLE_REVOLUTE_POSITION)
        uint32_t limit = revolute_position_limit_enabled(sample);
#else
        uint32_t limit = (sample >> 2) & 1u;
#endif
        uint32_t definition[] = {1,
                                 0,
                                 (uint32_t)(uintptr_t)bodies[0],
                                 (uint32_t)(uintptr_t)bodies[1],
                                 0,
                                 input[10],
                                 input[11],
                                 input[12],
                                 input[13],
                                 input[14],
                                 limit,
                                 input[15],
                                 input[16],
                                 motor,
                                 input[30],
                                 input[29]};
        uint32_t before_definition[16];
        for (size_t field = 0; field < 16; ++field) {
            before_definition[field] = definition[field];
        }
        uint32_t guarded[68];
        for (size_t field = 0; field < 68; ++field) {
            guarded[field] = UINT32_C(0x4f123456);
        }
        uint32_t *joint = guarded + 2;
        construct(joint, definition);
        for (size_t field = 0; field < 3; ++field) {
            joint[21 + field] = input[19 + field];
        }
        joint[24] = input[28];
        joint[56] = input[31];
        uint32_t before_joint[64];
        for (size_t field = 0; field < 64; ++field) {
            before_joint[field] = joint[field];
        }
#if defined(AB_ORACLE_REVOLUTE_POSITION)
        uint32_t positions[] = {input[32], input[33], input[2],
                                input[34], input[35], input[7]};
#else
        uint32_t positions[] = {0, 0, input[2], 0, 0, input[7]};
#endif
        uint32_t velocities[6];
        for (size_t field = 0; field < 6; ++field) {
            velocities[field] = input[22 + field];
        }
        uint32_t solver[] = {input[17],
                             0,
                             input[18],
                             1,
                             1,
                             warm,
                             (uint32_t)(uintptr_t)positions,
                             (uint32_t)(uintptr_t)velocities};
        uint32_t before_solver[8];
        for (size_t field = 0; field < 8; ++field) {
            before_solver[field] = solver[field];
        }
        initialize(joint, solver);
        for (size_t field = 0; field < 64; ++field) {
            if ((field < 21 || field > 24) && (field < 32 || field > 56) &&
                joint[field] != before_joint[field]) {
                fail("Original revolute initialization changed a preserved field\n");
            }
        }
        if (joint[32] != 0 || joint[33] != 1 || joint[56] > 3) {
            fail("Original revolute body indices/limit state disagree\n");
        }
#if defined(AB_ORACLE_REVOLUTE_POSITION)
        uint32_t iterations = sample < 256 ? sample % 8 + 1 : sample % 9;
        uint32_t metadata[] = {sample, warm, motor, limit, iterations};
#elif defined(AB_ORACLE_REVOLUTE_VELOCITY)
        uint32_t iterations = sample % 9;
        uint32_t metadata[] = {sample, warm, motor, limit, iterations};
#else
        uint32_t metadata[] = {sample, warm, motor, limit};
#endif
        print_words(metadata, sizeof(metadata) / sizeof(*metadata));
        print_words(input, sizeof(input) / sizeof(*input));
        print_revolute_state(joint, velocities);
#if defined(AB_ORACLE_REVOLUTE_VELOCITY)
        for (size_t field = 0; field < 64; ++field) {
            before_joint[field] = joint[field];
        }
        for (uint32_t pass = 0; pass < iterations; ++pass) {
            solve(joint, solver);
            print_revolute_state(joint, velocities);
            for (size_t field = 0; field < 64; ++field) {
                if ((field < 21 || field > 24) && joint[field] != before_joint[field]) {
                    fail("Original revolute velocity changed a cached field\n");
                }
            }
        }
#elif defined(AB_ORACLE_REVOLUTE_POSITION)
        print_words(positions, 6);
        uint32_t before_velocities[6];
        for (size_t field = 0; field < 64; ++field) {
            before_joint[field] = joint[field];
        }
        for (size_t field = 0; field < 6; ++field) {
            before_velocities[field] = velocities[field];
        }
        for (uint32_t pass = 0; pass < iterations; ++pass) {
            int result = solve_position(joint, solver);
            if (result < 0 || result > 1) {
                fail("Original revolute position result is not boolean\n");
            }
            uint32_t output[7] = {(uint32_t)result};
            for (size_t field = 0; field < 6; ++field) {
                output[field + 1] = positions[field];
                if (velocities[field] != before_velocities[field]) {
                    fail("Original revolute position changed velocities\n");
                }
            }
            print_words(output, 7);
            for (size_t field = 0; field < 64; ++field) {
                if (joint[field] != before_joint[field]) {
                    fail("Original revolute position changed joint state\n");
                }
            }
        }
#endif
        for (size_t body = 0; body < 2; ++body) {
            for (size_t field = 0; field < 64; ++field) {
                if (bodies[body][field] != before_bodies[body][field]) {
                    fail("Original revolute initialization changed a source body\n");
                }
            }
        }
        for (size_t field = 0; field < 16; ++field) {
            if (definition[field] != before_definition[field]) {
                fail("Original revolute constructor changed its definition\n");
            }
        }
        for (size_t field = 0; field < 8; ++field) {
            if (solver[field] != before_solver[field]) {
                fail("Original revolute initialization changed solver controls\n");
            }
        }
#if !defined(AB_ORACLE_REVOLUTE_POSITION)
        if (positions[0] != 0 || positions[1] != 0 || positions[2] != input[2] ||
            positions[3] != 0 || positions[4] != 0 || positions[5] != input[7]) {
            fail("Original revolute initialization changed positions\n");
        }
#endif
        if (guarded[0] != UINT32_C(0x4f123456) || guarded[1] != UINT32_C(0x4f123456) ||
            guarded[66] != UINT32_C(0x4f123456) ||
            guarded[67] != UINT32_C(0x4f123456)) {
            fail("Original revolute initialization changed positions or guards\n");
        }
    }
#if defined(AB_ORACLE_REVOLUTE_POSITION)
    semihost(4, "AB revolute position reference complete\n");
#elif defined(AB_ORACLE_REVOLUTE_VELOCITY)
    semihost(4, "AB revolute velocity reference complete\n");
#else
    semihost(4, "AB revolute initialization reference complete\n");
#endif
}

#endif
