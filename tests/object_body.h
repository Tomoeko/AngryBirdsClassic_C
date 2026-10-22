#ifndef AB_TEST_OBJECT_BODY_H
#define AB_TEST_OBJECT_BODY_H

#include "ab/level_bodies.h"
#include "ab/object_body.h"
#include "ab/slingshot_body.h"
#include "ab/body_pose.h"
#include "ab/classic_level.h"

static bool body_pose_edge_checks(void) {
    AbBodyMotion body = {
        .mass = {.type = AB_BODY_DYNAMIC, .mass = 1, .transform = {{1, 2}, 1, 0}}};
    AbBodyPose pose = {{3, 4}, 5}, previous = pose;
    CHECK(ab_body_pose_capture(&body, 3, &pose));
    CHECK(memcmp(&pose, &previous, sizeof(pose)) == 0 && !body.bullet);
    body.awake = true;
    CHECK(ab_body_pose_capture(&body, 3, &pose));
    CHECK(body.bullet && pose.position.x == 1 && pose.position.y == 2 &&
          number(pose.angle).bits == UINT32_C(0x3fc90fdb));
    body.bullet = false;
    CHECK(ab_body_pose_capture(&body, 1, &pose) && !body.bullet);
    body.mass.mass = 0;
    CHECK(ab_body_pose_capture(&body, 3, &pose) && !body.bullet);
    body.mass.mass = NAN;
    previous = pose;
    CHECK(!ab_body_pose_capture(&body, 3, &pose));
    CHECK(memcmp(&pose, &previous, sizeof(pose)) == 0 && !body.bullet);
    AbBodyPose history[2] = {{{0, 4}, 0}, {{2, 8}, 0x1.921fb6p+1f}};
    CHECK(ab_body_pose_interpolate(history, 1, 0.5f, &pose));
    CHECK(pose.position.x == 1 && pose.position.y == 6 &&
          number(pose.angle).bits == UINT32_C(0x4096cbe4));
    CHECK(ab_body_pose_interpolate(history, 0, 0.5f, &pose));
    CHECK(number(pose.angle).bits == UINT32_C(0xbfc90fdb));
    previous = pose;
    CHECK(!ab_body_pose_interpolate(history, 1, NAN, &pose));
    CHECK(memcmp(&pose, &previous, sizeof(pose)) == 0);
    CHECK(!ab_body_pose_interpolate(history, 2, 0.5f, &pose));
    CHECK(!ab_body_pose_capture(NULL, 3, &pose));
    CHECK(!ab_body_pose_interpolate(NULL, 1, 0.5f, &pose));
    return true;
}

typedef struct {
    AbWorldSolveBody bodies[32];
    AbWorldFixture fixtures[32];
    AbFixtureMass mass[32];
    AbWorldSolveWorkspace solve;
} ObjectBodyStorage;

typedef struct {
    char name[128];
    char definition[128];
    char sprite_name[128];
    uint32_t sprite[2], body[30], shape[24], metadata[4], bounds[4];
    AbObjectBody instance;
    bool leading_bird;
} NativeObjectBody;

static bool open_object_world(ObjectBodyStorage *memory, AbPhysicsWorld *world,
                              AbError *error) {
    *memory = (ObjectBodyStorage){0};
    memory->solve.body_capacity = 32;
    AbWorldStorage storage = {.bodies = memory->bodies,
                              .fixtures = memory->fixtures,
                              .mass_scratch = memory->mass,
                              .mass_capacity = 32,
                              .solve = &memory->solve,
                              .body_capacity = 32,
                              .fixture_capacity = 32};
    AbWorldDefinition definition = {.broadphase_limits = {128, 1024, 1024},
                                    .maximum_translation = 2,
                                    .maximum_translation_squared = 4};
    return ab_world_open(world, definition, storage, error);
}

static const AbDataValue *data_field(const AbDataTable *source, const char *name) {
    return ab_data_get(source, string(name));
}

