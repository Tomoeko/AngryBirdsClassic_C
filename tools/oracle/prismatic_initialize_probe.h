#ifndef AB_ORACLE_PRISMATIC_INITIALIZE_H
#define AB_ORACLE_PRISMATIC_INITIALIZE_H

#include "../../tests/prismatic_initialize_cases.h"
#if defined(AB_ORACLE_PRISMATIC_POSITION)
#include "../../tests/prismatic_position_cases.h"
#endif

static void print_prismatic_state(const uint32_t *joint, const uint32_t velocities[6]) {
    uint32_t output[41];
    for (size_t field = 0; field < 4; ++field) {
        output[field] = joint[21 + field];
    }
    for (size_t field = 0; field < 8; ++field) {
        output[4 + field] = joint[38 + field];
        output[12 + field] = joint[46 + field];
    }
    for (size_t field = 0; field < 10; ++field) {
        output[20 + field] = joint[54 + field];
    }
    for (size_t field = 0; field < 4; ++field) {
        output[30 + field] = joint[26 + field];
    }
    for (size_t field = 0; field < 6; ++field) {
        output[34 + field] = velocities[field];
    }
    output[40] = joint[35];
    print_words(output, 41);
}

static void probe_prismatic_initialize(void) {
    typedef void (*Call)(void *, const void *);
    Call construct =
        (Call)find_function("_ZN16b2PrismaticJointC2EPK19b2PrismaticJointDef");
    Call initialize = (Call)find_function(
        "_ZN16b2PrismaticJoint23InitVelocityConstraintsERK12b2SolverData");
#if defined(AB_ORACLE_PRISMATIC_POSITION)
    typedef int (*Position)(void *, const void *);
    Position solve_position = (Position)find_function(
        "_ZN16b2PrismaticJoint24SolvePositionConstraintsERK12b2SolverData");
    semihost(4, "AB_ANDROID_PRISMATIC_POSITION_V1 4096 8\n");
#elif defined(AB_ORACLE_PRISMATIC_VELOCITY)
    Call solve = (Call)find_function(
        "_ZN16b2PrismaticJoint24SolveVelocityConstraintsERK12b2SolverData");
    semihost(4, "AB_ANDROID_PRISMATIC_VELOCITY_V1 4096 8\n");
#else
    semihost(4, "AB_ANDROID_PRISMATIC_INITIALIZE_V1 4096\n");
#endif
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        uint32_t input[38];
#if defined(AB_ORACLE_PRISMATIC_POSITION)
        prismatic_position_case(sample, input);
#else
        prismatic_initialize_case(sample, input);
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
#if defined(AB_ORACLE_PRISMATIC_POSITION)
        uint32_t limit = prismatic_position_limit_enabled(sample);
#else
        uint32_t limit = (sample >> 2) & 1u;
#endif
        uint32_t definition[] = {2,
                                 0,
                                 (uint32_t)(uintptr_t)bodies[0],
                                 (uint32_t)(uintptr_t)bodies[1],
                                 0,
                                 input[10],
                                 input[11],
                                 input[12],
                                 input[13],
                                 input[32],
                                 input[33],
                                 input[14],
                                 limit,
                                 input[15],
                                 input[16],
                                 motor,
                                 input[29],
                                 input[30]};
        uint32_t before_definition[18];
        for (size_t field = 0; field < 18; ++field) {
            before_definition[field] = definition[field];
        }
        uint32_t guarded[68];
        for (size_t field = 0; field < 68; ++field) {
            guarded[field] = UINT32_C(0x4f123456);
        }
        uint32_t *joint = guarded + 2;
        construct(joint, definition);
        for (size_t field = 0; field < 3; ++field) {
            joint[26 + field] = input[19 + field];
        }
        joint[29] = input[28];
        joint[35] = input[31];
        uint32_t before_joint[64];
        for (size_t field = 0; field < 64; ++field) {
            before_joint[field] = joint[field];
        }
        uint32_t positions[] = {input[34], input[35], input[2],
                                input[36], input[37], input[7]};
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
            if ((field < 26 || field > 29) && field < 35 &&
                joint[field] != before_joint[field]) {
                fail("Original prismatic initialization changed preserved fields\n");
            }
        }
        if (joint[36] != 0 || joint[37] != 1 || joint[35] > 3) {
            fail("Original prismatic body indices/limit state disagree\n");
        }
