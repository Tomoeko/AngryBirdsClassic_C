#include "ab/block_removal_pass.h"
#include "ab/game_behavior.h"
#include "ab/game_object_removal.h"
#include "ab/object_removal_notify.h"
#include "ab/random.h"
#include "world_storage.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Removal pass failed at %d: %s\n", __LINE__, #condition);  \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    WorldStorage physics_storage;
    AbPhysicsWorld physics;
    AbGameObjectCore physical_object;
    AbGameObjectCoreRegistry physical_registry;
    AbClassicJoints joints;
    AbClassicJointRecord glue;
    AbGameTeleporter teleporter;
    AbGameObjectRemoval native_removal;
    AbDataTable body, rocket, world, dynamic, objects, raw, catalog, block_table;
    AbDataEntry body_fields[9], rocket_fields[3], world_fields[2], dynamic_fields[2];
    AbDataEntry objects_fields[2], raw_fields[7], catalog_field, blocks_field;
    AbDataValue body_value, rocket_value;
    AbStringMap *dead, *goals, *physical, *behaviors;
    AbGameBehaviorMethod method;
    AbGameBehavior behavior, *behavior_item;
    AbGameBehaviorGroup group;
    AbObjectRemovalLevel level;
    AbObjectRemovalNotifyHost notification;
    AbRandom48 random;
    AbBlockRemovalState state;
    AbBlockRemovalScores scores;
    float individual;
    AbBlockRemovalHost traversal;
    AbBlockRemovalScoreHost scoring;
    AbBlockRemovalEffectsHost effects;
    AbBlockRemovalParticlesHost particles;
    AbBlockRemovalDetachHost detach;
    AbBlockRemovalCleanupState cleanup_state;
    AbBlockRemovalCleanupHost cleanup;
    AbBlockRemovalPassHost pass;
    size_t destroyed, floated, randoms, notified, emitted, removed, pigs, gifts,
        rockets;
    bool fail_emit;
} Fixture;

static AbSpan span(const char *value) {
    return (AbSpan){(const uint8_t *)value, strlen(value)};
}

static AbDataValue text(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING, .string = span(value)};
}

static AbDataValue table(AbDataTable *value) {
    return (AbDataValue){.kind = AB_DATA_TABLE, .table = value};
}

static bool named(AbSpan value, const char *name) {
    return value.size == strlen(name) && !memcmp(value.data, name, value.size);
}

static bool behavior_destroy(void *context, const AbUiPointerValue *arguments,
                             size_t count, AbError *error) {
    Fixture *fixture = context;
    (void)error;
    CHECK(!arguments && !count);
    ++fixture->destroyed;
    return true;
}

static bool destroy(void *context, AbDataValue object, AbError *error) {
    Fixture *fixture = context;
    CHECK(object.kind == AB_DATA_TABLE && object.table == &fixture->body);
    return ab_game_object_destroy(fixture->behaviors, error);
}

static bool definition(void *context, AbSpan key, AbDataValue *output, AbError *error) {
    Fixture *fixture = context;
    (void)error;
    CHECK(named(key, "queued"));
    *output = table(&fixture->raw);
    return true;
}

static bool breaking_score(void *context, AbDataValue object, AbScoreNumber *output,
                           AbError *error) {
    Fixture *fixture = context;
    (void)error;
    CHECK(object.table == &fixture->body);
    AbBreakScoreInput input = {
        .block_increment = 500, .goal_increment = 5000, .level_goal_truthy = true};
    return ab_break_score(&input, output);
}

static bool floating(void *context, const AbBlockRemovalFloatingScore *request,
                     AbError *error) {
    Fixture *fixture = context;
    (void)error;
    CHECK(!request->text_present && request->sprite.kind == AB_DATA_NIL);
    CHECK(request->score.present && request->score.value == 5000 &&
          request->lifetime == 0.9f);
    ++fixture->floated;
    fixture->body_fields[0].value = text("renamed");
    fixture->pass.particles = &fixture->particles;
    return true;
}