static bool body_edge_checks(AbPhysicsWorld *world) {
    ObjectBodyStorage memory;
    AbError error = {0};
    CHECK(open_object_world(&memory, world, &error));
    AbDataEntry fields[] = {
        {string("type"), string("box")},
        {string("width"), number(2)},
        {string("height"), number(4)},
        {string("density"), number(3)},
        {string("friction"), number(0.3f)},
        {string("restitution"), number(0)},
        {string("sensor"), {.kind = AB_DATA_BOOLEAN, .bits = 1}},
        {string("collision"), {.kind = AB_DATA_BOOLEAN}},
        {string("gravityEnabled"), {.kind = AB_DATA_BOOLEAN}},
        {string("controllable"), {.kind = AB_DATA_BOOLEAN, .bits = 1}},
        {string("radius"), number(1)},
        {string("angle"), number(2)},
    };
    AbDataTable source = {fields, 12, 12, NULL};
    AbDataEntry angle = {string("angle"), number(-1)};
    AbDataTable overrides = {&angle, 1, 1, NULL};
    AbObjectDefinition definition = {
        .source = &source, .overrides = &overrides, .collider = number(3)};
    AbObjectBody object;
    CHECK(ab_object_body_create(world, &definition, (AbVec2){1, 2}, (AbVec2){0, 0}, 0,
                                7, &object, &error));
    CHECK(object.body == 0 && object.fixture == 0 && !object.collisions_enabled &&
          !object.gravity_enabled && object.controllable);
    CHECK(memory.bodies[0].motion.mass.angle == 0x1.521fb6p+2f);
    CHECK(memory.bodies[0].motion.angular_damping == 1 &&
          memory.bodies[0].motion.gravity_scale == 1);
    CHECK(memory.bodies[0].motion.user_data == 0 && memory.fixtures[0].user_data == 7);
    CHECK(memory.fixtures[0].sensor && memory.bodies[0].motion.mass.mass == 24);
    CHECK(ab_object_body_queue(world, &object, &error));
    AbSlingTransferInput transfer = {
        .start = {1, 2}, .target = {3, 4}, .time = 0.25f, .height = 1, .rotation = -1};
    AbSlingTransferFrame frame;
    CHECK(ab_slingshot_transfer_body(world, &object, &transfer, &frame, &error));
    CHECK(memory.bodies[0].motion.mass.transform.position.x == 1.5f &&
          memory.bodies[0].motion.mass.transform.position.y == 2 &&
          memory.bodies[0].motion.mass.angle == 0x1.521fb6p+2f &&
          memory.bodies[0].motion.mass.type == AB_BODY_STATIC);
    transfer.time = 1;
    CHECK(ab_slingshot_transfer_body(world, &object, &transfer, &frame, &error));
    CHECK(frame.complete && memory.bodies[0].motion.mass.type == AB_BODY_DYNAMIC &&
          memory.bodies[0].motion.mass.mass == 24 &&
          memory.bodies[0].motion.mass.angle == 0 &&
          memory.bodies[0].motion.mass.transform.position.x == 2.9f &&
          memory.bodies[0].motion.mass.transform.position.y == 3.9f);
    transfer.time = NAN;
    CHECK(!ab_slingshot_transfer_body(world, &object, &transfer, &frame, &error));
    CHECK(memory.bodies[0].motion.mass.transform.position.x == 2.9f && !world->failed);
    CHECK(ab_body_velocity_set(&memory.bodies[0].motion, (AbVec2){3, 4}));
    CHECK(ab_body_angular_velocity_set(&memory.bodies[0].motion, 2));
    CHECK(ab_slingshot_hold_body(world, &object, (AbVec2){6, 7}, &error));
    CHECK(memory.bodies[0].motion.mass.transform.position.x == 6 &&
          memory.bodies[0].motion.mass.transform.position.y == 7 &&
          memory.bodies[0].motion.mass.angle == 0 &&
          memory.bodies[0].motion.mass.linear_velocity.x == 0 &&
          memory.bodies[0].motion.mass.linear_velocity.y == 0 &&
          memory.bodies[0].motion.mass.angular_velocity == 0);
    CHECK(!ab_slingshot_hold_body(world, &object, (AbVec2){NAN, 0}, &error));
    CHECK(memory.bodies[0].motion.mass.transform.position.x == 6 && !world->failed);
    CHECK(ab_object_body_queue(world, &object, &error));
    CHECK(memory.bodies[0].motion.mass.type == AB_BODY_STATIC &&
          memory.bodies[0].motion.mass.angle == 0 &&
          memory.bodies[0].motion.mass.mass == 0);
    CHECK(!ab_object_body_queue(world, NULL, &error));
    AbObjectBody previous = object;
    angle.value = number(NAN);
    CHECK(!ab_object_body_create(world, &definition, (AbVec2){1, 2}, (AbVec2){0, 0}, 0,
                                 7, &object, &error));
    CHECK(memcmp(&object, &previous, sizeof(object)) == 0 && world->body_count == 1);
    angle.value = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1};
    CHECK(!ab_object_body_create(world, &definition, (AbVec2){1, 2}, (AbVec2){0, 0}, 0,
                                 7, &object, &error));
    angle.value.bits = 0;
    fields[0].value = string("circle");
    CHECK(ab_object_body_create(world, &definition, (AbVec2){1, 2}, (AbVec2){0, 0}, 0,
                                8, &object, &error));
    CHECK(object.collisions_enabled && memory.bodies[1].motion.mass.angle == 0);
    CHECK(ab_ground_body_create(world, (AbVec2){3, 5}, 9, &object, &error));
    CHECK(object.collider.bits == 0 && object.collisions_enabled &&
          object.gravity_enabled && !object.controllable);
    CHECK(memory.fixtures[2].shape.data.polygon.vertices[2].x == 1000 &&
          memory.fixtures[2].shape.data.polygon.vertices[2].y == 5 &&
          memory.fixtures[2].friction == 0.8f && memory.fixtures[2].density == 0);
    CHECK(!ab_ground_body_create(world, (AbVec2){NAN, 5}, 9, &object, &error));
    CHECK(!ab_ground_body_create(world, (AbVec2){3, 5}, 9, NULL, &error));
    CHECK(!ab_object_body_create(world, &definition, (AbVec2){1, 2}, (AbVec2){0, 0}, 0,
                                 7, NULL, &error));
    world->locked = true;
    CHECK(!ab_object_body_queue(world, &object, &error));
    CHECK(!ab_ground_body_create(world, (AbVec2){3, 5}, 9, &object, &error));
    world->locked = false;
    ab_world_close(world);
    return true;
}

