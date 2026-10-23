#include "ab/classic_joint_updates.h"
#include "world_storage.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Joint updates failed at %d: %s\n", __LINE__, #condition); \
            return false;                                                              \
        }                                                                              \
    } while (0)

static AbDataValue text(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)value, strlen(value)}};
}

static AbDataValue field(const AbDataTable *table, const char *name) {
    const AbDataValue *value = ab_data_get(table, text(name));
    return value ? *value : (AbDataValue){0};
}

typedef struct {
    AbClassicJoints *joints;
    AbClassicJointUpdates *updates;
    const AbDataTable *saved;
    const AbDataTable *saved_event;
    size_t notifications, prints, checks;
    bool reject;
} Fixture;

static bool printed(void *context, AbSpan name, AbError *error) {
    (void)name;
    (void)error;
    ++((Fixture *)context)->prints;
    return true;
}

static bool notified(void *context, AbDataTable *event, AbError *error) {
    Fixture *fixture = context;
    CHECK(fixture->prints == fixture->notifications + 1);
    if (fixture->reject)
        return ab_fail(error, "Selected joint listener rejection");
    AbDataValue joint = field(event, "joint");
    if (fixture->notifications == 1) {
        CHECK(joint.kind == AB_DATA_NIL && fixture->joints->joint_count == 1);
    } else {
        CHECK(joint.kind == AB_DATA_TABLE);
        AbDataValue name = field(joint.table, "name");
        CHECK(name.kind == AB_DATA_STRING &&
              ab_classic_joints_metadata(fixture->joints, name.string));
        CHECK(field(joint.table, "end1").kind == AB_DATA_STRING &&
              field(joint.table, "end2").kind == AB_DATA_STRING);
        if (!fixture->notifications) {
            fixture->saved = joint.table;
            fixture->saved_event = event;
            CHECK(fixture->joints->joint_count == 2 &&
                  ab_classic_joint_updates_defer(fixture->updates, text("motor").string,
                                                 error));
        }
    }
    ++fixture->notifications;
    return true;
}

static bool service(void *context, AbGameTailCall *call, AbError *error) {
    Fixture *fixture = context;
    if (call->operation == AB_GAME_TAIL_CHECK_JOINT_LIMIT) {
        ++fixture->checks;
        CHECK(ab_classic_joint_updates_call(fixture->updates, call, error));
        const AbClassicJointRecord *record =
            ab_classic_joints_metadata(fixture->joints, call->joint_name);
        CHECK(record && fixture->joints->world->storage.joints[record->world_joint]
                                .constraint.definition.prismatic.motor_speed == 2);
        return true;
    }
    if (call->operation == AB_GAME_TAIL_DESTROY_JOINT)
        return ab_classic_joint_updates_call(fixture->updates, call, error);
    if (call->operation == AB_GAME_TAIL_PHYSICS_ENABLED)
        call->result = false;
    return true;
}

static bool checks(bool reject) {
    WorldStorage memory;
    AbWorldStorage storage = prepare_storage(&memory, 2, 0);
    const AbBodyDefinition body = {.type = AB_BODY_DYNAMIC, .active = true};
    CHECK(ab_body_initialize(&body, &memory.bodies[0].motion) &&
          ab_body_initialize(&body, &memory.bodies[1].motion));
    AbPhysicsWorld world;
    CHECK(ab_world_open(&world, default_definition(), storage, NULL));
    AbClassicJointRecord records[4], glue[1];
    AbClassicJoints joints;
    CHECK(ab_classic_joints_open(&joints, &world,
                                 (AbClassicJointStorage){records, 4, glue, 1}, NULL));
    AbClassicJoint joint = {
        .type = AB_CLASSIC_JOINT_WELD,
        .name = text("brace").string,
        .endpoints = {text("first").string, text("second").string},
        .joint = {.first_body = 0, .second_body = 1, .constraint.kind = AB_JOINT_WELD}};
    size_t slot;
    CHECK(ab_classic_joints_create(&joints, &joint, &slot, NULL));
    joint.name = text("motor").string;
    joint.type = AB_CLASSIC_JOINT_PRISMATIC;
    joint.back_and_forth = true;
    joint.world_axis = (AbVec2){1, 0};
    joint.joint.constraint =
        (AbJointConstraint){.kind = AB_JOINT_PRISMATIC,
                            .definition.prismatic = {.local.axis = {1, 0},
                                                     .enable_motor = true,
                                                     .enable_limit = true,
                                                     .upper_translation = 5,
                                                     .motor_speed = -2,
                                                     .maximum_motor_force = 10}};
    CHECK(ab_classic_joints_create(&joints, &joint, &slot, NULL));
    AbDataTable empty = {0};
    AbLuaData data = {.globals = &empty};
    AbEventManager *events = NULL;
    CHECK(ab_event_manager_open((AbEventManagerLimits){1, 1, 0, 0, 4, 65536},
                                (AbEventManagerHost){0}, &events, NULL));
    Fixture fixture = {.joints = &joints, .reject = reject};
    AbClassicJointUpdateConfig config = {.joints = &joints,
                                         .level_data = &data,
                                         .events = events,
                                         .destroyed_event = text("destroyed"),
                                         .context = &fixture,
                                         .print_destroy = printed,
                                         .names = 4,
                                         .requests = 4,
                                         .storage_bytes = 65536};
    AbClassicJoints invalid = {0};
    config.joints = &invalid;
    CHECK(!ab_classic_joint_updates_open(config, &fixture.updates, NULL) &&
          !fixture.updates);
    config.joints = &joints;
    CHECK(ab_classic_joint_updates_open(config, &fixture.updates, NULL));
    CHECK(ab_classic_joint_updates_register(fixture.updates, text("brace").string,
                                            NULL) &&
          ab_classic_joint_updates_register(fixture.updates, text("motor").string,
                                            NULL) &&
          ab_classic_joint_updates_refresh(fixture.updates, NULL));
    AbEventListener listener = {.name = text("observer").string,
                                .context = &fixture,
                                .event_triggered = notified};
    CHECK(ab_event_manager_add(events, text("destroyed"), &listener, NULL));
    CHECK(ab_classic_joint_updates_defer(fixture.updates, text("brace").string, NULL) &&
          ab_classic_joint_updates_defer(fixture.updates, text("brace").string, NULL));
    AbGameTailState state = {.joints = ab_classic_joint_updates_tail(fixture.updates)};
    AbError error = {0};
    bool okay =
        ab_game_update_tail(&state, NULL, 0, 1.0f / 60, 0, service, &fixture, &error);
    CHECK(fixture.checks == 1);
    if (reject) {
        CHECK(!okay && !strcmp(error.message, "Selected joint listener rejection") &&
              joints.joint_count == 2 && state.joints->destroy->count == 2 &&
              !ab_classic_joint_updates_refresh(fixture.updates, NULL));
    } else {
        CHECK(okay && fixture.notifications == 3 && fixture.prints == 3 &&
              !joints.joint_count && !world.joint_count &&
              !state.joints->destroy->count && fixture.saved &&
              field(fixture.saved, "name").kind == AB_DATA_STRING &&
              field(fixture.saved_event, "joint").table == fixture.saved &&
              ab_classic_joint_updates_refresh(fixture.updates, NULL) &&
              !state.joints->count);
    }
    ab_classic_joint_updates_close(fixture.updates);
    ab_event_manager_close(events);
    ab_classic_joints_close(&joints);
    ab_world_close(&world);
    return true;
}

int main(void) {
    return checks(false) && checks(true) ? 0 : 1;
}
