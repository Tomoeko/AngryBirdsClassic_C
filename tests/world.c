#include "ab/physics_world.h"

#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Failed at line %d: %s\n", __LINE__, #condition);          \
            return false;                                                              \
        }                                                                              \
    } while (0)

#include "world_storage.h"

static float from_bits(uint32_t bits) {
    float number;
    memcpy(&number, &bits, sizeof(number));
    return number;
}

static uint32_t to_bits(float number) {
    uint32_t bits;
    memcpy(&bits, &number, sizeof(bits));
    return bits;
}

static bool reject_pair(void *context, const AbPhysicsWorld *world, size_t first,
                        size_t second) {
    (void)world;
    (void)first;
    (void)second;
    ++*(size_t *)context;
    return false;
}

static bool edge_checks(AbPhysicsWorld *world) {
    WorldStorage memory;
    AbWorldStorage storage = prepare_storage(&memory, 1, 1);
    AbBodyDefinition body = {
        .type = AB_BODY_DYNAMIC, .awake = true, .active = true, .gravity_scale = 1};
    CHECK(ab_body_initialize(&body, &memory.bodies[0].motion));
    memory.fixtures[0] = (AbWorldFixture){
        .shape = {.kind = AB_SHAPE_CIRCLE, .data.circle = {1, {0, 0}}},
        .density = 1,
        .filter = {1, 0xffff, 0},
    };
    AbError error = {0};
    CHECK(ab_world_open(world, default_definition(), storage, &error));
    AbBodyMotion initial = memory.bodies[0].motion;
    CHECK(!ab_world_set_transform(world, 0, (AbVec2){NAN, 0}, 0, &error));
    CHECK(!ab_world_set_transform(world, 0, (AbVec2){0, 0}, NAN, &error));
    CHECK(!ab_world_set_body_type(world, 0, (AbBodyType)3, &error));
    CHECK(!ab_world_set_active(world, 1, true, &error));
    CHECK(memcmp(&initial, &memory.bodies[0].motion, sizeof(initial)) == 0);
    CHECK(!world->failed);
    world->locked = true;
    CHECK(!ab_world_set_transform(world, 0, (AbVec2){1, 2}, 0, &error));
    CHECK(!ab_world_set_active(world, 0, false, &error));
    CHECK(!ab_world_set_body_type(world, 0, AB_BODY_STATIC, &error));
    CHECK(!ab_world_set_fixed_rotation(world, 0, true, &error));
    CHECK(ab_world_set_fixture_filter(world, 0, (AbCollisionFilter){1, 0xffff, 0},
                                      &error));
    CHECK(ab_world_set_fixture_sensor(world, 0, true));
    CHECK(ab_world_set_fixture_sensor(world, 0, false));
    world->locked = false;
    CHECK(!ab_world_set_fixture_sensor(world, 1, false));
    CHECK(!ab_world_set_fixture_filter(world, 1, (AbCollisionFilter){0}, &error));
    CHECK(ab_world_set_active(world, 0, false, &error));
    CHECK(world->broadphase.proxy_count == 0 && !memory.fixtures[0].proxy.active);
    CHECK(ab_world_set_active(world, 0, false, &error));
    CHECK(ab_world_set_active(world, 0, true, &error));
    CHECK(world->broadphase.proxy_count == 1 && memory.fixtures[0].proxy.active);
    CHECK(ab_world_set_body_type(world, 0, AB_BODY_STATIC, &error));
    CHECK(memory.bodies[0].motion.mass.inverse_mass == 0);
    CHECK(ab_world_set_body_type(world, 0, AB_BODY_DYNAMIC, &error));
    CHECK(memory.bodies[0].motion.mass.inverse_mass > 0);
    CHECK(ab_world_set_fixed_rotation(world, 0, true, &error));
    CHECK(memory.bodies[0].motion.mass.inverse_inertia == 0);
    CHECK(ab_world_set_fixed_rotation(world, 0, false, &error));
    CHECK(memory.bodies[0].motion.mass.inverse_inertia > 0);
    CHECK(ab_world_apply_force(world, 0, (AbVec2){2, 3}, 4));
    CHECK(!ab_world_apply_force(world, 1, (AbVec2){0, 0}, 0));
    CHECK(!ab_world_apply_force(world, 0, (AbVec2){NAN, 0}, 0));
    memory.bodies[0].motion.force.x = FLT_MAX;
    AbBodyMotion unchanged = memory.bodies[0].motion;
    CHECK(!ab_world_apply_force(world, 0, (AbVec2){FLT_MAX, 0}, 0));
    CHECK(memcmp(&unchanged, &memory.bodies[0].motion, sizeof(unchanged)) == 0);
    CHECK(ab_world_step(world, 0, 10, 10, &error));
    CHECK(memory.bodies[0].motion.force.x == 0 && memory.bodies[0].motion.torque == 0 &&
          !world->new_fixtures);
    CHECK(!ab_world_step(world, -1, 10, 10, &error));
    CHECK(!world->failed && !world->locked);
    world->locked = true;
    CHECK(!ab_world_step(world, 0, 10, 10, &error));
    world->locked = false;
    CHECK(ab_world_step(world, 0.1f, 10, 10, &error));
    ab_world_close(world);
    memory.fixtures[0].shape.kind = AB_SHAPE_POLYGON;
    memory.fixtures[0].shape.data.polygon = (AbPolygon){.count = 9};
    memory.bodies[0].motion.active = false;
    CHECK(!ab_world_open(world, default_definition(), storage, &error));
    CHECK(world->failed);
    ab_world_close(world);
    storage = prepare_storage(&memory, 2, 2);
    CHECK(ab_body_initialize(&body, &memory.bodies[0].motion));
    CHECK(ab_body_initialize(&body, &memory.bodies[1].motion));
    for (size_t fixture = 0; fixture < 2; ++fixture) {
        memory.fixtures[fixture] = (AbWorldFixture){
            .body = fixture,
            .shape = {.kind = AB_SHAPE_CIRCLE, .data.circle = {1, {0, 0}}},
            .density = 1,
            .filter = {1, 0xffff, 0},
        };
    }
    CHECK(ab_world_open(world, default_definition(), storage, &error));
    size_t rejected = 0;
    world->listener = (AbWorldListener){.context = &rejected, .filter = reject_pair};
    CHECK(ab_world_step(world, 0, 10, 10, &error));
    CHECK(rejected == 1 && world->contact_count == 0);
    ab_world_close(world);
    storage.contact_capacity = 0;
    CHECK(ab_world_open(world, default_definition(), storage, &error));
    CHECK(!ab_world_step(world, 0, 10, 10, &error));
    CHECK(world->failed && !world->locked && world->contact_count == 0);
    CHECK(!ab_world_step(world, 0, 10, 10, &error));
    ab_world_close(world);
    return true;
}