static bool angle_edge_checks(void) {
    const uint32_t inputs[] = {0,          0x80000000, 1,         0x80000001,
                               0x40c90fdb, 0xc0c90fdb, 0x7f7fffff};
    const uint32_t expected[] = {0, 0x80000000, 1,         0x40c90fdb,
                                 0, 0x80000000, 0x3fddb0f8};
    for (size_t index = 0; index < sizeof(inputs) / sizeof(*inputs); ++index) {
        float output = 9;
        CHECK(ab_game_angle_wrap(from_bits(inputs[index]), &output));
        CHECK(number(output).bits == expected[index]);
    }
    float output = 9;
    CHECK(!ab_game_angle_wrap(NAN, &output));
    CHECK(!ab_game_angle_wrap(INFINITY, &output));
    CHECK(!ab_game_angle_wrap(-INFINITY, &output));
    CHECK(!ab_game_angle_wrap(0, NULL) && output == 9);
    return true;
}

static bool level_body_edge_checks(AbPhysicsWorld *world) {
    ObjectBodyStorage memory;
    AbError error = {0};
    CHECK(open_object_world(&memory, world, &error));
    AbDataTable placements = {0}, catalog = {0};
    AbDataEntry fields[] = {{string("world"), table(&placements)},
                            {string("joints"), {0}},
                            {string("chains"), {0}}};
    AbDataTable globals = {fields, 3, 3, NULL};
    AbDataTableLayout layout = {.table = &placements, .no_nil_assignments = true};
    AbLuaData level = {.globals = &globals, .layouts = &layout};
    AbLevelObject objects[2];
    AbLevelBodies output = {objects, 2, 0};
    size_t indices[2];
    AbStringOrderNode nodes[2];
    AbStringOrderWorkspace order = {indices, 2, nodes, 2};
    AbLevelBodySource request = {.level = &level,
                                 .catalog = &catalog,
                                 .placement_scale = 1,
                                 .physics_scale = 0.05f,
                                 .ground_position = {3, 5}};
    CHECK(!ab_level_bodies_create(world, NULL, &output, &order, &error));
    request.placement_scale = 0;
    CHECK(!ab_level_bodies_create(world, &request, &output, &order, &error));
    request.placement_scale = 1;
    fields[0].value = (AbDataValue){0};
    CHECK(!ab_level_bodies_create(world, &request, &output, &order, &error));
    fields[0].value = table(&placements);
    fields[2].value = table(&globals);
    CHECK(!ab_level_bodies_create(world, &request, &output, &order, &error));
    fields[2].value = (AbDataValue){0};
    fields[2].value = (AbDataValue){.kind = AB_DATA_BOOLEAN};
    CHECK(!ab_level_bodies_create(world, &request, &output, &order, &error));
    fields[2].value = (AbDataValue){0};
    layout.no_nil_assignments = false;
    CHECK(!ab_level_bodies_create(world, &request, &output, &order, &error));
    layout.no_nil_assignments = true;
    output.capacity = 0;
    CHECK(!ab_level_bodies_create(world, &request, &output, &order, &error));
    output.capacity = 2;
    CHECK(world->body_count == 0 && world->fixture_count == 0 && !world->failed);
    fields[1].value = table(&globals);
    CHECK(ab_level_bodies_create(world, &request, &output, &order, &error));
    CHECK(output.count == 1 && objects[0].physics.collider.bits == 0 &&
          world->joint_count == 0);
    fields[1].value = (AbDataValue){0};
    CHECK(world->storage.fixtures[objects[0].physics.fixture].user_data ==
          (uintptr_t)objects);
    AbDataEntry runtime_entries[2];
    AbLevelObjectOrder runtime_order = {runtime_entries, 2, order, SIZE_MAX};
    CHECK(!ab_level_bodies_order(&output, NULL, &error));
    runtime_order.entry_capacity = 0;
    CHECK(!ab_level_bodies_order(&output, &runtime_order, &error));
    CHECK(runtime_order.hash_capacity == SIZE_MAX);
    runtime_order.entry_capacity = 2;
    CHECK(ab_level_bodies_order(&output, &runtime_order, &error));
    CHECK(runtime_order.hash_capacity == 1 && order.indices[0] == 0);
    objects[1] = objects[0];
    output.count = 2;
    CHECK(!ab_level_bodies_order(&output, &runtime_order, &error));
    output.count = 0;
    CHECK(ab_level_bodies_order(&output, &runtime_order, &error));
    CHECK(runtime_order.hash_capacity == 0);
    output.count = 1;
    CHECK(!ab_level_bodies_create(world, &request, &output, &order, &error));
    CHECK(ab_world_destroy_body(world, objects[0].physics.body, &error));
    AbDataEntry object_fields[] = {{string("name"), string("wrong")},
                                   {string("definition"), string("missing")},
                                   {string("angle"), number(0)}};
    AbDataTable object = {object_fields, 3, 3, NULL};
    AbDataEntry placement = {string("object"), table(&object)};
    placements = (AbDataTable){&placement, 1, 1, NULL};
    layout.hash_capacity = 1;
    CHECK(!ab_level_bodies_create(world, &request, &output, &order, &error));
    object_fields[0].value = string("object");
    CHECK(!ab_level_bodies_create(world, &request, &output, &order, &error));
    CHECK(output.count == 0 && !world->failed);
    ab_world_close(world);
    return true;
}