static bool property(void *context, AbDataValue object, AbSpan key, AbDataValue *output,
                     AbError *error) {
    Fixture *fixture = context;
    (void)error;
    CHECK(object.table == &fixture->body);
    if (named(key, "particles")) {
        *output = text("old");
    } else {
        CHECK(named(key, "particleAmount") || named(key, "useNewParticleTable"));
        *output = named(key, "particleAmount") ? (AbDataValue){.kind = AB_DATA_NUMBER}
                                               : (AbDataValue){.kind = AB_DATA_BOOLEAN};
    }
    return true;
}

static bool destroyed_sound(void *context, AbDataValue object, AbDataValue raw,
                            AbDataValue *output, AbError *error) {
    Fixture *fixture = context;
    (void)error;
    CHECK(object.table == &fixture->body && raw.table == &fixture->raw);
    *output = (AbDataValue){0};
    return true;
}

static bool random_value(void *context, float lower, float upper, AbDataValue *output,
                         AbError *error) {
    Fixture *fixture = context;
    (void)error;
    CHECK(lower == 12 && upper == 20);
    float value = 0;
    CHECK(ab_script_random(&fixture->random, 2, 12, 20, &value) == AB_SCRIPT_RANDOM_OK);
    ++fixture->randoms;
    uint32_t word;
    memcpy(&word, &value, sizeof(word));
    *output = (AbDataValue){.kind = AB_DATA_NUMBER, .bits = word};
    return true;
}

static bool update(void *context, AbDataValue object, AbError *error) {
    Fixture *fixture = context;
    (void)error;
    CHECK(object.table == &fixture->body && fixture->randoms == 1);
    return true;
}

static bool notification_binding(void *context, AbDataValue key,
                                 AbUiPointerValue *output, AbError *error) {
    (void)context;
    (void)error;
    CHECK(key.kind == AB_DATA_STRING && named(key.string, "removed"));
    *output = (AbUiPointerValue){.kind = AB_UI_POINTER_FUNCTION, .function = 1};
    return true;
}

static bool notification_level(void *context, AbDataValue name,
                               AbObjectRemovalLevel **output, AbError *error) {
    Fixture *fixture = context;
    (void)error;
    CHECK(name.kind == AB_DATA_STRING && named(name.string, "level"));
    *output = &fixture->level;
    return true;
}

static bool notification_call(void *context, AbObjectRemovalNotice notice,
                              AbUiPointerValue function, AbObjectRemovalObject *object,
                              AbError *error) {
    Fixture *fixture = context;
    (void)error;
    CHECK(object->context == &fixture->body && function.kind == AB_UI_POINTER_FUNCTION);
    CHECK(function.function == (notice == AB_OBJECT_REMOVAL_DEFINITION ? 1u : 2u));
    ++fixture->notified;
    return true;
}

static bool notify(void *context, AbDataValue object, AbError *error) {
    Fixture *fixture = context;
    CHECK(object.table == &fixture->body);
    AbObjectRemovalObject view = {fixture->body_fields[8].value, object.table};
    return ab_object_removal_notify(&view, &fixture->notification, error);
}

static bool functions(void *context, AbDataValue object, AbUiPointerValue *output,
                      AbError *error) {
    Fixture *fixture = context;
    (void)error;
    CHECK(object.table == &fixture->body && fixture->notified == 2);
    *output = (AbUiPointerValue){0};
    return true;
}

static bool old_particles(void *context, AbSpan key, AbDataValue particles,
                          AbDataValue amount, bool flag1, bool flag2, AbError *error) {
    Fixture *fixture = context;
    CHECK(named(key, "queued") && particles.kind == AB_DATA_STRING &&
          named(particles.string, "old"));
    CHECK(amount.kind == AB_DATA_NUMBER && !amount.bits && !flag1 && !flag2);
    ++fixture->emitted;
    return fixture->fail_emit ? ab_fail(error, "Injected composed particle failure")
                              : true;
}