static bool read_words(FILE *file, uint32_t *words, size_t count) {
    for (size_t field = 0; field < count; ++field) {
        CHECK(fscanf(file, "%8" SCNx32, words + field) == 1);
    }
    CHECK(fgetc(file) == '\n');
    return true;
}

static bool compare_words(const uint32_t *actual, const uint32_t *expected,
                          size_t count, size_t scene, size_t frame, const char *section,
                          size_t item) {
    for (size_t field = 0; field < count; ++field) {
        if (actual[field] != expected[field]) {
            fprintf(stderr,
                    "World scene %zu frame %zu %s %zu field %zu: %08" PRIx32
                    " != %08" PRIx32 "\n",
                    scene, frame, section, item, field, actual[field], expected[field]);
            return false;
        }
    }
    return true;
}

static uint32_t contact_key(const AbPhysicsWorld *world, size_t index) {
    AbFixturePair pair = world->storage.pairs[index];
    return (uint32_t)(world->storage.fixtures[pair.first].user_data << 16) |
           (uint32_t)world->storage.fixtures[pair.second].user_data;
}

#include "world_events.h"
#include "world_operations.h"

static void write_body(const AbPhysicsWorld *world, size_t index, uint32_t output[38]) {
    const AbWorldSolveBody *body = world->storage.bodies + index;
    const AbBodyMotion *motion = &body->motion;
    const AbBodyMassState *mass = &motion->mass;
    memset(output, 0, 38 * sizeof(uint32_t));
    output[0] = (uint32_t)mass->type;
    output[1] = (body->island ? 1u : 0u) | (motion->awake ? 2u : 0u) |
                (motion->sleep_allowed ? 4u : 0u) | (motion->bullet ? 8u : 0u) |
                (mass->fixed_rotation ? 16u : 0u) | (motion->active ? 32u : 0u);
    output[2] = (uint32_t)body->island_index;
    output[15] = to_bits(mass->sweep_alpha);
    const float transform[] = {
        mass->transform.position.x,
        mass->transform.position.y,
        mass->transform.sine,
        mass->transform.cosine,
        mass->local_center.x,
        mass->local_center.y,
        mass->previous_center.x,
        mass->previous_center.y,
        mass->center.x,
        mass->center.y,
        mass->previous_angle,
        mass->angle,
    };
    for (size_t field = 0; field < 12; ++field) {
        output[3 + field] = to_bits(transform[field]);
    }
    const float velocity[] = {mass->linear_velocity.x, mass->linear_velocity.y,
                              mass->angular_velocity,  motion->force.x,
                              motion->force.y,         motion->torque};
    for (size_t field = 0; field < 6; ++field) {
        output[16 + field] = to_bits(velocity[field]);
    }
    output[22] = 0xdead0000;
    for (size_t cursor = 0; cursor < world->body_count; ++cursor) {
        if (world->body_order[cursor] == index) {
            output[23] =
                cursor ? (uint32_t)world->storage.bodies[world->body_order[cursor - 1]]
                             .motion.user_data
                       : 0;
            output[24] =
                cursor + 1 < world->body_count
                    ? (uint32_t)world->storage.bodies[world->body_order[cursor + 1]]
                          .motion.user_data
                    : 0;
            break;
        }
    }
    for (size_t cursor = 0; cursor < world->fixture_count; ++cursor) {
        size_t fixture = world->fixture_order[cursor];
        if (world->storage.fixtures[fixture].body == index) {
            output[25] = (uint32_t)world->storage.fixtures[fixture].user_data;
            ++output[26];
        }
    }
    for (size_t contact = 0; contact < world->contact_count; ++contact) {
        const AbWorldSolveContact *current = world->storage.contacts + contact;
        if (current->first_body == index || current->second_body == index) {
            output[28] = contact_key(world, contact);
            break;
        }
    }
    for (size_t cursor = 0; cursor < world->joint_count; ++cursor) {
        const AbWorldSolveJoint *joint =
            world->storage.joints + world->joint_order[cursor];
        if (joint->first_body == index || joint->second_body == index) {
            output[27] = (uint32_t)joint->user_data;
            break;
        }
    }
    const float properties[] = {mass->mass,
                                mass->inverse_mass,
                                mass->inertia,
                                mass->inverse_inertia,
                                motion->linear_damping,
                                motion->angular_damping,
                                motion->gravity_scale,
                                motion->sleep_time};
    for (size_t field = 0; field < 8; ++field) {
        output[29 + field] = to_bits(properties[field]);
    }
    output[37] = (uint32_t)motion->user_data;
}

