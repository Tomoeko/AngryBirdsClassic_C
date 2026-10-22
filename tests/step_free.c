#include "ab/physics_step.h"
#include "ab/physics_island.h"

#include "step_body_native.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static bool edge_checks(void) {
    AbFreeIslandStep step = {.seconds = 0.5f,
                             .gravity = {0, -10},
                             .maximum_translation = 100,
                             .maximum_translation_squared = 10000,
                             .position_iterations = 10};
    AbBodyMotion body = {.mass = {.type = AB_BODY_DYNAMIC,
                                  .inverse_mass = 0.5f,
                                  .inverse_inertia = 0.25f},
                         .force = {4, 0},
                         .torque = 2,
                         .gravity_scale = 1,
                         .awake = true,
                         .sleep_allowed = true};
    CHECK(ab_island_step_free(&body, 1, &step));
    CHECK(body.mass.linear_velocity.x == 1 && body.mass.linear_velocity.y == -5 &&
          body.mass.angular_velocity == 0.25f);
    CHECK(body.mass.center.x == 0.5f && body.mass.center.y == -2.5f &&
          body.mass.angle == 0.125f && body.force.x == 4 && body.torque == 2);
    body.mass.linear_velocity = (AbVec2){0, 0};
    body.mass.angular_velocity = 0;
    body.force = (AbVec2){0, 0};
    body.torque = 0;
    step.gravity = (AbVec2){0, 0};
    step.allow_sleep = true;
    step.position_iterations = 0;
    CHECK(ab_island_step_free(&body, 1, &step));
    CHECK(body.awake && body.sleep_time == 0.5f);
    step.position_iterations = 1;
    CHECK(ab_island_step_free(&body, 1, &step));
    CHECK(!body.awake && body.sleep_time == 0);
    CHECK(ab_island_step_free(NULL, 0, &step));
    CHECK(!ab_island_step_free(NULL, 1, &step));
    CHECK(!ab_island_step_free(&body, 1, NULL));
    step.seconds = -1;
    AbBodyMotion unchanged = body;
    CHECK(!ab_island_step_free(&body, 1, &step));
    CHECK(memcmp(&body, &unchanged, sizeof(body)) == 0);
    return true;
}

static bool contact_edge_checks(void) {
    AbBodyMotion bodies[2] = {
        {.mass = {.type = AB_BODY_DYNAMIC,
                  .inverse_mass = 1,
                  .linear_velocity = {1, 0}},
         .awake = true,
         .sleep_allowed = true},
        {.mass = {.type = AB_BODY_DYNAMIC, .inverse_mass = 1, .center = {1, 0}},
         .awake = true,
         .sleep_allowed = true},
    };
    AbBodyMotion original[2];
    memcpy(original, bodies, sizeof(original));
    AbIslandContact contact = {.manifold = {.point_count = 1},
                               .second_body = 1,
                               .first_radius = 0.5f,
                               .second_radius = 0.5f};
    AbIslandStep step = {.motion = {.maximum_translation = 2,
                                    .maximum_translation_squared = 4,
                                    .position_iterations = 1},
                         .velocity_iterations = 10,
                         .time_ratio = 1};
    AbPositionState positions[2];
    AbVelocity velocities[2];
    AbVelocityContact velocity_contact;
    AbPositionConstraint position_contact;
    AbIslandWorkspace workspace = {positions,         velocities, &velocity_contact,
                                   &position_contact, 2,          1};
    CHECK(ab_island_step(bodies, 2, NULL, 1, &step, &workspace) == AB_POSITION_INVALID);
    CHECK(ab_island_step(bodies, 2, &contact, 1, &step, NULL) == AB_POSITION_INVALID);
    workspace.body_capacity = 1;
    CHECK(ab_island_step(bodies, 2, &contact, 1, &step, &workspace) ==
          AB_POSITION_INVALID);
    workspace.body_capacity = 2;
    contact.second_body = 2;
    CHECK(ab_island_step(bodies, 2, &contact, 1, &step, &workspace) ==
          AB_POSITION_INVALID);
    contact.second_body = 1;
    contact.manifold.point_count = 0;
    CHECK(ab_island_step(bodies, 2, &contact, 1, &step, &workspace) ==
          AB_POSITION_INVALID);
    CHECK(memcmp(bodies, original, sizeof(original)) == 0);
    contact.manifold.point_count = 1;
    CHECK(ab_island_step(bodies, 2, &contact, 1, &step, &workspace) ==
          AB_POSITION_RESOLVED);
    CHECK(bodies[0].mass.linear_velocity.x == 0.5f &&
          bodies[1].mass.linear_velocity.x == 0.5f &&
          contact.manifold.points[0].normal_impulse == 0.5f);
    step.velocity_iterations = 0;
    step.warm_start = true;
    step.time_ratio = 0.5f;
    CHECK(ab_island_step(bodies, 2, &contact, 1, &step, &workspace) ==
          AB_POSITION_RESOLVED);
    CHECK(bodies[0].mass.linear_velocity.x == 0.25f &&
          bodies[1].mass.linear_velocity.x == 0.75f &&
          contact.manifold.points[0].normal_impulse == 0.25f);
    return true;
}