#if defined(AB_ORACLE_PRISMATIC_POSITION)
        uint32_t iterations = sample < 512 ? sample % 8 + 1 : sample % 9;
        uint32_t metadata[] = {sample, warm, motor, limit, iterations};
#elif defined(AB_ORACLE_PRISMATIC_VELOCITY)
        uint32_t iterations = sample % 9;
        uint32_t metadata[] = {sample, warm, motor, limit, iterations};
#else
        uint32_t metadata[] = {sample, warm, motor, limit};
#endif
        print_words(metadata, sizeof(metadata) / sizeof(*metadata));
        print_words(input, 38);
        print_prismatic_state(joint, velocities);
#if defined(AB_ORACLE_PRISMATIC_VELOCITY)
        for (size_t field = 0; field < 64; ++field) {
            before_joint[field] = joint[field];
        }
        for (uint32_t pass = 0; pass < iterations; ++pass) {
            solve(joint, solver);
            print_prismatic_state(joint, velocities);
            for (size_t field = 0; field < 64; ++field) {
                if ((field < 26 || field > 29) && joint[field] != before_joint[field]) {
                    fail("Original prismatic velocity changed cached fields\n");
                }
            }
        }
#elif defined(AB_ORACLE_PRISMATIC_POSITION)
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
                fail("Original prismatic position result is not boolean\n");
            }
            uint32_t output[7] = {(uint32_t)result};
            for (size_t field = 0; field < 6; ++field) {
                output[field + 1] = positions[field];
                if (velocities[field] != before_velocities[field]) {
                    fail("Original prismatic position changed velocities\n");
                }
            }
            print_words(output, 7);
            for (size_t field = 0; field < 64; ++field) {
                if (joint[field] != before_joint[field]) {
                    fail("Original prismatic position changed joint state\n");
                }
            }
        }
#endif
        for (size_t body = 0; body < 2; ++body) {
            for (size_t field = 0; field < 64; ++field) {
                if (bodies[body][field] != before_bodies[body][field]) {
                    fail("Original prismatic initialization changed a source body\n");
                }
            }
        }
        for (size_t field = 0; field < 18; ++field) {
            if (definition[field] != before_definition[field]) {
                fail("Original prismatic constructor changed its definition\n");
            }
        }
        for (size_t field = 0; field < 8; ++field) {
            if (solver[field] != before_solver[field]) {
                fail("Original prismatic initialization changed solver controls\n");
            }
        }
#if !defined(AB_ORACLE_PRISMATIC_POSITION)
        if (positions[0] != input[34] || positions[1] != input[35] ||
            positions[2] != input[2] || positions[3] != input[36] ||
            positions[4] != input[37] || positions[5] != input[7]) {
            fail("Original prismatic initialization changed positions\n");
        }
#endif
        if (guarded[0] != UINT32_C(0x4f123456) || guarded[1] != UINT32_C(0x4f123456) ||
            guarded[66] != UINT32_C(0x4f123456) ||
            guarded[67] != UINT32_C(0x4f123456)) {
            fail("Original prismatic initialization changed positions or guards\n");
        }
    }
#if defined(AB_ORACLE_PRISMATIC_POSITION)
    semihost(4, "AB prismatic position reference complete\n");
#elif defined(AB_ORACLE_PRISMATIC_VELOCITY)
    semihost(4, "AB prismatic velocity reference complete\n");
#else
    semihost(4, "AB prismatic initialization reference complete\n");
#endif
}

#endif
