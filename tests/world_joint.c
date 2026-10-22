#include "ab/physics_world_solve.h"

#include "joint_island_cases.h"
#include "joint_native.h"
#include "joint_state_projection.h"
#include "world_solve_workspace.h"

static bool edges(void) {
    AbWorldSolveBody bodies[5] = {0};
    AbBodyDefinition definition = {
        .type = AB_BODY_DYNAMIC, .active = true, .awake = true, .gravity_scale = 1};
    for (size_t index = 0; index < 5; ++index) {
        CHECK(ab_body_initialize(&definition, &bodies[index].motion));
    }
    bodies[2].motion.mass.type = AB_BODY_STATIC;
    bodies[2].motion.mass.inverse_mass = 0;
    bodies[2].motion.awake = false;
    bodies[3].motion.awake = false;
    bodies[3].motion.sleep_time = 0.25f;
    bodies[4].motion.awake = false;
    bodies[4].motion.active = false;
    AbWorldSolveJoint joints[5] = {
        {.first_body = 0, .second_body = 2, .constraint.kind = AB_JOINT_DISTANCE},
        {.first_body = 1, .second_body = 2, .constraint.kind = AB_JOINT_WELD},
        {.first_body = 0, .second_body = 3, .constraint.kind = AB_JOINT_REVOLUTE},
        {.first_body = 1, .second_body = 4, .constraint.kind = AB_JOINT_PRISMATIC},
        {.first_body = 4, .second_body = 3, .constraint.kind = AB_JOINT_WELD},
    };
    for (size_t index = 0; index < 5; ++index) {
        joints[index].island = true;
    }
    WorldTestStorage storage;
    AbWorldSolveWorkspace workspace = world_workspace(&storage);
    AbIslandStep step = {
        .motion = {.maximum_translation = 2, .maximum_translation_squared = 4},
        .time_ratio = 1};
    CHECK(ab_world_solve_discrete_joints(bodies, 5, NULL, 0, joints, 5, &step,
                                         &workspace));
    CHECK(workspace.island_count == 2 && !bodies[2].island &&
          bodies[0].island_index == 0 && bodies[3].island_index == 1 &&
          bodies[1].island_index == 0 && bodies[2].island_index == 1 &&
          bodies[3].motion.awake && bodies[3].motion.sleep_time == 0 &&
          !bodies[4].motion.awake && !bodies[4].island && joints[0].island &&
          joints[1].island && joints[2].island && !joints[3].island &&
          !joints[4].island);
    AbWorldSolveBody before_bodies[5];
    AbWorldSolveJoint before_joints[5];
    memcpy(before_bodies, bodies, sizeof(bodies));
    memcpy(before_joints, joints, sizeof(joints));
    CHECK(!ab_world_solve_discrete_joints(bodies, 5, NULL, 0, NULL, 5, &step,
                                          &workspace));
    workspace.joint_capacity = 4;
    CHECK(!ab_world_solve_discrete_joints(bodies, 5, NULL, 0, joints, 5, &step,
                                          &workspace));
    workspace.joint_capacity = 6;
    workspace.joint_heads = NULL;
    CHECK(!ab_world_solve_discrete_joints(bodies, 5, NULL, 0, joints, 5, &step,
                                          &workspace));
    workspace = world_workspace(&storage);
    workspace.solver.velocities = NULL;
    CHECK(!ab_world_solve_discrete_joints(bodies, 5, NULL, 0, joints, 5, &step,
                                          &workspace));
    workspace = world_workspace(&storage);
    joints[0].second_body = 5;
    CHECK(!ab_world_solve_discrete_joints(bodies, 5, NULL, 0, joints, 5, &step,
                                          &workspace));
    joints[0].second_body = 0;
    CHECK(!ab_world_solve_discrete_joints(bodies, 5, NULL, 0, joints, 5, &step,
                                          &workspace));
    joints[0].second_body = 2;
    joints[0].constraint.kind = (AbJointKind)4;
    CHECK(!ab_world_solve_discrete_joints(bodies, 5, NULL, 0, joints, 5, &step,
                                          &workspace));
    joints[0].constraint.kind = AB_JOINT_DISTANCE;
    CHECK(memcmp(before_bodies, bodies, sizeof(bodies)) == 0 &&
          memcmp(before_joints, joints, sizeof(joints)) == 0 &&
          ab_world_solve_discrete_joints(NULL, 0, NULL, 0, NULL, 0, &step, &workspace));
    return true;
}