static bool compare_snapshot(FILE *file, const AbPhysicsWorld *world, size_t scene,
                             size_t frame, bool with_fixtures, bool lifecycle) {
    uint32_t expected[38], actual[38];
    CHECK(read_words(file, expected, 6));
    const uint32_t summary[] = {
        (world->new_fixtures ? 1u : 0u) | (world->locked ? 2u : 0u) |
            (world->definition.clear_forces ? 4u : 0u),
        to_bits(world->inverse_previous_step),
        (uint32_t)world->broadphase.tree.root,
        (uint32_t)world->broadphase.proxy_count,
        (uint32_t)world->broadphase.move_count,
        (uint32_t)world->contact_count,
    };
    CHECK(compare_words(summary, expected, 6, scene, frame, "summary", 0));
    if (lifecycle) {
        const uint32_t counts[] = {(uint32_t)world->body_count,
                                   (uint32_t)world->fixture_count};
        CHECK(read_words(file, expected, 2));
        CHECK(compare_words(counts, expected, 2, scene, frame, "counts", 0));
    }
    for (size_t logical = 0; logical < world->body_count; ++logical) {
        size_t cursor = lifecycle ? logical : world->body_count - 1 - logical;
        size_t index = world->body_order[cursor];
        CHECK(read_words(file, expected, 38));
        expected[1] &= 0xffff; /* Upper halfword is native structure padding. */
        write_body(world, index, actual);
        CHECK(compare_words(actual, expected, 38, scene, frame, "body", logical));
    }
    for (size_t contact = 0; contact < world->contact_count; ++contact) {
        size_t words = world->definition.continuous ? 24 : 22;
        CHECK(read_words(file, expected, words));
        memset(actual, 0, words * sizeof(uint32_t));
        uint32_t key = contact_key(world, contact);
        const AbContactState *state = &world->storage.contacts[contact].state;
        actual[0] = key >> 16;
        actual[1] = key & 0xffff;
        actual[2] = state->flags;
        uint32_t manifold[16];
        memcpy(manifold, &state->manifold, sizeof(manifold));
        if (state->manifold.point_count) {
            memcpy(actual + 3, manifold,
                   state->manifold.point_count * 5 * sizeof(uint32_t));
            memcpy(actual + 13, manifold + 10, 6 * sizeof(uint32_t));
        }
        actual[19] = to_bits(state->friction);
        actual[20] = to_bits(state->restitution);
        actual[21] = to_bits(state->tangent_speed);
        actual[22] = world->storage.contacts[contact].impact_count;
        actual[23] = state->flags & 32
                         ? to_bits(world->storage.contacts[contact].impact_fraction)
                         : 0;
        CHECK(compare_words(actual, expected, words, scene, frame, "contact", contact));
    }
    return !with_fixtures || compare_fixtures(file, world, scene, frame);
}