static bool native_angle_checks(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        perror(path);
        return false;
    }
    bool okay = true;
    char header[80];
    if (!fgets(header, sizeof(header), file) ||
        strcmp(header, "AB_ANDROID_GAME_ANGLE_V1 65536\n") != 0) {
        okay = false;
    }
    for (size_t index = 0; okay && index < 65536; ++index) {
        uint32_t input, expected;
        float actual;
        okay = fscanf(file, "%8" SCNx32 " %8" SCNx32, &input, &expected) == 2 &&
               fgetc(file) == '\n' && ab_game_angle_wrap(from_bits(input), &actual);
        if (okay && number(actual).bits != expected) {
            fprintf(stderr,
                    "Game angle %zu input %08" PRIx32 ": %08" PRIx32 " != %08" PRIx32
                    "\n",
                    index, input, number(actual).bits, expected);
            okay = false;
        }
    }
    okay = okay && fgets(header, sizeof(header), file) &&
           strcmp(header, "AB game angle reference complete\n") == 0 &&
           fgetc(file) == EOF && !ferror(file);
    if (fclose(file) != 0) {
        okay = false;
    }
    if (okay) {
        puts("All 65536 finite-angle wrappers match the measured Android imports.");
    }
    return okay;
}

static bool read_body_words(FILE *file, uint32_t *words, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%8" SCNx32, words + index) == 1);
    }
    CHECK(fgetc(file) == '\n');
    return true;
}