static bool toi_edge_checks(void) {
    AbBodyDefinition definition = {.type = AB_BODY_DYNAMIC,
                                   .active = true,
                                   .sleep_allowed = true,
                                   .linear_velocity = {0.1f, 0},
                                   .linear_damping = 100,
                                   .gravity_scale = 1};
    AbBodyMotion bodies[2];
    CHECK(ab_body_initialize(&definition, bodies));
    definition.position.x = 1;
    CHECK(ab_body_initialize(&definition, bodies + 1));
    bodies[0].force = (AbVec2){10, 20};
    bodies[0].torque = 30;
    bodies[0].sleep_time = 0.5f;
    bodies[0].mass.previous_center.x = -2;
    bodies[1].mass.previous_center.x = -3;
    AbPositionState positions[2];
    AbVelocity velocities[2];
    AbIslandWorkspace workspace = {
        .positions = positions, .velocities = velocities, .body_capacity = 2};
    AbIslandStep step = {.motion = {.seconds = 0.1f,
                                    .gravity = {0, -10},
                                    .maximum_translation = 2,
                                    .maximum_translation_squared = 4,
                                    .position_iterations = 1,
                                    .allow_sleep = true},
                         .velocity_iterations = 1};
    CHECK(ab_island_step_toi(bodies, 2, NULL, 0, &step, &workspace, 0, 0) ==
          AB_POSITION_RESOLVED);
    CHECK(bodies[0].mass.center.x > 0 && bodies[0].mass.center.x < 0.1f &&
          bodies[0].mass.linear_velocity.x == 0.1f && bodies[0].force.x == 10 &&
          bodies[0].torque == 30 && bodies[0].sleep_time == 0.5f && !bodies[0].awake);
    CHECK(bodies[0].mass.previous_center.x == 0 &&
          bodies[1].mass.previous_center.x == -3);
    AbBodyMotion original[2];
    memcpy(original, bodies, sizeof(bodies));
    CHECK(ab_island_step_toi(bodies, 2, NULL, 0, &step, &workspace, 2, 0) ==
          AB_POSITION_INVALID);
    CHECK(memcmp(original, bodies, sizeof(bodies)) == 0);
    CHECK(ab_island_step_toi(NULL, 0, NULL, 0, &step, &workspace, 0, 0) ==
          AB_POSITION_INVALID);
    return true;
}

#include "island_reports.h"
#include "world_solve.h"