static bool remove_object(void *context, AbSpan key, AbError *error) {
    Fixture *fixture = context;
    CHECK(named(key, "queued") && fixture->emitted == 1);
    bool removed = false;
    CHECK(ab_game_object_remove(&fixture->native_removal, key, &removed, error) &&
          removed);
    CHECK(!ab_world_has_body(&fixture->physics, fixture->physical_object.body));
    AbStringMapLayout layout;
    CHECK(!fixture->joints.glue_count &&
          ab_string_map_layout(fixture->native_removal.teleporters, &layout) &&
          !layout.count);
    ++fixture->removed;
    fixture->raw_fields[3].value = (AbDataValue){.kind = AB_DATA_BOOLEAN};
    return true;
}

static bool remove_pig(void *context, AbSpan key, AbError *error) {
    Fixture *fixture = context;
    (void)error;
    CHECK(named(key, "queued") && fixture->removed == 1);
    ++fixture->pigs;
    fixture->body_fields[7].value = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1};
    fixture->body_fields[3].value = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1};
    fixture->body_fields[4].value = (AbDataValue){.kind = AB_DATA_BOOLEAN};
    fixture->cleanup_state.level_name = text("renamed");
    return true;
}

static bool collect_gift(void *context, AbDataValue level, AbDataValue x, AbDataValue y,
                         AbError *error) {
    Fixture *fixture = context;
    (void)error;
    CHECK(level.kind == AB_DATA_STRING && named(level.string, "renamed"));
    CHECK(x.kind == AB_DATA_BOOLEAN && x.bits && y.kind == AB_DATA_BOOLEAN && !y.bits);
    ++fixture->gifts;
    return true;
}

static bool shoot_rocket(void *context, AbDataValue name, AbError *error) {
    Fixture *fixture = context;
    (void)error;
    CHECK(name.kind == AB_DATA_STRING && named(name.string, "rocket"));
    ++fixture->rockets;
    return true;
}