static bool read_body_records(FILE *file, NativeObjectBody records[23]) {
    char header[80];
    CHECK(fgets(header, sizeof(header), file));
    CHECK(strcmp(header, "AB_LEVEL_INITIAL_BODIES_V1 23\n") == 0);
    for (size_t index = 0; index < 23; ++index) {
        NativeObjectBody *record = records + index;
        CHECK(fscanf(file, "%127s\n%127s\n%127s\n", record->name, record->definition,
                     record->sprite_name) == 3);
        CHECK(read_body_words(file, record->sprite, 2));
        CHECK(read_body_words(file, record->body, 30));
        CHECK(read_body_words(file, record->shape, 24));
        CHECK(read_body_words(file, record->metadata, 4));
        CHECK(read_body_words(file, record->bounds, 4));
    }
    CHECK(fgetc(file) == EOF && !ferror(file));
    return true;
}

static bool body_position(const AbDataTable *placement, AbVec2 *position) {
    const AbDataValue *x = data_field(placement, "x");
    const AbDataValue *y = data_field(placement, "y");
    CHECK(x && y && x->kind == AB_DATA_NUMBER && y->kind == AB_DATA_NUMBER);
    *position = (AbVec2){from_bits(x->bits), from_bits(y->bits)};
    return true;
}

static bool native_sprite_size(void *context, AbSpan name, AbVec2 *size,
                               AbError *error) {
    const NativeObjectBody *records = context;
    for (size_t index = 0; index < 23; ++index) {
        if (name.size == strlen(records[index].sprite_name) &&
            memcmp(name.data, records[index].sprite_name, name.size) == 0) {
            *size = (AbVec2){from_bits(records[index].sprite[0]),
                             from_bits(records[index].sprite[1])};
            return true;
        }
    }
    return ab_fail(error, "Missing native reference sprite size");
}

static bool level_body_source(const AbDataTable *catalog, const AbLuaData *level,
                              NativeObjectBody records[23], AbLevelBodySource *output) {
    const AbDataValue *source = data_field(level->globals, "world");
    CHECK(source && source->kind == AB_DATA_TABLE && source->table->count == 23);
    const AbDataValue *ground = data_field(source->table, "ground");
    CHECK(ground && ground->kind == AB_DATA_TABLE);
    AbVec2 ground_position;
    CHECK(body_position(ground->table, &ground_position));
    *output = (AbLevelBodySource){.level = level,
                                  .catalog = catalog,
                                  .sprite_context = records,
                                  .sprite_size = native_sprite_size,
                                  .placement_scale = 1,
                                  .physics_scale = 0.05f,
                                  .ground_position = ground_position};
    return true;
}

