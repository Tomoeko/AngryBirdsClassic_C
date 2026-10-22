#ifndef AB_TEST_WORLD_SOLVE_H
#define AB_TEST_WORLD_SOLVE_H

#include "ab/physics_world_solve.h"

#include "world_solve_workspace.h"

typedef struct {
    AbWorldSolveBody *bodies;
    AbWorldSolveContact *contacts;
    size_t count;
    bool invalid;
} WorldCallbackCheck;

static void change_world_during_report(void *context, size_t contact,
                                       const AbContactImpulse *impulse) {
    WorldCallbackCheck *check = context;
    if (contact != check->count || contact >= 2 || impulse->count != 1 ||
        !check->bodies[contact].island || !check->bodies[2].island) {
        check->invalid = true;
        return;
    }
    ++check->count;
    check->bodies[contact].motion.force.x = 7;
    check->contacts[contact].state.friction = 9;
    check->contacts[contact].state.manifold.points[0].feature_id = 0x520;
}

static bool world_edge_checks(void) {
    WorldTestStorage storage;
    AbWorldSolveWorkspace workspace = world_workspace(&storage);
    AbWorldSolveBody bodies[3] = {0};
    AbWorldSolveContact contacts[2] = {0};
    AbBodyDefinition definition = {.type = AB_BODY_DYNAMIC,
                                   .awake = true,
                                   .active = true,
                                   .sleep_allowed = true,
                                   .gravity_scale = 1};
    CHECK(ab_body_initialize(&definition, &bodies[0].motion));
    CHECK(ab_body_initialize(&definition, &bodies[1].motion));
    definition.type = AB_BODY_STATIC;
    definition.awake = false;
    CHECK(ab_body_initialize(&definition, &bodies[2].motion));
    for (size_t contact = 0; contact < 2; ++contact) {
        contacts[contact] = (AbWorldSolveContact){
            .state = {.flags = AB_CONTACT_ENABLED | AB_CONTACT_TOUCHING,
                      .manifold = {.kind = AB_CONTACT_CIRCLES, .point_count = 1}},
            .first_body = contact,
            .second_body = 2,
        };
    }
    AbIslandStep step = {.motion = {.seconds = 0.1f,
                                    .maximum_translation = 2,
                                    .maximum_translation_squared = 4,
                                    .position_iterations = 1,
                                    .allow_sleep = false},
                         .velocity_iterations = 1,
                         .time_ratio = 1};
    CHECK(ab_world_solve_discrete(bodies, 3, contacts, 2, &step, &workspace));
    CHECK(workspace.island_count == 2 && !bodies[2].island && bodies[0].island &&
          bodies[1].island && bodies[2].motion.awake);
    CHECK(bodies[0].island_index == 0 && bodies[1].island_index == 0 &&
          bodies[2].island_index == 1 && (contacts[0].state.flags & 1) &&
          (contacts[1].state.flags & 1));
    WorldCallbackCheck callback = {bodies, contacts, 0, false};
    step.report = change_world_during_report;
    step.report_context = &callback;
    CHECK(ab_world_solve_discrete(bodies, 3, contacts, 2, &step, &workspace));
    CHECK(callback.count == 2 && !callback.invalid);
    for (size_t body = 0; body < 2; ++body) {
        CHECK(bodies[body].motion.force.x == 7 && contacts[body].state.friction == 9 &&
              contacts[body].state.manifold.points[0].feature_id == 0x520);
    }
    AbWorldSolveBody unchanged[3];
    memcpy(unchanged, bodies, sizeof(bodies));
    contacts[0].second_body = 3;
    CHECK(!ab_world_solve_discrete(bodies, 3, contacts, 2, &step, &workspace));
    CHECK(memcmp(unchanged, bodies, sizeof(bodies)) == 0);
    contacts[0].second_body = 2;
    workspace.contact_capacity = 1;
    CHECK(!ab_world_solve_discrete(bodies, 3, contacts, 2, &step, &workspace));
    CHECK(memcmp(unchanged, bodies, sizeof(bodies)) == 0);
    workspace.contact_capacity = 8;
    bodies[0].motion.active = false;
    bodies[1].motion.awake = false;
    CHECK(ab_world_solve_discrete(bodies, 3, contacts, 2, &step, &workspace));
    CHECK(workspace.island_count == 0 && !bodies[0].island && !bodies[1].island);
    CHECK(ab_world_solve_discrete(NULL, 0, NULL, 0, &step, &workspace));
    return true;
}

