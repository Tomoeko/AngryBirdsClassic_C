#include "ab/physics_island.h"

#include "joint_island_cases.h"
#include "joint_native.h"

static bool edges(void) {
    AbBodyMotion bodies[2] = {
        {.mass = {.type = AB_BODY_DYNAMIC,
                  .inverse_mass = 1,
                  .inverse_inertia = 1,
                  .linear_velocity = {1, 0}},
         .gravity_scale = 1},
        {.mass = {.type = AB_BODY_DYNAMIC,
                  .inverse_mass = 1,
                  .inverse_inertia = 1,
                  .center = {1, 0},
                  .linear_velocity = {-1, 0}},
         .gravity_scale = 1},
    };
    AbJointConstraint constraint = {.kind = AB_JOINT_WELD};
    AbIslandJoint joint = {0, 1, &constraint};
    AbPositionState positions[2];
    AbVelocity velocities[2];
    AbIslandWorkspace workspace = {
        .positions = positions, .velocities = velocities, .body_capacity = 2};
    AbIslandStep step = {.motion = {.maximum_translation = 2,
                                    .maximum_translation_squared = 4,
                                    .position_iterations = 1},
                         .velocity_iterations = 1,
                         .time_ratio = 1};
    CHECK(ab_island_step_joints(bodies, 2, NULL, 0, &joint, 1, &step, &workspace) ==
              AB_POSITION_UNRESOLVED &&
          bodies[0].mass.center.x == 0.5f && bodies[1].mass.center.x == 0.5f &&
          bodies[0].mass.linear_velocity.x == 0 &&
          bodies[1].mass.linear_velocity.x == 0);
    step.warm_start = true;
    CHECK(ab_island_step_joints(bodies, 2, NULL, 0, &joint, 1, &step, &workspace) ==
          AB_POSITION_RESOLVED);
    AbBodyMotion original[2];
    memcpy(original, bodies, sizeof(original));
    AbJointConstraint before = constraint;
    CHECK(ab_island_step_joints(bodies, 2, NULL, 0, NULL, 1, &step, &workspace) ==
              AB_POSITION_INVALID &&
          ab_island_step_joints(bodies, 2, NULL, 0, &joint, 1, &step, NULL) ==
              AB_POSITION_INVALID);
    joint.second_body = 2;
    CHECK(ab_island_step_joints(bodies, 2, NULL, 0, &joint, 1, &step, &workspace) ==
          AB_POSITION_INVALID);
    joint.second_body = 0;
    CHECK(ab_island_step_joints(bodies, 2, NULL, 0, &joint, 1, &step, &workspace) ==
          AB_POSITION_INVALID);
    joint.second_body = 1;
    joint.constraint = NULL;
    CHECK(ab_island_step_joints(bodies, 2, NULL, 0, &joint, 1, &step, &workspace) ==
              AB_POSITION_INVALID &&
          memcmp(original, bodies, sizeof(original)) == 0 &&
          memcmp(&before, &constraint, sizeof(before)) == 0);
    return true;
}