static bool bind_level_body_records(const AbPhysicsWorld *world,
                                    const AbLevelBodies *bodies,
                                    NativeObjectBody records[23]) {
    CHECK(bodies && bodies->count == 23);
    for (size_t index = 0; index < 23; ++index) {
        const AbLevelObject *object = bodies->objects + index;
        NativeObjectBody *record = records + index;
        CHECK(object->name.size == strlen(record->name) &&
              memcmp(object->name.data, record->name, object->name.size) == 0);
        CHECK(object->definition_name.size == strlen(record->definition) &&
              memcmp(object->definition_name.data, record->definition,
                     object->definition_name.size) == 0);
        CHECK(world->storage.fixtures[object->physics.fixture].user_data ==
              (uintptr_t)object);
        record->instance = object->physics;
        if (object->physics.controllable) {
            const AbDataValue *start = data_field(object->placement, "startNumber");
            record->leading_bird =
                start && start->kind == AB_DATA_NUMBER && start->bits == number(1).bits;
        }
    }
    return true;
}

static bool open_native_classic_level(const AbDataTable *catalog, const AbLuaData *data,
                                      NativeObjectBody records[23],
                                      AbWorldDefinition definition,
                                      AbClassicLevel **output, AbError *error) {
    AbLevelBodySource source;
    CHECK(level_body_source(catalog, data, records, &source));
    CHECK(ab_classic_level_open(&source, definition,
                                (AbClassicLevelLimits){.objects = 32,
                                                       .contacts = 128,
                                                       .storage_bytes = 1024 * 1024},
                                true, output, error));
    AbClassicLevelView view = ab_classic_level_view(*output);
    CHECK(view.order_hash_capacity == 32);
    return bind_level_body_records(view.world, view.bodies, records);
}

static void write_object_body(const AbWorldSolveBody *body, uint32_t output[30]) {
    const AbBodyMotion *motion = &body->motion;
    const AbBodyMassState *mass = &motion->mass;
    output[0] = (uint32_t)mass->type;
    output[1] = (body->island ? 1u : 0u) | (motion->awake ? 2u : 0u) |
                (motion->sleep_allowed ? 4u : 0u) | (motion->bullet ? 8u : 0u) |
                (mass->fixed_rotation ? 16u : 0u) | (motion->active ? 32u : 0u);
    const float fields[] = {
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
        mass->sweep_alpha,
        mass->linear_velocity.x,
        mass->linear_velocity.y,
        mass->angular_velocity,
        motion->force.x,
        motion->force.y,
        motion->torque,
        mass->mass,
        mass->inverse_mass,
        mass->inertia,
        mass->inverse_inertia,
        motion->linear_damping,
        motion->angular_damping,
        motion->gravity_scale,
        motion->sleep_time,
    };
    for (size_t index = 0; index < 27; ++index) {
        output[index + 2] = number(fields[index]).bits;
    }
    output[29] = (uint32_t)motion->user_data;
}

static void write_object_shape(const AbWorldFixture *fixture, uint32_t output[24]) {
    memset(output, 0, 24 * sizeof(*output));
    output[0] = (uint32_t)fixture->shape.kind;
    output[1] = number(fixture->density).bits;
    output[2] = number(fixture->friction).bits;
    output[3] = number(fixture->restitution).bits;
    if (fixture->shape.kind == AB_SHAPE_CIRCLE) {
        AbCircle circle = fixture->shape.data.circle;
        output[4] = number(circle.radius).bits;
        output[5] = number(circle.center.x).bits;
        output[6] = number(circle.center.y).bits;
    } else {
        const AbPolygon *polygon = &fixture->shape.data.polygon;
        output[4] = number(polygon->radius).bits;
        output[5] = (uint32_t)polygon->count;
        output[6] = number(polygon->centroid.x).bits;
        output[7] = number(polygon->centroid.y).bits;
        for (size_t vertex = 0; vertex < 4; ++vertex) {
            output[8 + vertex * 2] = number(polygon->vertices[vertex].x).bits;
            output[9 + vertex * 2] = number(polygon->vertices[vertex].y).bits;
            output[16 + vertex * 2] = number(polygon->normals[vertex].x).bits;
            output[17 + vertex * 2] = number(polygon->normals[vertex].y).bits;
        }
    }
}