static bool compare_world_case(FILE *file, size_t sample, size_t *islands,
                               size_t *body_states, size_t *manifolds,
                               bool with_reports, size_t *report_count) {
    uint32_t controls[12], original[7][64];
    CHECK(read_words(file, controls, 12) && controls[0] <= 7 && controls[9] <= 8);
    AbWorldSolveBody bodies[7];
    AbWorldSolveContact contacts[8];
    for (size_t body = 0; body < controls[0]; ++body) {
        CHECK(read_words(file, original[body], 64));
        bodies[body] = (AbWorldSolveBody){read_body(original[body]), original[body][2],
                                          (original[body][1] & 1) != 0};
    }
    for (size_t index = 0; index < controls[9]; ++index) {
        uint32_t metadata[2], data[23];
        CHECK(read_words(file, metadata, 2) && read_words(file, data, 23));
        contacts[index] = (AbWorldSolveContact){
            .state = {.flags = metadata[0],
                      .friction = from_bits(data[4]),
                      .restitution = from_bits(data[5]),
                      .tangent_speed = from_bits(data[6])},
            .first_body = data[0],
            .second_body = data[1],
            .first_radius = from_bits(data[2]),
            .second_radius = from_bits(data[3]),
            .sensor = metadata[1] != 0,
        };
        memcpy(&contacts[index].state.manifold, data + 7, sizeof(AbManifold));
    }
    AbIslandStep step = {
        .motion = {.seconds = from_bits(controls[1]),
                   .gravity = {from_bits(controls[2]), from_bits(controls[3])},
                   .maximum_translation = from_bits(controls[7]),
                   .maximum_translation_squared = from_bits(controls[8]),
                   .position_iterations = controls[5],
                   .allow_sleep = controls[4] != 0},
        .velocity_iterations = controls[6],
        .time_ratio = from_bits(controls[11]),
        .warm_start = controls[10] != 0,
    };
    WorldTestStorage storage;
    AbWorldSolveWorkspace workspace = world_workspace(&storage);
    IslandReports reports = {.world_bodies = bodies,
                             .original = (const uint32_t(*)[64])original,
                             .body_count = controls[0]};
    if (with_reports) {
        step.report = record_impulse;
        step.report_context = &reports;
    }
    CHECK(ab_world_solve_discrete(bodies, controls[0], contacts, controls[9], &step,
                                  &workspace));
    for (size_t body = 0; body < controls[0]; ++body) {
        uint32_t expected[64];
        CHECK(read_words(file, expected, 64));
        write_body(&bodies[body].motion, original[body]);
        original[body][1] =
            (original[body][1] & ~UINT32_C(1)) | (bodies[body].island ? 1u : 0u);
        original[body][2] = (uint32_t)bodies[body].island_index;
        for (size_t field = 0; field < 64; ++field) {
            if (original[body][field] != expected[field]) {
                fprintf(stderr,
                        "World solve %zu body %zu field %zu: %08" PRIx32
                        " != %08" PRIx32 "\n",
                        sample, body, field, original[body][field], expected[field]);
                return false;
            }
        }
    }
    for (size_t index = 0; index < controls[9]; ++index) {
        uint32_t flags, expected[16], actual[16];
        CHECK(read_words(file, &flags, 1) && read_words(file, expected, 16));
        CHECK(contacts[index].state.flags == flags);
        memcpy(actual, &contacts[index].state.manifold, sizeof(actual));
        for (size_t field = 0; field < 16; ++field) {
            if (actual[field] != expected[field]) {
                fprintf(stderr,
                        "World solve %zu contact %zu field %zu: %08" PRIx32
                        " != %08" PRIx32 "\n",
                        sample, index, field, actual[field], expected[field]);
                return false;
            }
        }
    }
    if (with_reports) {
        CHECK(compare_reports(file, &reports, sample, report_count));
    }
    *islands += workspace.island_count;
    *body_states += controls[0];
    *manifolds += controls[9];
    return true;
}

static bool world_native_checks(FILE *file, bool with_reports) {
    size_t islands = 0, body_states = 0, manifolds = 0, reports = 0;
    for (size_t sample = 0; sample < 4096; ++sample) {
        CHECK(compare_world_case(file, sample, &islands, &body_states, &manifolds,
                                 with_reports, &reports));
    }
    char footer[128];
    CHECK(fgets(footer, sizeof(footer), file));
    CHECK(strcmp(footer, with_reports ? "AB world report reference complete\n"
                                      : "AB world solve reference complete\n") == 0);
    CHECK(fgetc(file) == EOF && !ferror(file));
    printf("All 4096 world graphs match: %zu islands, %zu bodies, %zu contacts.\n",
           islands, body_states, manifolds);
    if (with_reports) {
        printf("All %zu world post-solve impulse/body snapshots match.\n", reports);
    }
    return true;
}

#endif