static AbBodyDefinition read_definition(const uint32_t words[13]) {
    return (AbBodyDefinition){
        .type = (AbBodyType)words[0],
        .position = {from_bits(words[1]), from_bits(words[2])},
        .angle = from_bits(words[3]),
        .linear_velocity = {from_bits(words[4]), from_bits(words[5])},
        .angular_velocity = from_bits(words[6]),
        .linear_damping = from_bits(words[7]),
        .angular_damping = from_bits(words[8]),
        .sleep_allowed = (words[9] & 0xff) != 0,
        .awake = (words[9] & 0xff00) != 0,
        .fixed_rotation = (words[9] & 0xff0000) != 0,
        .bullet = (words[9] & 0xff000000) != 0,
        .active = (words[10] & 0xff) != 0,
        .user_data = words[11],
        .gravity_scale = from_bits(words[12]),
    };
}

static bool read_scene(FILE *file, WorldStorage *memory, AbPhysicsWorld *world) {
    uint32_t controls[7];
    CHECK(read_words(file, controls, 7) && controls[0] <= 5 && controls[1] <= 6);
    AbWorldStorage storage = prepare_storage(memory, controls[0], controls[1]);
    for (size_t body = 0; body < controls[0]; ++body) {
        uint32_t words[13];
        CHECK(read_words(file, words, 13));
        AbBodyDefinition definition = read_definition(words);
        CHECK(ab_body_initialize(&definition,
                                 &memory->bodies[controls[0] - 1 - body].motion));
    }
    for (size_t fixture = 0; fixture < controls[1]; ++fixture) {
        uint32_t data[13];
        CHECK(read_words(file, data, 13) && data[0] > 0 && data[0] <= controls[0]);
        uint16_t group_bits = (uint16_t)data[11];
        int16_t group;
        memcpy(&group, &group_bits, sizeof(group));
        AbWorldFixture *output = memory->fixtures + fixture;
        *output =
            (AbWorldFixture){.body = controls[0] - data[0],
                             .density = from_bits(data[7]),
                             .friction = from_bits(data[5]),
                             .restitution = from_bits(data[6]),
                             .sensor = data[8] != 0,
                             .filter = {(uint16_t)data[9], (uint16_t)data[10], group},
                             .user_data = data[12]};
        output->shape.kind = (AbShapeKind)data[1];
        if (data[1] == AB_SHAPE_CIRCLE) {
            output->shape.data.circle = (AbCircle){
                from_bits(data[2]), {from_bits(data[3]), from_bits(data[4])}};
        } else {
            CHECK(data[1] == AB_SHAPE_POLYGON);
            output->shape.data.polygon = ab_polygon_box(
                from_bits(data[3]), from_bits(data[4]), from_bits(data[2]));
        }
    }
    AbWorldDefinition definition = default_definition();
    definition.gravity = (AbVec2){from_bits(controls[5]), from_bits(controls[6])};
    definition.allow_sleep = controls[2] != 0;
    definition.warm_start = controls[3] != 0;
    definition.clear_forces = controls[4] != 0;
    AbError error = {0};
    if (!ab_world_open(world, definition, storage, &error)) {
        fprintf(stderr, "World open: %s\n", error.message);
        return false;
    }
    return true;
}