static bool compare_body_fields(const NativeObjectBody *record, const uint32_t *actual,
                                const uint32_t *expected, size_t count,
                                const char *section) {
    for (size_t field = 0; field < count; ++field) {
        /* The leading bird has already begun its separate sling animation. */
        if (record->leading_bird && strcmp(section, "body") == 0 &&
            ((field >= 2 && field <= 5) || (field >= 8 && field <= 13))) {
            continue;
        }
        if (actual[field] != expected[field]) {
            fprintf(stderr,
                    "Initial object %s %s field %zu: %08" PRIx32 " != %08" PRIx32 "\n",
                    record->name, section, field, actual[field], expected[field]);
            return false;
        }
    }
    return true;
}

static bool compare_initial_bodies(const AbPhysicsWorld *world,
                                   const NativeObjectBody records[23]) {
    size_t leading = 0;
    for (size_t index = 0; index < 23; ++index) {
        const NativeObjectBody *record = records + index;
        const AbObjectBody *instance = &record->instance;
        const AbWorldFixture *fixture = world->storage.fixtures + instance->fixture;
        uint32_t actual[30];
        write_object_body(world->storage.bodies + instance->body, actual);
        CHECK(compare_body_fields(record, actual, record->body, 30, "body"));
        write_object_shape(fixture, actual);
        CHECK(compare_body_fields(record, actual, record->shape, 24, "shape"));
        const uint32_t metadata[] = {instance->collider.bits,
                                     instance->collisions_enabled,
                                     instance->gravity_enabled, fixture->sensor};
        CHECK(compare_body_fields(record, metadata, record->metadata, 4, "metadata"));
        if (record->leading_bird) {
            ++leading;
        } else {
            AbAabb bounds = fixture->proxy.bounds;
            const uint32_t coordinates[] = {
                number(bounds.lower.x).bits,
                number(bounds.lower.y).bits,
                number(bounds.upper.x).bits,
                number(bounds.upper.y).bits,
            };
            CHECK(
                compare_body_fields(record, coordinates, record->bounds, 4, "bounds"));
        }
        CHECK(world->body_order[22 - index] == instance->body);
    }
    CHECK(leading == 1);
    puts("All 23 initial fixture geometries/materials and object physics metadata "
         "match.");
    puts("All defined body fields and tight bounds match for 22 objects; leading-bird "
         "sling pose remains excluded. Allocator bytes and pointer fields excluded.");
    return true;
}

static bool initial_body_checks(const char *blocks_path, const char *level_path,
                                const char *reference_path) {
    AbBuffer inputs[2] = {0};
    AbLuaChunk chunks[2] = {0};
    AbLuaData data[2] = {0};
    AbClassicLevel *owned_level = NULL;
    NativeObjectBody records[23] = {0};
    const char *paths[] = {blocks_path, level_path};
    AbError error = {0};
    FILE *file = NULL;
    bool okay = false;
    for (size_t index = 0; index < 2; ++index) {
        if (!ab_file_read(paths[index], 8 * 1024 * 1024, inputs + index, &error) ||
            !ab_lua_read((AbSpan){inputs[index].data, inputs[index].size},
                         8 * 1024 * 1024, chunks + index, &error) ||
            !ab_lua_data(chunks + index, 8 * 1024 * 1024, data + index, &error)) {
            goto release_data;
        }
    }
    file = fopen(reference_path, "rb");
    if (!file) {
        perror(reference_path);
        goto release_data;
    }
    AbWorldDefinition definition = {.broadphase_limits = {128, 1024, 1024},
                                    .maximum_translation = 2,
                                    .maximum_translation_squared = 4};
    okay = read_body_records(file, records) &&
           open_native_classic_level(data[0].globals, data + 1, records, definition,
                                     &owned_level, &error) &&
           compare_initial_bodies(ab_classic_level_view(owned_level).world, records);
release_data:
    if (!okay && error.message[0]) {
        fprintf(stderr, "Initial bodies: %s\n", error.message);
    }
    if (file && fclose(file) != 0) {
        okay = false;
    }
    ab_classic_level_close(owned_level);
    for (size_t index = 0; index < 2; ++index) {
        ab_lua_data_close(data + index);
        ab_lua_close(chunks + index);
        ab_buffer_free(inputs + index);
    }
    return okay;
}

#endif