static bool compare_case(FILE *file, uint32_t sample, size_t totals[3]) {
    uint32_t controls[14], original[7][64];
    CHECK(read_words(file, controls, 14) && controls[0] == sample &&
          controls[1] == 2 + sample % 6 && controls[2] == (sample / 6) % 9 &&
          controls[3] == (sample / 11) % 7);
    AbWorldSolveBody bodies[7];
    for (size_t body = 0; body < controls[1]; ++body) {
        CHECK(read_words(file, original[body], 64));
        bodies[body] = (AbWorldSolveBody){read_body(original[body]), original[body][2],
                                          (original[body][1] & 1u) != 0};
    }
    AbWorldSolveContact contacts[8];
    for (size_t index = 0; index < controls[2]; ++index) {
        uint32_t metadata[2], data[23];
        CHECK(read_words(file, metadata, 2) && read_words(file, data, 23));
        contacts[index] =
            (AbWorldSolveContact){.state = {.flags = metadata[0],
                                            .friction = from_bits(data[4]),
                                            .restitution = from_bits(data[5]),
                                            .tangent_speed = from_bits(data[6])},
                                  .first_body = data[0],
                                  .second_body = data[1],
                                  .first_radius = from_bits(data[2]),
                                  .second_radius = from_bits(data[3]),
                                  .sensor = metadata[1] != 0};
        memcpy(&contacts[index].state.manifold, data + 7, sizeof(AbManifold));
    }
    AbWorldSolveJoint joints[6] = {0};
    for (uint32_t index = 0; index < controls[3]; ++index) {
        uint32_t flag, input[41], generated[41];
        CHECK(read_words(file, &flag, 1) && read_words(file, input, 41));
        joint_island_input(sample, index, controls[1], generated);
        CHECK(memcmp(input, generated, sizeof(input)) == 0 &&
              prepare_joint(input, sample, index, &joints[index].constraint));
        joints[index].first_body = input[1];
        joints[index].second_body = input[2];
        joints[index].island = flag != 0;
    }
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
    WorldTestStorage storage;
    AbWorldSolveWorkspace workspace = world_workspace(&storage);
    CHECK(ab_world_solve_discrete_joints(bodies, controls[1], contacts, controls[2],
                                         joints, controls[3], &step, &workspace));
    for (size_t body = 0; body < controls[1]; ++body) {
        uint32_t expected[64];
        CHECK(read_words(file, expected, 64));
        write_body(&bodies[body].motion, original[body]);
        original[body][1] =
            (original[body][1] & ~UINT32_C(1)) | (bodies[body].island ? 1u : 0u);
        original[body][2] = (uint32_t)bodies[body].island_index;
        CHECK(compare_words(original[body], expected, 64, sample, "world body", body));
    }
    for (size_t index = 0; index < controls[2]; ++index) {
        uint32_t flags, actual[16], expected[16];
        CHECK(read_words(file, &flags, 1) && read_words(file, expected, 16) &&
              flags == contacts[index].state.flags);
        memcpy(actual, &contacts[index].state.manifold, sizeof(actual));
        CHECK(compare_words(actual, expected, 16, sample, "world contact", index));
    }
    for (size_t index = 0; index < controls[3]; ++index) {
        uint32_t flag, actual[37], expected[37];
        CHECK(read_words(file, &flag, 1) && read_words(file, expected, 37) &&
              flag == (joints[index].island ? 1u : 0u) &&
              project_joint(&joints[index].constraint, actual));
        if (!flag) {
            joint_state_mask_uninitialized((uint32_t)joints[index].constraint.kind,
                                           actual);
        }
        CHECK(compare_words(actual, expected, 37, sample, "world joint", index));
    }
    for (size_t field = 0; field < 3; ++field) {
        totals[field] += controls[1 + field];
    }
    return true;
}

static bool native_checks(FILE *file) {
    char line[100];
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB_ANDROID_WORLD_JOINT_V1 4096\n") == 0);
    size_t totals[3] = {0};
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        CHECK(compare_case(file, sample, totals));
    }
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB world joint reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("Compared 4,096 native world graphs: %zu bodies, %zu contacts and "
           "%zu joint memberships/states (%zu output words)\n",
           totals[0], totals[1], totals[2],
           totals[0] * 64 + totals[1] * 17 + totals[2] * 38);
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
        fputs("Usage: ab_world_joint_tests [NATIVE_CORPUS]\n", stderr);
        return 2;
    }
    FILE *file = fopen(argv[1], "rb");
    bool okay = file && native_checks(file);
    if (file && fclose(file) != 0) {
        okay = false;
    }
    return okay ? 0 : 1;
}