static bool native_checks(FILE *file) {
    char line[100];
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB_ANDROID_JOINT_ISLAND_V1 4096\n") == 0);
    size_t total_bodies = 0, total_contacts = 0, total_joints = 0;
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        uint32_t controls[14], original[4][64];
        CHECK(read_words(file, controls, 14) && controls[0] == sample &&
              controls[1] == 2 + sample % 3 && controls[2] == (sample / 16) % 4 &&
              controls[3] == 1 + sample % 3 && controls[10] == (sample & 1u));
        AbBodyMotion bodies[4];
        for (size_t body = 0; body < controls[1]; ++body) {
            CHECK(read_words(file, original[body], 64));
            bodies[body] = read_body(original[body]);
        }
        AbIslandContact contacts[3];
        for (size_t index = 0; index < controls[2]; ++index) {
            uint32_t data[23];
            CHECK(read_words(file, data, 23));
            contacts[index] = (AbIslandContact){.first_body = data[0],
                                                .second_body = data[1],
                                                .first_radius = from_bits(data[2]),
                                                .second_radius = from_bits(data[3]),
                                                .friction = from_bits(data[4]),
                                                .restitution = from_bits(data[5]),
                                                .tangent_speed = from_bits(data[6])};
            _Static_assert(sizeof(AbManifold) == 16 * sizeof(uint32_t),
                           "Native manifold projection requires sixteen words");
            memcpy(&contacts[index].manifold, data + 7, sizeof(AbManifold));
        }
        AbJointConstraint constraints[3];
        AbIslandJoint joints[3];
        for (uint32_t index = 0; index < controls[3]; ++index) {
            uint32_t input[41], generated[41];
            CHECK(read_words(file, input, 41));
            joint_island_input(sample, index, controls[1], generated);
            CHECK(memcmp(input, generated, sizeof(input)) == 0 &&
                  prepare_joint(input, sample, index, constraints + index));
            joints[index] = (AbIslandJoint){input[1], input[2], constraints + index};
        }
        AbPositionState positions[4];
        AbVelocity velocities[4];
        AbVelocityContact velocity_contacts[3];
        AbPositionConstraint position_contacts[3];
        AbIslandWorkspace workspace = {positions,         velocities, velocity_contacts,
                                       position_contacts, 4,          3};
        AbIslandStep step = {
            .motion = {.seconds = from_bits(controls[4]),
                       .gravity = {from_bits(controls[5]), from_bits(controls[6])},
                       .maximum_translation = from_bits(controls[12]),
                       .maximum_translation_squared = from_bits(controls[13]),
                       .position_iterations = controls[8],
                       .allow_sleep = controls[7] != 0},
            .velocity_iterations = controls[9],
            .time_ratio = from_bits(controls[11]),
            .warm_start = controls[10] != 0};
        AbPositionResult result =
            ab_island_step_joints(bodies, controls[1], contacts, controls[2], joints,
                                  controls[3], &step, &workspace);
        if (result == AB_POSITION_INVALID) {
            fprintf(stderr, "Joint island %" PRIu32 " rejected\n", sample);
            return false;
        }
        for (size_t body = 0; body < controls[1]; ++body) {
            uint32_t expected[64];
            CHECK(read_words(file, expected, 64));
            write_body(bodies + body, original[body]);
            CHECK(compare_words(original[body], expected, 64, sample, "body", body));
        }
        for (size_t contact = 0; contact < controls[2]; ++contact) {
            uint32_t expected[16], actual[16];
            CHECK(read_words(file, expected, 16));
            memcpy(actual, &contacts[contact].manifold, sizeof(actual));
            CHECK(compare_words(actual, expected, 16, sample, "contact", contact));
        }
        for (size_t joint = 0; joint < controls[3]; ++joint) {
            uint32_t expected[37], actual[37];
            CHECK(read_words(file, expected, 37) &&
                  project_joint(constraints + joint, actual) &&
                  compare_words(actual, expected, 37, sample, "joint", joint));
        }
        total_bodies += controls[1];
        total_contacts += controls[2];
        total_joints += controls[3];
    }
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB joint island reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("Compared 4,096 native joint islands: %zu bodies, %zu contacts and "
           "%zu joint states (%zu output words)\n",
           total_bodies, total_contacts, total_joints,
           total_bodies * 64 + total_contacts * 16 + total_joints * 37);
    return true;
}

int main(int argc, char **argv) {
    if (!edges()) {
        return 1;
    }
    if (argc == 1) {
        return 0;
    }
    if (argc != 2) {
        fputs("Usage: ab_joint_island_tests [NATIVE_CORPUS]\n", stderr);
        return 2;
    }
    FILE *file = fopen(argv[1], "rb");
    bool okay = file && native_checks(file);
    if (file && fclose(file) != 0) {
        okay = false;
    }
    return okay ? 0 : 1;
}