#include "world_lifecycle.h"
#include "world_clear_forces.h"
#include "world_joint_lifecycle.h"
#include "world_joint_native.h"

static bool native_checks(FILE *file, AbPhysicsWorld *world) {
    char header[128];
    CHECK(fgets(header, sizeof(header), file));
    if (strcmp(header, "AB_ANDROID_WORLD_JOINT_LIFECYCLE_V1 16 256\n") == 0) {
        return native_joint_lifecycle_checks(file, world);
    }
    if (strcmp(header, "AB_ANDROID_WORLD_LIFECYCLE_V1 16 256\n") == 0) {
        return native_lifecycle_checks(file, world);
    }
    bool reject_impacts = strstr(header, "REJECTED_IMPACTS") != NULL;
    bool operations = strstr(header, "WORLD_OPERATIONS") != NULL;
    bool continuous =
        operations || reject_impacts || strstr(header, "WORLD_CONTINUOUS") != NULL;
    bool with_events =
        operations || reject_impacts || strstr(header, "EVENTS_V1") != NULL;
    const char *expected_header =
        continuous ? (with_events ? "AB_ANDROID_WORLD_CONTINUOUS_EVENTS_V1 16 256\n"
                                  : "AB_ANDROID_WORLD_CONTINUOUS_V1 16 256\n")
                   : (with_events ? "AB_ANDROID_WORLD_EVENTS_V1 16 256\n"
                                  : "AB_ANDROID_WORLD_STEPS_V1 16 256\n");
    if (reject_impacts) {
        expected_header = "AB_ANDROID_WORLD_REJECTED_IMPACTS_V1 16 256\n";
    }
    if (operations) {
        expected_header = "AB_ANDROID_WORLD_OPERATIONS_V1 16 256\n";
    }
    CHECK(strcmp(header, expected_header) == 0);
    size_t event_counts[7] = {0};
    size_t rejected = 0;
    size_t operation_counts[7] = {0};
    for (size_t scene = 0; scene < 16; ++scene) {
        WorldStorage memory;
        CHECK(read_scene(file, &memory, world));
        world->definition.continuous = continuous;
        WorldEvents events = {.reject_impacts = reject_impacts};
        if (with_events) {
            world->listener = (AbWorldListener){.context = &events,
                                                .begin = capture_begin,
                                                .end = capture_end,
                                                .pre_solve = capture_pre_solve,
                                                .post_solve = capture_post_solve};
        }
        CHECK(compare_snapshot(file, world, scene, SIZE_MAX, operations, false));
        if (with_events) {
            CHECK(compare_events(file, &events, scene, SIZE_MAX, event_counts));
        }
        for (size_t frame = 0; frame < 256; ++frame) {
            if (operations) {
                events.count = 0;
                CHECK(read_world_operation(file, world, operation_counts));
                CHECK(compare_snapshot(file, world, scene, frame, true, false));
                CHECK(compare_events(file, &events, scene, frame, event_counts));
            }
            uint32_t controls[7];
            CHECK(read_words(file, controls, 7));
            events.count = 0;
            if (controls[3]) {
                CHECK(controls[3] <= world->storage.body_count);
                CHECK(ab_world_apply_force(
                    world, world->storage.body_count - controls[3],
                    (AbVec2){from_bits(controls[4]), from_bits(controls[5])},
                    from_bits(controls[6])));
            }
            AbError error = {0};
            if (!ab_world_step(world, from_bits(controls[0]), controls[1], controls[2],
                               &error)) {
                fprintf(stderr, "World scene %zu frame %zu: %s\n", scene, frame,
                        error.message);
                return false;
            }
            CHECK(compare_snapshot(file, world, scene, frame, operations, false));
            if (with_events) {
                CHECK(compare_events(file, &events, scene, frame, event_counts));
            }
        }
        rejected += events.rejected;
        ab_world_close(world);
    }
    CHECK(fgets(header, sizeof(header), file));
    const char *completion =
        continuous ? (with_events ? "AB world continuous events reference complete\n"
                                  : "AB world continuous reference complete\n")
                   : (with_events ? "AB world events reference complete\n"
                                  : "AB world steps reference complete\n");
    if (reject_impacts) {
        completion = "AB world rejected impacts reference complete\n";
    }
    if (operations) {
        completion = "AB world operations reference complete\n";
    }
    CHECK(strcmp(header, completion) == 0);
    CHECK(fgetc(file) == EOF && !ferror(file));
    puts("All 4096 world steps and initial attachment states match.");
    if (with_events) {
        printf(
            "World events match: %zu begin, %zu end, %zu pre-solve, %zu post-solve.\n",
            event_counts[1], event_counts[2], event_counts[3], event_counts[4]);
    }
    if (reject_impacts) {
        CHECK(rejected > 0);
        printf("Rejected %zu cached-impact callbacks.\n", rejected);
    }
    if (operations) {
        printf("World operations match: %zu transforms, %zu activations, %zu types, "
               "%zu rotations, %zu filters, %zu sensors.\n",
               operation_counts[1], operation_counts[2], operation_counts[3],
               operation_counts[4], operation_counts[5], operation_counts[6]);
    }
    return true;
}

int main(int argc, char **argv) {
    bool clear = argc == 3 && strcmp(argv[1], "--clear-forces") == 0;
    if (argc > 3 || (argc == 3 && !clear)) {
        fprintf(stderr,
                "Usage: ab_world_tests [--clear-forces] [NATIVE_WORLD_TRACE]\n");
        return 2;
    }
    AbPhysicsWorld world = {0};
    bool okay = edge_checks(&world) && lifecycle_edge_checks(&world) &&
                clear_force_edge_checks(&world) && joint_lifecycle_edges(&world);
    if (okay && argc >= 2) {
        const char *path = argv[clear ? 2 : 1];
        FILE *file = fopen(path, "rb");
        if (!file) {
            perror(path);
            okay = false;
        } else {
            okay = clear ? clear_force_native_checks(file, &world)
                         : native_checks(file, &world);
            if (fclose(file) != 0) {
                okay = false;
            }
        }
    }
    ab_world_close(&world);
    return okay ? 0 : 1;
}
