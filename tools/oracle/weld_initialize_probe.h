#ifndef AB_ORACLE_WELD_INITIALIZE_H
#define AB_ORACLE_WELD_INITIALIZE_H

#include "../../tests/weld_initialize_cases.h"
#if defined(AB_ORACLE_WELD_POSITION)
#include "../../tests/weld_position_cases.h"
#endif

static void print_weld_state(const uint32_t *joint, const uint32_t velocities[6]) {
    uint32_t output[24];
    for (size_t field = 0; field < 4; ++field) {
        output[field] = joint[31 + field];
    }
    for (size_t field = 0; field < 9; ++field) {
        output[4 + field] = joint[43 + field];
    }
    output[13] = joint[25];
    output[14] = joint[19];
    for (size_t field = 0; field < 3; ++field) {
        output[15 + field] = joint[26 + field];
    }
    for (size_t field = 0; field < 6; ++field) {
        output[18 + field] = velocities[field];
    }
    print_words(output, 24);
}

static void probe_weld_initialize(void) {
    typedef void (*Construct)(void *, const void *);
    typedef void (*Initialize)(void *, const void *);
    Construct construct =
        (Construct)find_function("_ZN11b2WeldJointC2EPK14b2WeldJointDef");
    Initialize initialize = (Initialize)find_function(
        "_ZN11b2WeldJoint23InitVelocityConstraintsERK12b2SolverData");
#if defined(AB_ORACLE_WELD_POSITION)
    typedef int (*PositionSolve)(void *, const void *);
    PositionSolve solve_position = (PositionSolve)find_function(
        "_ZN11b2WeldJoint24SolvePositionConstraintsERK12b2SolverData");
    semihost(4, "AB_ANDROID_WELD_POSITION_V1 4096 8\n");
#elif defined(AB_ORACLE_WELD_VELOCITY)
    Initialize solve = (Initialize)find_function(
        "_ZN11b2WeldJoint24SolveVelocityConstraintsERK12b2SolverData");
    semihost(4, "AB_ANDROID_WELD_VELOCITY_V1 4096 8\n");
#else
    semihost(4, "AB_ANDROID_WELD_INITIALIZE_V1 4096\n");
#endif
    for (uint32_t sample = 0; sample < 4096; ++sample) {
#if defined(AB_ORACLE_WELD_POSITION)
        uint32_t input[32];
        weld_position_case(sample, input);
#else
        uint32_t input[28];
        weld_initialize_case(sample, input);
#endif
        uint32_t bodies[2][64] = {{0}}, before_bodies[2][64];
        for (size_t body = 0; body < 2; ++body) {
            size_t offset = body * 5;
            bodies[body][0] = 2;
            bodies[body][2] = (uint32_t)body;
            bodies[body][7] = input[offset];
            bodies[body][8] = input[offset + 1];
            bodies[body][14] = input[offset + 2];
            bodies[body][30] = input[offset + 3];
            bodies[body][32] = input[offset + 4];
            for (size_t field = 0; field < 64; ++field) {
                before_bodies[body][field] = bodies[body][field];
            }
        }
        uint32_t definition[] = {
            8,
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
            input[16],
        };
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
        for (size_t field = 0; field < 3; ++field) {
            joint[26 + field] = input[19 + field];
        }
        uint32_t before_joint[64];
        for (size_t field = 0; field < 64; ++field) {
            before_joint[field] = joint[field];
        }
#if defined(AB_ORACLE_WELD_POSITION)
        uint32_t positions[] = {input[28], input[29], input[2],
                                input[30], input[31], input[7]};
#else
        uint32_t positions[] = {0, 0, input[2], 0, 0, input[7]};
#endif
        uint32_t velocities[6];
        for (size_t field = 0; field < 6; ++field) {
            velocities[field] = input[22 + field];
        }
        uint32_t warm = sample & 1u;
        uint32_t solver[] = {input[17],
                             0,
                             input[18],
                             1,
                             1,
                             warm,
                             (uint32_t)(uintptr_t)positions,
                             (uint32_t)(uintptr_t)velocities};
        initialize(joint, solver);
        if (guarded[0] != UINT32_C(0x4f123456) || guarded[1] != UINT32_C(0x4f123456) ||
            guarded[66] != UINT32_C(0x4f123456) ||
            guarded[67] != UINT32_C(0x4f123456)) {
            fail("Original weld initialization exceeded output bounds\n");
        }
        for (size_t field = 0; field < 64; ++field) {
            if (field == 19 || (field >= 25 && field <= 51)) {
                continue;
            }
            if (joint[field] != before_joint[field]) {
                fail("Original weld initialization changed a preserved field\n");
            }
        }
        for (size_t body = 0; body < 2; ++body) {
            for (size_t field = 0; field < 64; ++field) {
                if (bodies[body][field] != before_bodies[body][field]) {
                    fail("Original weld initialization changed a body\n");
                }
            }
        }
        for (size_t field = 0; field < 12; ++field) {
            if (definition[field] != before_definition[field]) {
                fail("Original weld construction modified its definition\n");
            }
        }
        if (joint[29] != 0 || joint[30] != 1 || joint[35] != input[0] ||
            joint[36] != input[1] || joint[37] != input[5] || joint[38] != input[6] ||
            joint[39] != input[3] || joint[40] != input[8] || joint[41] != input[4] ||
            joint[42] != input[9]) {
            fail("Original weld body-cache values disagree\n");
        }
#if defined(AB_ORACLE_WELD_POSITION)
        uint32_t iterations = sample < 96 ? sample % 8 + 1 : sample % 9;
        uint32_t metadata[] = {sample, warm, iterations};
#elif defined(AB_ORACLE_WELD_VELOCITY)
        uint32_t iterations = sample % 9;
        uint32_t metadata[] = {sample, warm, iterations};
#else
        uint32_t metadata[] = {sample, warm};
#endif
        print_words(metadata, sizeof(metadata) / sizeof(*metadata));
        print_words(input, sizeof(input) / sizeof(*input));
        print_weld_state(joint, velocities);
#if defined(AB_ORACLE_WELD_VELOCITY)
        uint32_t initialized[64], before_solver[8];
        for (size_t field = 0; field < 64; ++field) {
            initialized[field] = joint[field];
        }
        for (size_t field = 0; field < 8; ++field) {
            before_solver[field] = solver[field];
        }
        for (uint32_t iteration = 0; iteration < iterations; ++iteration) {
            solve(joint, solver);
            print_weld_state(joint, velocities);
            for (size_t field = 0; field < 64; ++field) {
                if ((field < 26 || field > 28) && joint[field] != initialized[field]) {
                    fail("Original weld velocity pass changed cached fields\n");
                }
            }
            if (guarded[0] != UINT32_C(0x4f123456) ||
                guarded[1] != UINT32_C(0x4f123456) ||
                guarded[66] != UINT32_C(0x4f123456) ||
                guarded[67] != UINT32_C(0x4f123456)) {
                fail("Original weld velocity pass exceeded output bounds\n");
            }
        }
        for (size_t body = 0; body < 2; ++body) {
            for (size_t field = 0; field < 64; ++field) {
                if (bodies[body][field] != before_bodies[body][field]) {
                    fail("Original weld velocity pass changed a body\n");
                }
            }
        }
        for (size_t field = 0; field < 8; ++field) {
            if (solver[field] != before_solver[field]) {
                fail("Original weld velocity pass changed solver controls\n");
            }
        }
        if (positions[0] != 0 || positions[1] != 0 || positions[2] != input[2] ||
            positions[3] != 0 || positions[4] != 0 || positions[5] != input[7]) {
            fail("Original weld velocity pass changed positions\n");
        }
#elif defined(AB_ORACLE_WELD_POSITION)
        print_words(positions, 6);
        uint32_t initialized[64], before_velocities[6], before_solver[8];
        for (size_t field = 0; field < 64; ++field) {
            initialized[field] = joint[field];
        }
        for (size_t field = 0; field < 6; ++field) {
            before_velocities[field] = velocities[field];
        }
        for (size_t field = 0; field < 8; ++field) {
            before_solver[field] = solver[field];
        }
        for (uint32_t iteration = 0; iteration < iterations; ++iteration) {
            int result = solve_position(joint, solver);
            if (result < 0 || result > 1) {
                fail("Original weld position result is not boolean\n");
            }
            uint32_t output[7] = {(uint32_t)result};
            for (size_t field = 0; field < 6; ++field) {
                output[field + 1] = positions[field];
                if (velocities[field] != before_velocities[field]) {
                    fail("Original weld position pass changed velocities\n");
                }
            }
            print_words(output, 7);
            for (size_t field = 0; field < 64; ++field) {
                if (joint[field] != initialized[field]) {
                    fail("Original weld position pass changed joint state\n");
                }
            }
            if (guarded[0] != UINT32_C(0x4f123456) ||
                guarded[1] != UINT32_C(0x4f123456) ||
                guarded[66] != UINT32_C(0x4f123456) ||
                guarded[67] != UINT32_C(0x4f123456)) {
                fail("Original weld position pass exceeded output bounds\n");
            }
        }
        for (size_t body = 0; body < 2; ++body) {
            for (size_t field = 0; field < 64; ++field) {
                if (bodies[body][field] != before_bodies[body][field]) {
                    fail("Original weld position pass changed a body\n");
                }
            }
        }
        for (size_t field = 0; field < 8; ++field) {
            if (solver[field] != before_solver[field]) {
                fail("Original weld position pass changed solver controls\n");
            }
        }
#endif
    }
#if defined(AB_ORACLE_WELD_POSITION)
    semihost(4, "AB weld position reference complete\n");
#elif defined(AB_ORACLE_WELD_VELOCITY)
    semihost(4, "AB weld velocity reference complete\n");
#else
    semihost(4, "AB weld initialization reference complete\n");
#endif
}

#endif