static bool prepare(Fixture *fixture) {
    memset(fixture, 0, sizeof(*fixture));
    CHECK(ab_world_open(&fixture->physics, default_definition(),
                        prepare_storage(&fixture->physics_storage, 0, 0), NULL));
    AbBodyDefinition body = {
        .type = AB_BODY_DYNAMIC, .awake = true, .active = true, .gravity_scale = 1};
    fixture->physical_object.name = span("queued");
    CHECK(ab_world_create_body(&fixture->physics, &body, &fixture->physical_object.body,
                               NULL));
    const char *const fields[] = {"name",      "strength",  "levelGoal",    "x",
                                  "y",         "specialty", "killSilently", "isGiftBox",
                                  "definition"};
    for (size_t field = 0; field < 9; ++field) {
        fixture->body_fields[field].key = text(fields[field]);
    }
    fixture->body_fields[0].value = text("body");
    fixture->body_fields[1].value = (AbDataValue){.kind = AB_DATA_NUMBER};
    fixture->body_fields[2].value = (AbDataValue){.kind = AB_DATA_NUMBER};
    fixture->body_fields[8].value = text("raw");
    fixture->body = (AbDataTable){fixture->body_fields, 9, 9, NULL};
    fixture->rocket_fields[0] = (AbDataEntry){text("name"), text("rocket")};
    fixture->rocket_fields[1] =
        (AbDataEntry){text("strength"), {.kind = AB_DATA_NUMBER}};
    fixture->rocket_fields[2] =
        (AbDataEntry){text("rocket"), {.kind = AB_DATA_BOOLEAN}};
    fixture->rocket = (AbDataTable){fixture->rocket_fields, 3, 3, NULL};
    fixture->body_value = table(&fixture->body);
    fixture->rocket_value = table(&fixture->rocket);
    fixture->world_fields[0] = (AbDataEntry){text("queued"), fixture->body_value};
    fixture->world_fields[1] = (AbDataEntry){text("rocket"), fixture->rocket_value};
    memcpy(fixture->dynamic_fields, fixture->world_fields,
           sizeof(fixture->world_fields));
    fixture->world = (AbDataTable){fixture->world_fields, 2, 2, NULL};
    fixture->dynamic = (AbDataTable){fixture->dynamic_fields, 2, 2, NULL};
    fixture->objects_fields[0] = (AbDataEntry){text("world"), table(&fixture->world)};
    fixture->objects_fields[1] =
        (AbDataEntry){text("dynamic"), table(&fixture->dynamic)};
    fixture->objects = (AbDataTable){fixture->objects_fields, 2, 2, NULL};
    const char *const raw[] = {"spriteScore", "damageSound", "specialty", "spawn",
                               "deathSpawn",  "onRemoved",   "unused"};
    for (size_t field = 0; field < 7; ++field) {
        fixture->raw_fields[field].key = text(raw[field]);
    }
    fixture->raw_fields[4].value = (AbDataValue){.kind = AB_DATA_BOOLEAN};
    fixture->raw_fields[5].value = text("removed");
    fixture->raw = (AbDataTable){fixture->raw_fields, 7, 7, NULL};
    fixture->catalog_field = (AbDataEntry){text("raw"), table(&fixture->raw)};
    fixture->catalog = (AbDataTable){&fixture->catalog_field, 1, 1, NULL};
    fixture->blocks_field = (AbDataEntry){text("blocks"), table(&fixture->catalog)};
    fixture->block_table = (AbDataTable){&fixture->blocks_field, 1, 1, NULL};
    CHECK(ab_string_map_open(2, 2, 4096, &fixture->dead, NULL) &&
          ab_string_map_open(1, 1, 4096, &fixture->goals, NULL));
    CHECK(ab_string_map_open(1, 1, 4096, &fixture->physical, NULL) &&
          ab_string_map_open(1, 1, 4096, &fixture->behaviors, NULL));
    CHECK(ab_string_map_set(fixture->dead, span("queued"),
                            (uintptr_t)&fixture->body_value, true, NULL));
    CHECK(ab_string_map_set(fixture->dead, span("rocket"),
                            (uintptr_t)&fixture->rocket_value, true, NULL));
    CHECK(ab_string_map_set(fixture->goals, span("queued"), 1, true, NULL) &&
          ab_string_map_set(fixture->physical, span("queued"),
                            (uintptr_t)&fixture->physical_object, true, NULL));
    fixture->physical_registry = (AbGameObjectCoreRegistry){
        .world = &fixture->physics, .objects = fixture->physical};
    CHECK(ab_classic_joints_open(&fixture->joints, &fixture->physics,
                                 (AbClassicJointStorage){NULL, 0, &fixture->glue, 1},
                                 NULL));
    AbClassicJoint joint = {.type = AB_CLASSIC_JOINT_GLUE,
                            .name = span("joint"),
                            .endpoints = {span("queued"), span("absent")},
                            .destroy_timer = 1};
    size_t joint_slot;
    CHECK(ab_classic_joints_create(&fixture->joints, &joint, &joint_slot, NULL));
    fixture->native_removal.objects = &fixture->physical_registry;
    fixture->native_removal.joints = &fixture->joints;
    CHECK(ab_string_map_open(1, 1, 4096, &fixture->native_removal.teleporters, NULL) &&
          ab_string_map_set(fixture->native_removal.teleporters, span("queued"),
                            (uintptr_t)&fixture->teleporter, true, NULL));
    fixture->method =
        (AbGameBehaviorMethod){span("onDestroy"), true, fixture, behavior_destroy};
    fixture->behavior = (AbGameBehavior){&fixture->method, 1};
    fixture->behavior_item = &fixture->behavior;
    fixture->group = (AbGameBehaviorGroup){&fixture->behavior_item, 1};
    CHECK(ab_string_map_set(fixture->behaviors, span("group"),
                            (uintptr_t)&fixture->group, true, NULL));
    fixture->level.on_remove =
        (AbUiPointerValue){.kind = AB_UI_POINTER_FUNCTION, .function = 2};
    fixture->notification = (AbObjectRemovalNotifyHost){fixture,
                                                        table(&fixture->block_table),
                                                        text("level"),
                                                        notification_binding,
                                                        notification_level,
                                                        notification_call};
    CHECK(ab_random48_seed(&fixture->random, 123));
    fixture->state = (AbBlockRemovalState){table(&fixture->objects), fixture->dead};
    fixture->scores = (AbBlockRemovalScores){{0, true}, &fixture->individual, 0, 1};
    fixture->traversal = (AbBlockRemovalHost){fixture, shoot_rocket};
    fixture->scoring =
        (AbBlockRemovalScoreHost){fixture, destroy,  definition, breaking_score,  NULL,
                                  NULL,    floating, {1, true},  &fixture->scores};
    fixture->effects = (AbBlockRemovalEffectsHost){
        fixture, &fixture->state, property, destroyed_sound, random_value, NULL, NULL,
        NULL,    update};
    fixture->particles = (AbBlockRemovalParticlesHost){
        fixture, notify, functions, NULL, property, NULL, old_particles};
    fixture->detach = (AbBlockRemovalDetachHost){fixture, table(&fixture->block_table),
                                                 remove_object};
    fixture->cleanup_state =
        (AbBlockRemovalCleanupState){&fixture->state, fixture->goals, text("level")};
    fixture->cleanup = (AbBlockRemovalCleanupHost){fixture, remove_pig, collect_gift};
    fixture->pass =
        (AbBlockRemovalPassHost){&fixture->traversal,     &fixture->scoring,
                                 &fixture->effects,       NULL,
                                 &fixture->detach,        NULL,
                                 &fixture->cleanup_state, &fixture->cleanup};
    return true;
}