static bool native_checks(FILE *file) {
    char header[128];
    CHECK(fgets(header, sizeof(header), file));
    if (strcmp(header, "WARNING: linker: Unsupported flags DT_FLAGS_1=0x8000000\n") ==
        0) {
        CHECK(fgets(header, sizeof(header), file));
    }
    if (strcmp(header, "AB_ANDROID_WORLD_SOLVE_V1 4096\n") == 0 ||
        strcmp(header, "AB_ANDROID_WORLD_REPORT_V1 4096\n") == 0) {
        return world_native_checks(
            file, strcmp(header, "AB_ANDROID_WORLD_REPORT_V1 4096\n") == 0);
    }
    bool with_toi = strcmp(header, "AB_ANDROID_TOI_ISLAND_V1 4096\n") == 0;
    bool with_reports =
        with_toi || strcmp(header, "AB_ANDROID_CONTACT_REPORT_V1 4096\n") == 0;
    bool with_contacts =
        with_reports || strcmp(header, "AB_ANDROID_CONTACT_ISLAND_V1 4096\n") == 0;
    CHECK(with_contacts || strcmp(header, "AB_ANDROID_FREE_ISLAND_V1 4096\n") == 0);
    size_t compared_bodies = 0, sleeping_bodies = 0;
    size_t compared_contacts = 0;
    size_t compared_reports = 0;
    for (size_t index = 0; index < 4096; ++index) {
        uint32_t controls[14] = {0}, original[4][64];
        CHECK(read_words(file, controls,
                         with_toi        ? 14
                         : with_contacts ? 12
                                         : 9) &&
              controls[0] <= 4 && controls[9] <= 4);
        AbFreeIslandStep step = {
            .seconds = from_bits(controls[1]),
            .gravity = {from_bits(controls[2]), from_bits(controls[3])},
            .maximum_translation = from_bits(controls[7]),
            .maximum_translation_squared = from_bits(controls[8]),
            .position_iterations = controls[5],
            .allow_sleep = controls[4] != 0,
        };
        AbBodyMotion bodies[4];
        for (size_t body = 0; body < controls[0]; ++body) {
            CHECK(read_words(file, original[body], 64));
            bodies[body] = read_body(original[body]);
        }
        AbIslandContact contacts[4];
        for (size_t index_contact = 0; index_contact < controls[9]; ++index_contact) {
            uint32_t data[23];
            CHECK(read_words(file, data, 23));
            AbIslandContact *contact = contacts + index_contact;
            *contact = (AbIslandContact){.first_body = data[0],
                                         .second_body = data[1],
                                         .first_radius = from_bits(data[2]),
                                         .second_radius = from_bits(data[3]),
                                         .friction = from_bits(data[4]),
                                         .restitution = from_bits(data[5]),
                                         .tangent_speed = from_bits(data[6])};
            _Static_assert(sizeof(contact->manifold) == 16 * sizeof(uint32_t),
                           "Native contact comparison requires 16 words");
            memcpy(&contact->manifold, data + 7, sizeof(contact->manifold));
        }
        AbPositionState positions[4];
        AbVelocity velocities[4];
        AbVelocityContact velocity_contacts[4];
        AbPositionConstraint position_contacts[4];
        AbIslandWorkspace workspace = {positions,         velocities, velocity_contacts,
                                       position_contacts, 4,          4};
        AbIslandStep island_step = {.motion = step,
                                    .velocity_iterations = controls[6],
                                    .time_ratio = from_bits(controls[11]),
                                    .warm_start = controls[10] != 0};
        IslandReports reports = {.bodies = bodies,
                                 .original = (const uint32_t(*)[64])original,
                                 .body_count = controls[0]};
        if (with_reports) {
            island_step.report = record_impulse;
            island_step.report_context = &reports;
        }
        if (with_toi) {
            CHECK(ab_island_step_toi(bodies, controls[0], contacts, controls[9],
                                     &island_step, &workspace, controls[12],
                                     controls[13]) != AB_POSITION_INVALID);
        } else if (with_contacts) {
            CHECK(ab_island_step(bodies, controls[0], contacts, controls[9],
                                 &island_step, &workspace) != AB_POSITION_INVALID);
        } else {
            CHECK(ab_island_step_free(bodies, controls[0], &step));
        }
        for (size_t body = 0; body < controls[0]; ++body) {
            uint32_t expected[64];
            CHECK(read_words(file, expected, 64));
            if (!(expected[1] & 2)) {
                ++sleeping_bodies;
            }
            write_body(bodies + body, original[body]);
            for (size_t field = 0; field < 64; ++field) {
                if (original[body][field] != expected[field]) {
                    fprintf(stderr,
                            "Free island %zu body %zu field %zu: "
                            "%08" PRIx32 " != %08" PRIx32 "\n",
                            index, body, field, original[body][field], expected[field]);
                    return false;
                }
            }
            ++compared_bodies;
        }
        for (size_t contact = 0; contact < controls[9]; ++contact) {
            uint32_t expected[16], actual[16];
            CHECK(read_words(file, expected, 16));
            memcpy(actual, &contacts[contact].manifold, sizeof(actual));
            for (size_t field = 0; field < 16; ++field) {
                if (actual[field] != expected[field]) {
                    fprintf(stderr,
                            "Island %zu contact %zu field %zu: "
                            "%08" PRIx32 " != %08" PRIx32 "\n",
                            index, contact, field, actual[field], expected[field]);
                    return false;
                }
            }
            ++compared_contacts;
        }
        if (with_reports) {
            CHECK(compare_reports(file, &reports, index, &compared_reports));
        }
    }
    CHECK(fgets(header, sizeof(header), file));
    CHECK(strcmp(header, with_toi        ? "AB TOI island reference complete\n"
                         : with_reports  ? "AB contact report reference complete\n"
                         : with_contacts ? "AB contact island reference complete\n"
                                         : "AB free island reference complete\n") == 0);
    CHECK(fgetc(file) == EOF && !ferror(file));
    printf("All %zu bodies and %zu contacts across 4096 islands match; "
           "%zu bodies sleep.\n",
           compared_bodies, compared_contacts, sleeping_bodies);
    if (with_reports) {
        printf("All %zu post-solve impulse/body snapshots match.\n", compared_reports);
    }
    return true;
}

int main(int argc, char **argv) {
    if (argc > 2) {
        fprintf(stderr, "Usage: ab_step_free_tests [NATIVE_ISLAND_OR_WORLD_TRACE]\n");
        return 2;
    }
    if (!edge_checks() || !contact_edge_checks() || !world_edge_checks() ||
        !toi_edge_checks()) {
        return 1;
    }
    if (argc == 1) {
        return 0;
    }
    FILE *file = fopen(argv[1], "rb");
    if (!file) {
        fprintf(stderr, "Cannot open native free-island trace\n");
        return 1;
    }
    bool okay = native_checks(file);
    if (fclose(file) != 0) {
        okay = false;
    }
    return okay ? 0 : 1;
}