static void close_fixture(Fixture *fixture) {
    ab_string_map_close(fixture->native_removal.teleporters);
    ab_string_map_close(fixture->dead);
    ab_string_map_close(fixture->goals);
    ab_string_map_close(fixture->physical);
    ab_string_map_close(fixture->behaviors);
    ab_world_close(&fixture->physics);
}

static bool controls(void) {
    Fixture fixture;
    CHECK(prepare(&fixture));
    CHECK(ab_block_removal_pass(&fixture.state, &fixture.pass, NULL));
    CHECK(fixture.destroyed == 1 && fixture.floated == 1 && fixture.randoms == 1 &&
          fixture.notified == 2);
    CHECK(fixture.emitted == 1 && fixture.removed == 1 && fixture.pigs == 1 &&
          fixture.gifts == 1 && fixture.rockets == 1);
    CHECK(fixture.scores.total.value == 5000 && fixture.individual == 5000 &&
          fixture.scores.count == 1);
    AbStringMapLayout layout;
    CHECK(ab_string_map_layout(fixture.dead, &layout) && !layout.count);
    CHECK(ab_string_map_layout(fixture.goals, &layout) && !layout.count);
    CHECK(ab_string_map_layout(fixture.physical, &layout) && !layout.count);
    CHECK(!fixture.physics.body_count);
    CHECK(fixture.world_fields[0].value.kind == AB_DATA_NIL &&
          fixture.dynamic_fields[0].value.kind == AB_DATA_NIL);
    close_fixture(&fixture);
    CHECK(prepare(&fixture));
    fixture.fail_emit = true;
    CHECK(!ab_block_removal_pass(&fixture.state, &fixture.pass, NULL));
    CHECK(fixture.destroyed == 1 && fixture.notified == 2 && fixture.emitted == 1 &&
          fixture.removed == 0);
    CHECK(fixture.scores.total.value == 5000 && fixture.scores.count == 1);
    uintptr_t value = 0;
    bool present = false;
    CHECK(ab_string_map_get(fixture.dead, span("queued"), &value, &present, NULL) &&
          present);
    CHECK(ab_string_map_get(fixture.physical, span("queued"), &value, &present, NULL) &&
          present);
    CHECK(fixture.world_fields[0].value.kind == AB_DATA_TABLE);
    CHECK(ab_world_has_body(&fixture.physics, fixture.physical_object.body));
    CHECK(fixture.joints.glue_count == 1 &&
          ab_string_map_get(fixture.native_removal.teleporters, span("queued"), &value,
                            &present, NULL) &&
          present);
    fixture.objects_fields[0].value = (AbDataValue){0};
    fixture.state.dead_blocks = NULL;
    CHECK(ab_block_removal_pass(&fixture.state, NULL, NULL));
    close_fixture(&fixture);
    return true;
}

int main(void) {
    return controls() ? 0 : 1;
}
