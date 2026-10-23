#include "ab/classic_level.h"
#include "ab/classic_forces.h"
#include "ab/game_update_tail.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Classic level check failed at %d: %s\n", __LINE__,        \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static AbDataValue string(const char *text) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)text, strlen(text)}};
}

static AbDataValue table(AbDataTable *value) {
    return (AbDataValue){.kind = AB_DATA_TABLE, .table = value};
}

static AbDataValue number(float value) {
    AbDataValue result = {.kind = AB_DATA_NUMBER};
    memcpy(&result.bits, &value, sizeof(value));
    return result;
}

#include "classic_level_joints.h"
#include "classic_contacts.h"
#include "classic_impact_prediction.h"

typedef struct {
    AbClassicForces *forces;
    AbForceQueue *queue;
    size_t applied, cleared;
} ClassicQueueCheck;

static bool classic_queue_hook(void *context, AbGamePhysicsStage stage,
                               AbPhysicsWorld *world, float seconds, AbError *error) {
    (void)world;
    (void)seconds;
    ClassicQueueCheck *check = context;
    if (stage == AB_GAME_PHYSICS_APPLY_QUEUED_FORCES) {
        ++check->applied;
        return ab_classic_forces_apply_queue(check->forces, check->queue, error);
    }
    if (stage == AB_GAME_PHYSICS_CLEAR_FORCE_FUNCTIONS) {
        ++check->cleared;
        return ab_force_queue_clear(check->queue, error);
    }
    return true;
}

static bool jump_random(void *context, int minimum, int maximum, int *output) {
    float value;
    CHECK(ab_script_random(context, 2, minimum, maximum, &value) ==
          AB_SCRIPT_RANDOM_OK);
    *output = (int)value;
    return true;
}

static bool transform_checks(AbClassicLevel *level) {
    AbClassicLevelView view = ab_classic_level_view(level);
    CHECK(view.bodies->count == 2 && view.bird_count == 1);
    AbGameFrameObject *frame = view.frame->objects;
    AbBirdJumpState *jump = &view.character_objects[0].jump;
    AbBodyMotion *body = &view.world->storage.bodies[frame->body].motion;
    frame->history[0].angle = 0.25f;
    frame->history[1].angle = 0.5f;
    frame->rendered.angle = 0.75f;
    CHECK(!ab_classic_level_set_position(NULL, 0, (AbVec2){0}, NULL) &&
          !ab_classic_level_set_rotation(level, 2, 0, NULL) &&
          ab_classic_level_set_position(level, 0, (AbVec2){1, 2}, NULL) &&
          jump->position.y == 2 && frame->rendered.position.y == 2 &&
          frame->history[0].position.y == 2 && frame->history[1].position.y == 2 &&
          frame->history[0].angle == 0.25f && frame->history[1].angle == 0.5f &&
          frame->rendered.angle == 0.75f &&
          ab_classic_level_set_rotation(level, 0, -0.5f, NULL) && jump->rotation > 0 &&
          frame->history[0].angle == jump->rotation &&
          frame->history[1].angle == jump->rotation &&
          frame->rendered.angle == jump->rotation);
    AbLevelRaycast ray = {0};
    CHECK(!ab_classic_level_raycast(NULL, (AbVec2){0}, (AbVec2){0}, &ray, NULL));
    view.world->locked = true;
    bool queried =
        ab_classic_level_raycast(level, (AbVec2){-1, 2}, (AbVec2){3, 2}, &ray, NULL);
    view.world->locked = false;
    CHECK(queried && ray.hit && ray.name.kind == AB_DATA_STRING &&
          ray.name.string.size == view.bodies->objects[0].name.size &&
          !memcmp(ray.name.string.data, view.bodies->objects[0].name.data,
                  ray.name.string.size));
    CHECK(classic_prediction_checks(level));
    CHECK(classic_camera_world_checks(level));
    CHECK(classic_owned_camera_checks(level));
    AbBirdJumpFrame invalid = {
        .events = {{AB_BIRD_JUMP_POSITION, {9, 10}}, {(AbBirdJumpEventKind)99, {0, 0}}},
        .event_count = 2};
    CHECK(!ab_classic_level_apply_jump(level, 0, &invalid, NULL) &&
          jump->position.y == 2 && body->mass.transform.position.y == 2);
    AbRandom48 random, expected;
    CHECK(ab_random48_seed(&random, 7));
    expected = random;
    uint32_t unused;
    CHECK(ab_random48_next(&expected, &unused) && ab_random48_next(&expected, &unused));
    view.character_objects[0].bird.timer = 1000;
    jump->jumping = false;
    jump->timer = -1;
    AbCharacterAnimationBindings bindings[2] = {0};
    bindings[0].jump = (AbBirdJumpRandom){&random, jump_random};
    AbCharacterAnimations animations = {.objects = view.character_objects,
                                        .bindings = bindings,
                                        .count = view.bodies->count,
                                        .bird_order = view.bird_order,
                                        .bird_count = view.bird_count,
                                        .current_bird = SIZE_MAX,
                                        .context = level,
                                        .apply_jump = ab_classic_level_apply_jump};
    CHECK(ab_character_animations_update(&animations, 0.1f, NULL) && jump->jumping &&
          random.value == expected.value && jump->position.y < 2 &&
          jump->position.y == body->mass.transform.position.y &&
          frame->rendered.position.y == jump->position.y &&
          frame->history[0].position.y == jump->position.y &&
          frame->history[1].position.y == jump->position.y);
    CHECK(ab_character_animations_update(&animations, 10, NULL) && jump->rebound &&
          ab_character_animations_update(&animations, 10, NULL) && !jump->jumping &&
          jump->position.y == 2 && body->mass.transform.position.y == 2 &&
          frame->rendered.position.y == 2 && frame->rendered.angle == jump->rotation &&
          random.value == expected.value);
    CHECK(view.motion_properties && !view.motion_properties[0].initialized);
    frame->rendered.position.x = body->mass.transform.position.x + 0.25f;
    AbObjectPropertyPlan properties;
    CHECK(ab_object_properties_plan(body, frame, true, -100, 100, &properties, NULL) &&
          properties.publish && properties.values.pose.angle < 0 &&
          frame->rendered.angle > 0);
    AbGameFrameObject prior_frame = *frame;
    AbBodyMotion prior_body = *body;
    CHECK(!ab_classic_level_publish_properties(NULL, 0, &properties.values, NULL) &&
          !ab_classic_level_publish_properties(level, 2, &properties.values, NULL) &&
          !ab_classic_level_publish_properties(level, 0, NULL, NULL) &&
          ab_classic_level_publish_properties(level, 0, &properties.values, NULL) &&
          jump->position.x == frame->rendered.position.x &&
          jump->rotation == properties.values.pose.angle &&
          view.motion_properties[0].initialized &&
          !view.motion_properties[1].initialized &&
          view.motion_properties[0].velocity.x == body->mass.linear_velocity.x &&
          view.motion_properties[0].velocity.y == body->mass.linear_velocity.y &&
          view.motion_properties[0].sleeping == !body->awake &&
          !view.motion_properties[0].outside_boundaries &&
          memcmp(frame, &prior_frame, sizeof(*frame)) == 0 &&
          memcmp(body, &prior_body, sizeof(*body)) == 0);
    CHECK(!view.motion_properties[0].previous_awake &&
          !ab_classic_level_property_plan(NULL, 0, 0, 0, &properties, NULL) &&
          !ab_classic_level_finish_object(level, 2, NULL));
    body->awake = true;
    CHECK(ab_classic_level_property_plan(level, 0, -100, 100, &properties, NULL) &&
          properties.publish && !properties.values.sleeping &&
          ab_classic_level_finish_object(level, 0, NULL) &&
          view.motion_properties[0].previous_awake);
    body->awake = false;
    CHECK(ab_classic_level_property_plan(level, 0, -100, 100, &properties, NULL) &&
          properties.publish && properties.values.sleeping &&
          ab_classic_level_publish_properties(level, 0, &properties.values, NULL) &&
          view.motion_properties[0].previous_awake &&
          ab_classic_level_finish_object(level, 0, NULL) &&
          !view.motion_properties[0].previous_awake &&
          ab_classic_level_property_plan(level, 0, -100, 100, &properties, NULL) &&
          !properties.publish);
    return true;
}

static bool dynamic_circle_checks(AbClassicLevel *level) {
    AbClassicLevelView view = ab_classic_level_view(level);
    CHECK(view.bodies->count == 2 && view.bodies->capacity == 4 &&
          view.world_count == 2);
    const AbLevelObject *old_objects = view.bodies->objects;
    AbLevelPropertyObject *old_properties = view.properties->objects;
    AbGameFrameObject *old_frames = view.frame->objects;
    AbDataTable *old_bird = &old_properties[0].physical;
    AbGameContactRoutes routes;
    CHECK(ab_game_contact_routes_initialize(&routes));
    AbClassicContactObject contacts_objects[4] = {
        {.velocity = {{-0.0f, 0.5f}, true}, .script_table_present = true},
        {.script_table_present = true},
        {.script_table_present = true},
        {.script_table_present = true}};
    AbClassicContacts contacts = {0};
    DynamicContactCheck contact_check = {.contacts = &contacts};
    CHECK(ab_classic_contacts_open(
        level, contacts_objects, 2,
        (AbClassicContactsConfig){.routes = &routes,
                                  .context = &contact_check,
                                  .collision = dynamic_contact_begin,
                                  .end = ab_classic_contacts_play_mode_end,
                                  .object_capacity = 4},
        &contacts, NULL));
    size_t index = 71;
    AbGameCircleInput input = {.name = string("Bird_clone_a").string,
                               .position = {4, -5},
                               .radius = 0.4f,
                               .density = 0.5f,
                               .friction = 0.3f,
                               .restitution = 0.2f,
                               .controllable = true,
                               .z_order = -0.25f};
    CHECK(!ab_classic_level_create_circle(NULL, input, &index, NULL) && index == 71);
    view.world->locked = true;
    bool locked = ab_classic_level_create_circle(level, input, &index, NULL);
    view.world->locked = false;
    CHECK(!locked && index == 71 && view.bodies->count == 2);
    CHECK(ab_classic_level_create_circle(level, input, &index, NULL) && index == 2);
    CHECK(!ab_classic_contacts_register_object(&contacts, 3, NULL));
    view.world->locked = true;
    CHECK(!ab_classic_contacts_register_object(&contacts, 2, NULL));
    view.world->locked = false;
    CHECK(ab_classic_contacts_register_object(&contacts, 2, NULL) &&
          !ab_classic_contacts_register_object(&contacts, 2, NULL) &&
          contacts.count == 3 && contacts_objects[0].velocity.pending &&
          signbit(contacts_objects[0].velocity.velocity.x));
    view = ab_classic_level_view(level);
    CHECK(view.bodies->objects == old_objects &&
          view.properties->objects == old_properties &&
          view.frame->objects == old_frames && view.bodies->count == 3 &&
          view.frame->count == 3 && view.properties->count == 3 &&
          view.world_count == 3 && view.properties->world.count == 3 &&
          view.properties->dynamic.count == 1 && view.bird_count == 1 &&
          view.goal_count == 0 && view.order_hash_capacity == 4 &&
          view.properties->objects[2].physical.entries ==
              view.properties->objects[2].physical_entries &&
          !view.properties->objects[2].script.count &&
          !view.bodies->objects[2].definition_name.size &&
          view.frame->objects[2].history[0].position.x == 4 &&
          view.world->storage.fixtures[view.bodies->objects[2].physics.fixture]
                  .user_data == (uintptr_t)(old_objects + 2));
    const AbDataValue *published =
        ab_data_get(&view.properties->world, string("Bird_clone_a"));
    CHECK(published && published->kind == AB_DATA_TABLE &&
          published->table == &view.properties->objects[2].physical);
    const AbDataValue *original =
        ab_data_get(&view.properties->world, string("Bird_1"));
    CHECK(original && original->table == old_bird);
    index = 71;
    CHECK(!ab_classic_level_create_circle(level, input, &index, NULL) && index == 71 &&
          view.bodies->count == 3);
    size_t old_body = old_objects[0].physics.body;
    CHECK(ab_world_destroy_body(view.world, old_body, NULL) &&
          ab_classic_level_forget_object(level, 0, NULL) &&
          ab_level_properties_forget(view.properties, 0, NULL));
    input.name = string("Bird_clone_b").string;
    input.position.x = 10;
    CHECK(ab_classic_level_create_circle(level, input, &index, NULL) && index == 3);
    CHECK(ab_classic_contacts_register_object(&contacts, 3, NULL) &&
          !ab_classic_contacts_flush(&contacts, 0, NULL));
    AbDeferredVelocity *retired_pending = &contacts_objects[0].velocity;
    size_t retired_body = 0;
    AbGameContactHost host = contacts.listener.config.basic;
    CHECK(
        host.velocity(host.context, string("Bird_1").string, &retired_pending, NULL) &&
        !retired_pending &&
        host.body(host.context, string("Bird_1").string, &retired_body, NULL) &&
        retired_body == SIZE_MAX);
    view = ab_classic_level_view(level);
    CHECK(view.bodies->objects[3].physics.body == old_body && view.bodies->count == 4 &&
          view.frame->count == 4 && view.world_count == 3 && view.removed_objects[0] &&
          !view.removed_objects[3] && view.world->body_count == 3 &&
          view.frame->objects[3].history[1].position.x == 10 &&
          view.object_order[3] == 0 &&
          view.properties->objects[0].physical.entries ==
              old_properties[0].physical_entries);
    unsigned seen = 0;
    for (size_t cursor = 0; cursor < view.world_count; ++cursor) {
        CHECK(view.object_order[cursor] > 0 && view.object_order[cursor] < 4);
        seen |= 1u << view.object_order[cursor];
    }
    CHECK(seen == 14 &&
          ab_classic_level_set_position(level, 3, (AbVec2){11, -6}, NULL) &&
          ab_classic_level_set_rotation(level, 3, -0.5f, NULL) &&
          view.frame->objects[3].history[0].position.x == 11 &&
          view.frame->objects[3].history[0].angle > 0);
    index = 71;
    input.name = string("Bird_clone_c").string;
    CHECK(!ab_classic_level_create_circle(level, input, &index, NULL) && index == 71 &&
          view.bodies->count == 4 && view.world->body_count == 3);
    CHECK(ab_world_set_transform(view.world, old_objects[2].physics.body,
                                 (AbVec2){20, -10}, 0, NULL) &&
          ab_world_set_transform(view.world, old_objects[3].physics.body,
                                 (AbVec2){20.5f, -10}, 0, NULL) &&
          ab_world_step(view.world, 0x1.111112p-5f, 8, 3, NULL) &&
          contact_check.begins == 1 && contacts_objects[3].velocity.pending &&
          contacts_objects[0].velocity.pending &&
          ab_classic_contacts_flush(&contacts, 3, NULL));
    const AbBodyMotion *replacement =
        &view.world->storage.bodies[old_objects[3].physics.body].motion;
    CHECK(replacement->mass.linear_velocity.x == 1 &&
          replacement->mass.linear_velocity.y == -2 &&
          !contacts_objects[3].velocity.pending &&
          !ab_classic_contacts_register_object(&contacts, 4, NULL) &&
          ab_classic_contacts_close(&contacts, NULL));
    return true;
}

static bool dynamic_box_checks(AbClassicLevel *level) {
    AbClassicLevelView view = ab_classic_level_view(level);
    CHECK(view.bodies->count == 2 && view.bodies->capacity == 4);
    const AbDataTable *bird = &view.properties->objects[0].physical;
    AbGameBoxInput input = {.name = string("generated_hat_1").string,
                            .position = {4, -5},
                            .size = {1.5f, 1.7f},
                            .density = 0.001f,
                            .friction = 1,
                            .restitution = 0,
                            .z_order = 10,
                            .collision = true};
    size_t object = 71;
    CHECK(!ab_classic_level_create_box(NULL, input, &object, NULL) && object == 71);
    view.world->locked = true;
    bool locked = ab_classic_level_create_box(level, input, &object, NULL);
    view.world->locked = false;
    CHECK(!locked && object == 71 && view.bodies->count == 2);
    input.size.x = -1;
    CHECK(!ab_classic_level_create_box(level, input, &object, NULL) && object == 71 &&
          view.bodies->count == 2 && view.world->body_count == 2);
    input.size.x = 1.5f;
    CHECK(ab_classic_level_create_box(level, input, &object, NULL) && object == 2);
    view = ab_classic_level_view(level);
    const AbLevelObject *box = view.bodies->objects + object;
    const AbDataTable *physical = &view.properties->objects[object].physical;
    const AbDataValue *published =
        ab_data_get(&view.properties->world, string("generated_hat_1"));
    CHECK(published && published->table == physical &&
          physical->entries == view.properties->objects[object].physical_entries &&
          view.frame->count == 3 && view.properties->count == 3 &&
          view.properties->world.count == 3 && view.world_count == 3 &&
          view.bird_count == 1 && view.properties->dynamic.count == 1 &&
          !box->physics.controllable && box->physics.dimensions.radius == -1 &&
          box->physics.dimensions.width == 1.5f &&
          box->physics.dimensions.height == 1.7f && box->initial_mass > 0 &&
          view.world->storage.fixtures[box->physics.fixture].user_data ==
              (uintptr_t)box &&
          view.frame->objects[object].history[1].position.y == -5);
    AbLevelRaycast ray = {0};
    CHECK(
        ab_classic_level_raycast(level, (AbVec2){2, -5}, (AbVec2){6, -5}, &ray, NULL) &&
        ray.hit && ray.name.kind == AB_DATA_STRING &&
        ray.name.string.size == input.name.size &&
        !memcmp(ray.name.string.data, input.name.data, input.name.size));
    object = 71;
    CHECK(!ab_classic_level_create_box(level, input, &object, NULL) && object == 71);
    AbGameCircleInput circle = {.name = string("Bird_clone").string,
                                .position = {8, -5},
                                .radius = 0.4f,
                                .density = 0.5f};
    CHECK(ab_classic_level_create_circle(level, circle, &object, NULL) && object == 3);
    published = ab_data_get(&view.properties->world, string("generated_hat_1"));
    const AbDataValue *original =
        ab_data_get(&view.properties->world, string("Bird_1"));
    CHECK(published && published->table == physical && original &&
          original->table == bird && view.bodies->count == 4 &&
          view.frame->objects[2].history[0].position.x == 4);
    input.name = string("generated_hat_2").string;
    object = 71;
    CHECK(!ab_classic_level_create_box(level, input, &object, NULL) && object == 71 &&
          view.world->body_count == 4);
    return true;
}

static bool bird_order_checks(void) {
    AbLevelObject objects[3] = {
        {.name = {(const uint8_t *)"Bird", 4}, .physics = {.controllable = true}},
        {.name = {(const uint8_t *)"Block", 5}},
        {.name = {(const uint8_t *)"ground", 6}},
    };
    AbLevelBodies bodies = {.objects = objects, .capacity = 3, .count = 3};
    AbDataEntry entries[3];
    size_t indices[3], insertion[] = {2, 0, 1};
    AbStringOrderNode nodes[4];
    AbLevelObjectOrder order = {.entries = entries,
                                .entry_capacity = 3,
                                .strings = {indices, 3, nodes, 4},
                                .hash_capacity = 77};
    size_t count = 88;
    CHECK(!ab_level_birds_order(NULL, insertion, &order, &count, NULL));
    CHECK(!ab_level_birds_order(&bodies, NULL, &order, &count, NULL));
    insertion[2] = 3;
    CHECK(!ab_level_birds_order(&bodies, insertion, &order, &count, NULL));
    insertion[2] = 2;
    CHECK(!ab_level_birds_order(&bodies, insertion, &order, &count, NULL));
    CHECK(count == 88 && order.hash_capacity == 77);
    insertion[2] = 1;
    CHECK(ab_level_birds_order(&bodies, insertion, &order, &count, NULL) &&
          count == 1 && order.hash_capacity == 1 && indices[0] == 0);
    objects[0].physics.controllable = false;
    order.strings = (AbStringOrderWorkspace){0};
    CHECK(ab_level_birds_order(&bodies, insertion, &order, &count, NULL) &&
          count == 0 && order.hash_capacity == 0);
    order.strings = (AbStringOrderWorkspace){indices, 3, nodes, 4};
    objects[1].level_goal = true;
    CHECK(ab_level_goals_order(&bodies, insertion, &order, &count, NULL) &&
          count == 1 && order.hash_capacity == 1 && indices[0] == 1);
    return true;
}

typedef struct {
    AbClassicLevel *level;
    AbClassicWorldCursor cursor;
    AbGameTailObject rows[4];
    unsigned calls[4], scripts, beginnings;
    size_t removed;
} TailWorldCheck;

static bool tail_world_begin(void *context, AbError *error) {
    (void)error;
    TailWorldCheck *check = context;
    check->cursor = (AbClassicWorldCursor){0};
    ++check->beginnings;
    return true;
}

static bool tail_world_next(void *context, AbGameTailObject **output, AbError *error) {
    TailWorldCheck *check = context;
    size_t object;
    bool present;
    CHECK(ab_classic_level_world_next(check->level, &check->cursor, &object, &present,
                                      error));
    CHECK(!present || object < 4);
    *output = present ? check->rows + object : NULL;
    return true;
}

/* Controlled phase callbacks test the tail/owned-world boundary. They do not
 * stand in for gameplay services or establish full-frame native fidelity. */
static bool tail_world_call(void *context, AbGameTailCall *call, AbError *error) {
    TailWorldCheck *check = context;
    AbClassicLevelView view = ab_classic_level_view(check->level);
    if (call->object) {
        size_t object = call->object->identity;
        CHECK(object < 3);
        if (call->operation == AB_GAME_TAIL_OBJECT_SCRIPT) {
            CHECK(!view.removed_objects[object]);
            check->scripts |= 1u << object;
            return true;
        }
        CHECK(call->operation >= AB_GAME_TAIL_ANIMATION &&
              call->operation <= AB_GAME_TAIL_OBJECT_EFFECT);
        check->calls[object] |= 1u << (call->operation - AB_GAME_TAIL_ANIMATION);
        if (check->removed == SIZE_MAX) {
            CHECK(call->operation == AB_GAME_TAIL_ANIMATION);
            check->removed = object;
            CHECK(ab_world_destroy_body(
                      view.world, view.bodies->objects[object].physics.body, error) &&
                  ab_classic_level_forget_object(check->level, object, error) &&
                  ab_level_properties_forget(view.properties, object, error));
        }
        call->result = true;
    } else if (call->operation == AB_GAME_TAIL_BLOCK_SCRIPTS) {
        size_t created = SIZE_MAX;
        CHECK(ab_classic_level_create_circle(
                  check->level,
                  (AbGameCircleInput){.name = string("world_tail_child").string,
                                      .radius = 0.5f,
                                      .density = 1},
                  &created, error) &&
              created == 2);
    }
    return true;
}

static bool tail_world_checks(const AbLevelBodySource *source,
                              AbWorldDefinition physics) {
    TailWorldCheck check = {.removed = SIZE_MAX};
    for (size_t index = 0; index < 4; ++index)
        check.rows[index] = (AbGameTailObject){index, true};
    CHECK(ab_classic_level_open(
        source, physics,
        (AbClassicLevelLimits){.objects = 4, .contacts = 16, .storage_bytes = 65536},
        false, &check.level, NULL));
    AbGameTailState state = {
        .cursor = {2, 3}, .skip_input = true, .time_since_present = true};
    CHECK(ab_game_update_tail_world(
        &state, (AbGameTailWorld){&check, tail_world_begin, tail_world_next}, 0.125f, 4,
        tail_world_call, &check, NULL));
    CHECK(check.beginnings == 2 && check.removed < 2 && check.calls[0] == 31 &&
          check.calls[1] == 31 && !check.calls[2] &&
          check.scripts == (7u ^ (1u << check.removed)) && state.old_cursor.x == 2 &&
          !state.skip_input && state.time_since_start == 0.125f);
    ab_classic_level_close(check.level);
    return true;
}

static bool world_cursor_checks(const AbLevelBodySource *source,
                                AbWorldDefinition physics) {
    AbClassicLevel *level = NULL;
    CHECK(ab_classic_level_open(
        source, physics,
        (AbClassicLevelLimits){
            .objects = 32, .contacts = 32, .storage_bytes = 2 * 1024 * 1024},
        false, &level, NULL));
    AbClassicLevelView view = ab_classic_level_view(level);
    AbClassicWorldCursor cursor = {0};
    size_t object = SIZE_MAX;
    bool present = false;
    for (size_t order = 0; order < view.bodies->count; ++order) {
        CHECK(ab_classic_level_world_next(level, &cursor, &object, &present, NULL) &&
              present && object == view.object_order[order]);
    }
    CHECK(ab_classic_level_world_next(level, &cursor, &object, &present, NULL) &&
          !present && object == SIZE_MAX);
    cursor = (AbClassicWorldCursor){0};
    CHECK(ab_classic_level_world_next(level, &cursor, &object, &present, NULL) &&
          present);
    size_t removed = object;
    CHECK(ab_world_destroy_body(view.world, view.bodies->objects[object].physics.body,
                                NULL) &&
          ab_classic_level_forget_object(level, object, NULL) &&
          ab_level_properties_forget(view.properties, object, NULL));
    AbClassicWorldCursor retained = cursor;
    CHECK(ab_classic_level_world_next(level, &cursor, &object, &present, NULL));
    char names[20][32];
    bool invalidated = false;
    for (size_t index = 0; index < 20; ++index) {
        int length =
            snprintf(names[index], sizeof(names[index]), "world_new_%zu", index);
        CHECK(length > 0 && (size_t)length < sizeof(names[index]));
        size_t created = SIZE_MAX;
        CHECK(ab_classic_level_create_circle(
                  level,
                  (AbGameCircleInput){
                      .name = {(const uint8_t *)names[index], (size_t)length},
                      .radius = 0.5f,
                      .density = 1},
                  &created, NULL) &&
              created == index + 2);
        cursor = retained;
        object = 99;
        present = true;
        if (!ab_classic_level_world_next(level, &cursor, &object, &present, NULL)) {
            CHECK(object == 99 && present && cursor.started == retained.started &&
                  cursor.name.data == retained.name.data);
            invalidated = true;
        }
    }
    CHECK(invalidated);
    cursor = (AbClassicWorldCursor){0};
    size_t visited = 0;
    uint32_t seen = 0;
    for (;;) {
        CHECK(ab_classic_level_world_next(level, &cursor, &object, &present, NULL));
        if (!present)
            break;
        CHECK(object < 22 && object != removed && !(seen & (UINT32_C(1) << object)));
        seen |= UINT32_C(1) << object;
        ++visited;
    }
    CHECK(visited == 21 &&
          seen == ((UINT32_C(1) << 22) - 1) - (UINT32_C(1) << removed));
    cursor = (AbClassicWorldCursor){.name = string("foreign").string, .started = true};
    object = 99;
    present = true;
    CHECK(!ab_classic_level_world_next(level, &cursor, &object, &present, NULL) &&
          object == 99 && present);
    ab_classic_level_close(level);
    return true;
}

static bool owner_checks(void) {
    AbZip archive = {0};
    AbSpriteSet *sprites = NULL;
    const char *paths[] = {"missing.dat", NULL};
    CHECK(!ab_sprite_set_open(NULL, paths, 1, 65536, &sprites, NULL));
    CHECK(!ab_sprite_set_open(&archive, paths, 1, 65536, NULL, NULL));
    CHECK(!ab_sprite_set_open(&archive, NULL, 1, 65536, &sprites, NULL));
    CHECK(!ab_sprite_set_open(&archive, paths, SIZE_MAX, 65536, &sprites, NULL));
    CHECK(!ab_sprite_set_open(&archive, paths, 1, 1, &sprites, NULL));
    CHECK(!ab_sprite_set_open(&archive, paths, 1, 65536, &sprites, NULL));
    CHECK(!ab_sprite_set_open(&archive, paths + 1, 1, 65536, &sprites, NULL));
    CHECK(!sprites);
    CHECK(ab_sprite_set_open(&archive, NULL, 0, 65536, &sprites, NULL));
    AbSpriteBinding binding = {0};
    CHECK(!ab_sprite_set_find(sprites, string("missing").string, &binding));
    CHECK(!binding.sheet && !binding.region && !binding.metadata_path.data);
    ab_sprite_set_close(sprites);
    ab_sprite_set_close(NULL);
    AbDataEntry properties[] = {
        {string("type"), string("circle")},
        {string("radius"), number(0.5f)},
        {string("density"), number(1)},
        {string("friction"), number(0.3f)},
        {string("restitution"), number(0)},
        {string("collider"), number(3)},
        {string("controllable"), {.kind = AB_DATA_BOOLEAN, .bits = 1}},
        {string("forceX"), number(0)},
        {string("forceY"), number(2)},
    };
    AbDataTable definition = {properties, 9, 9, NULL};
    AbDataTable empty = {0};
    AbDataEntry block[] = {{string("Bird"), table(&definition)},
                           {string("Ground"), table(&empty)}};
    AbDataTable blocks = {block, 2, 2, NULL};
    AbDataEntry catalog_entries[] = {{string("blocks"), table(&blocks)},
                                     {string("materials"), table(&empty)},
                                     {string("collider_types"), table(&empty)}};
    AbDataTable catalog = {catalog_entries, 3, 3, NULL};
    AbDataEntry placement_entries[] = {
        {string("name"), string("Bird_1")}, {string("definition"), string("Bird")},
        {string("x"), number(1)},           {string("y"), number(2)},
        {string("angle"), number(0.5f)},
    };
    AbDataTable placement = {placement_entries, 5, 5, NULL};
    AbDataEntry world_entry = {string("Bird_1"), table(&placement)};
    AbDataTable world_table = {&world_entry, 1, 1, NULL};
    AbDataEntry globals_entries[] = {{string("world"), table(&world_table)},
                                     {string("joints"), table(&empty)}};
    AbDataTable globals = {globals_entries, 2, 2, NULL};
    AbVec2 gravity = {3, 4};
    CHECK(!ab_level_gravity(NULL, &gravity, NULL));
    CHECK(!ab_level_gravity(&globals, NULL, NULL));
    CHECK(gravity.x == 3 && gravity.y == 4);
    CHECK(ab_level_gravity(&globals, &gravity, NULL) && gravity.x == 0 &&
          gravity.y == 20);
    AbDataEntry gravity_entry = {string("worldGravity"), number(-9.8f)};
    AbDataTable gravity_table = {&gravity_entry, 1, 1, NULL};
    CHECK(ab_level_gravity(&gravity_table, &gravity, NULL) && gravity.x == 0 &&
          gravity.y == -9.8f);
    gravity_entry.value = number(-0.0f);
    CHECK(ab_level_gravity(&gravity_table, &gravity, NULL) && signbit(gravity.y));
    gravity_entry.value = (AbDataValue){.kind = AB_DATA_BOOLEAN};
    CHECK(ab_level_gravity(&gravity_table, &gravity, NULL) && gravity.y == 20);
    gravity_entry.value.bits = 1;
    CHECK(!ab_level_gravity(&gravity_table, &gravity, NULL) && gravity.y == 20);
    gravity_entry.value = number(NAN);
    CHECK(!ab_level_gravity(&gravity_table, &gravity, NULL) && gravity.y == 20);
    AbDataTableLayout layout = {
        .table = &world_table, .hash_capacity = 1, .no_nil_assignments = true};
    AbLuaData data = {.globals = &globals, .layouts = &layout};
    AbLevelBodySource source = {.level = &data,
                                .catalog = &catalog,
                                .placement_scale = 1,
                                .physics_scale = 0.05f,
                                .ground_position = {0, 10}};
    AbWorldDefinition physics = {.gravity = {0, 9.8f},
                                 .broadphase_limits = {64, 128, 128},
                                 .maximum_translation = 2,
                                 .maximum_translation_squared = 4,
                                 .clear_forces = true,
                                 .continuous = true};
    AbClassicLevelLimits limits = {
        .objects = 4, .contacts = 16, .storage_bytes = 65536};
    AbClassicLevel *owner = NULL;
    AbError error = {0};
    AbClassicLevelAssets assets = {.level_path = "missing", .catalog_path = "missing"};
    CHECK(!ab_classic_level_load(&archive, &assets, physics, limits, true, &owner,
                                 &error));
    CHECK(
        !ab_classic_level_load(&archive, &assets, physics, limits, true, NULL, &error));
    CHECK(
        !ab_classic_level_load(&archive, NULL, physics, limits, true, &owner, &error));
    CHECK(!owner);
    CHECK(!ab_classic_level_open(&source, physics, limits, true, NULL, &error));
    CHECK(!ab_classic_level_open(NULL, physics, limits, true, &owner, &error));
    CHECK(!owner && !ab_classic_level_view(NULL).world);
    CHECK(!ab_classic_level_open(&source, physics,
                                 (AbClassicLevelLimits){.objects = SIZE_MAX,
                                                        .contacts = 16,
                                                        .storage_bytes = SIZE_MAX},
                                 true, &owner, &error));
    CHECK(!owner);
    CHECK(!ab_classic_level_open(&source, physics,
                                 (AbClassicLevelLimits){.objects = 4,
                                                        .contacts = SIZE_MAX,
                                                        .storage_bytes = 65536},
                                 true, &owner, &error));
    CHECK(!owner);
    CHECK(!ab_classic_level_open(
        &source, physics,
        (AbClassicLevelLimits){.objects = 4, .contacts = 16, .storage_bytes = 1}, true,
        &owner, &error));
    CHECK(!owner);
    CHECK(!ab_classic_level_open(
        &source, physics,
        (AbClassicLevelLimits){.objects = 4, .contacts = 16, .storage_bytes = 2048},
        true, &owner, &error));
    CHECK(!owner);
    CHECK(!ab_classic_level_open(
        &source, physics,
        (AbClassicLevelLimits){.objects = 1, .contacts = 16, .storage_bytes = 65536},
        true, &owner, &error));
    CHECK(!owner);
    layout.hash_capacity = SIZE_MAX;
    CHECK(!ab_classic_level_open(&source, physics, limits, true, &owner, &error));
    CHECK(!owner);
    layout.hash_capacity = 1;
    physics.gravity.x = NAN;
    CHECK(!ab_classic_level_open(&source, physics, limits, true, &owner, &error));
    CHECK(!owner);
    physics.gravity.x = 0;
    properties[5].value = number(3.5f);
    CHECK(!ab_classic_level_open(&source, physics, limits, true, &owner, &error));
    CHECK(!owner);
    properties[5].value = number(3);
    globals_entries[1].value = table(&globals);
    CHECK(!ab_classic_level_open(&source, physics, limits, true, &owner, &error));
    CHECK(!owner);
    globals_entries[1].value = table(&empty);
    CHECK(level_joint_owners(&source, physics));
    CHECK(world_cursor_checks(&source, physics));
    CHECK(tail_world_checks(&source, physics));
    float queued_mass = 0;
    for (size_t pass = 0; pass < 2; ++pass) {
        CHECK(
            ab_classic_level_open(&source, physics, limits, pass == 0, &owner, &error));
        AbClassicLevelView view = ab_classic_level_view(owner);
        CHECK(view.bodies->count == 2 && view.frame->count == 2 &&
              view.world->body_count == 2 && view.world->fixture_count == 2 &&
              view.order_hash_capacity == 2);
        CHECK(view.bird_count == 1 && view.bird_hash_capacity == 1 &&
              view.bird_order[0] == 0 &&
              view.bodies->objects[0].start_number.kind == AB_DATA_NUMBER &&
              view.bodies->objects[0].start_number.bits == number(1).bits &&
              view.bodies->objects[1].start_number.kind == AB_DATA_NIL);
        CHECK(view.goal_count == 0 && view.goal_hash_capacity == 0 &&
              view.character_objects && !view.character_objects[0].animation_enabled);
        for (size_t index = 0; index < 2; ++index) {
            const AbLevelObject *object = view.bodies->objects + index;
            CHECK(view.world->storage.fixtures[object->physics.fixture].user_data ==
                  (uintptr_t)object);
            CHECK(view.frame->objects[index].body == object->physics.body &&
                  view.frame->objects[index].history[0].position.x ==
                      view.frame->objects[index].history[1].position.x);
        }
        AbWorldSolveBody *bird = view.world->storage.bodies;
        CHECK(view.bodies->objects[0].initial_mass > 0 &&
              view.bodies->objects[1].initial_mass == 0);
        AbDataTable *physical = &view.properties->objects[0].physical;
        const AbDataValue *script_mass = ab_data_get(physical, string("mass"));
        const AbDataValue *start = ab_data_get(physical, string("startNumber"));
        const AbDataValue *force = ab_data_get(physical, string("forceY"));
        CHECK(script_mass && script_mass->kind == AB_DATA_NUMBER &&
              script_mass->bits == number(view.bodies->objects[0].initial_mass).bits &&
              start && start->kind == AB_DATA_NUMBER && start->bits == number(1).bits &&
              force && force->kind == AB_DATA_NUMBER && force->bits == number(2).bits &&
              !ab_data_get(physical, string("strength")));
        const AbDataTable *ground = &view.properties->objects[1].physical;
        const AbDataValue *strength = ab_data_get(ground, string("strength"));
        const AbDataValue *defence = ab_data_get(ground, string("defence"));
        CHECK(strength && strength->bits == number(30).bits && defence &&
              defence->bits == number(1000000).bits &&
              !ab_data_get(ground, string("controllable")));
        if (pass == 0) {
            queued_mass = view.bodies->objects[0].initial_mass;
            CHECK(bird->motion.mass.mass == 0);
        } else {
            CHECK(view.bodies->objects[0].initial_mass == queued_mass &&
                  bird->motion.mass.mass == queued_mass);
        }
        CHECK(bird->motion.mass.type == (pass == 0 ? AB_BODY_STATIC : AB_BODY_DYNAMIC));
        CHECK(bird->motion.mass.angle == (pass == 0 ? 0 : 0.5f));
        CHECK(view.frame->objects[0].rendered.angle == bird->motion.mass.angle);
        CHECK(view.character_objects[0].jump.rotation == bird->motion.mass.angle &&
              view.character_objects[0].jump.position.x == 1 &&
              view.character_objects[0].jump.position.y == 2);
        AbClassicForces *forces = NULL;
        AbClassicForceLimits force_limits = {.storage_bytes = 65536,
                                             .sound_choices = 8};
        CHECK(!ab_classic_forces_open(owner, force_limits, NULL, NULL, NULL, NULL,
                                      &error));
        CHECK(!ab_classic_forces_open(NULL, force_limits, NULL, NULL, NULL, &forces,
                                      &error) &&
              !forces);
        CHECK(!ab_classic_forces_open(
                  owner, (AbClassicForceLimits){.storage_bytes = 1, .sound_choices = 8},
                  NULL, NULL, NULL, &forces, &error) &&
              !forces);
        CHECK(!ab_classic_forces_view(NULL).objects);
        AbSpan bird_name = view.bodies->objects[0].name;
        AbSpan ground_name = view.bodies->objects[1].name;
        view.bodies->objects[1].name = bird_name;
        CHECK(!ab_classic_forces_open(owner, force_limits, NULL, NULL, NULL, &forces,
                                      &error) &&
              !forces);
        view.bodies->objects[0].name = (AbSpan){(const uint8_t *)"a\0z", 3};
        view.bodies->objects[1].name = (AbSpan){(const uint8_t *)"a", 1};
        CHECK(ab_classic_forces_open(owner, force_limits, NULL, NULL, NULL, &forces,
                                     &error));
        AbClassicForceView force_view = ab_classic_forces_view(forces);
        CHECK(force_view.count == 2 && force_view.source_indices[0] == 1 &&
              force_view.source_indices[1] == 0);
        ab_classic_forces_close(forces);
        view.bodies->objects[0].name = bird_name;
        view.bodies->objects[1].name = ground_name;
        CHECK(ab_classic_forces_open(owner, force_limits, NULL, NULL, NULL, &forces,
                                     &error));
        force_view = ab_classic_forces_view(forces);
        CHECK(force_view.source_indices[0] == 0 && force_view.source_indices[1] == 1 &&
              force_view.objects[0].prepare_forces &&
              !force_view.objects[1].prepare_forces &&
              force_view.objects[0].forces.object_mass == queued_mass &&
              force_view.objects[0].forces.force_x_present &&
              force_view.objects[0].forces.force.x == 0);
        AbGamePhysicsPrepare prepare = {.objects = force_view.objects,
                                        .count = force_view.count,
                                        .water_seconds = 0.05f};
        if (pass == 1) {
            view.bodies->objects[0].physics.gravity_enabled = false;
            CHECK(ab_game_physics_before_step(&prepare, view.world, 1.0f / 30, &error));
            CHECK(bird->motion.force.y ==
                  -(queued_mass * physics.gravity.y) + 2 * queued_mass);
            CHECK(ab_world_clear_forces(view.world, &error));
        }
        AbQueuedForce queue_entries[3];
        AbForceQueue queue = {queue_entries, 0, 3};
        AbQueuedForce queued = {bird_name, {3, 4}, {2, 5}};
        CHECK(ab_force_queue_append(&queue, queued, &error));
        queued.object = ground_name;
        CHECK(ab_force_queue_append(&queue, queued, &error));
        queued.object = string("missing").string;
        CHECK(ab_force_queue_append(&queue, queued, &error));
        CHECK(!ab_classic_forces_apply_queue(NULL, &queue, &error));
        CHECK(ab_classic_forces_apply_queue(forces, &queue, &error));
        CHECK(queue.count == 3 && bird->motion.force.x == (pass == 0 ? 0 : 3) &&
              bird->motion.force.y == (pass == 0 ? 0 : 4) &&
              bird->motion.torque == (pass == 0 ? 0 : -5));
        CHECK(ab_world_clear_forces(view.world, &error));
        ClassicQueueCheck queue_check = {forces, &queue, 0, 0};
        prepare.hook = classic_queue_hook;
        prepare.context = &queue_check;
        AbRandom48 random, expected;
        CHECK(ab_random48_seed(&random, 7));
        expected = random;
        uint32_t ignored;
        CHECK(ab_random48_next(&expected, &ignored) &&
              ab_random48_next(&expected, &ignored));
        CHECK(!ab_classic_level_initialize_objects(NULL, &random, true, &error));
        CHECK(!ab_classic_level_initialize_objects(owner, NULL, true, &error));
        view.frame->clock.time = 1;
        CHECK(!ab_classic_level_initialize_objects(owner, &random, true, &error));
        view.frame->clock.time = 0;
        CHECK(ab_classic_level_initialize_objects(owner, &random, pass == 0, &error));
        view = ab_classic_level_view(owner);
        CHECK(random.value == expected.value && view.timers_initialized &&
              view.timers[0].present == AB_STARTUP_BIRD_TIMERS &&
              view.timers[1].present == 0);
        CHECK(view.character_objects[0].animation_enabled &&
              view.character_objects[0].bird.timer == view.timers[0].animation &&
              view.character_objects[0].jump.timer == view.timers[0].jump);
        CHECK(!ab_classic_level_initialize_objects(owner, &random, true, &error) &&
              random.value == expected.value);
        AbFrameClockInput elapsed = {.seconds = 0.05f, .physics_enabled = true};
        AbGameFrameControls controls = {.velocity_iterations = 8,
                                        .position_iterations = 3,
                                        .maximum_steps = 8,
                                        .before_step = ab_game_physics_before_step,
                                        .after_steps = ab_game_physics_after_steps,
                                        .context = &prepare};
        AbFrameClockPlan plan;
        CHECK(ab_game_frame_advance(view.frame, view.world, &elapsed, &controls, &plan,
                                    &error));
        CHECK(plan.steps == 1 && view.frame->history_index == 1 &&
              view.frame->clock.time == elapsed.seconds && queue.count == 0 &&
              queue_check.applied == 1 && queue_check.cleared == 1);
        CHECK(pass == 0 ? bird->motion.mass.transform.position.y == 2
                        : bird->motion.mass.transform.position.y > 2);
        CHECK(ab_force_queue_append(&queue, queue_entries[0], &error));
        elapsed.seconds = 0;
        elapsed.physics_enabled = false;
        CHECK(ab_game_frame_advance(view.frame, view.world, &elapsed, &controls, &plan,
                                    &error) &&
              queue.count == 1 && queue_check.applied == 1 && queue_check.cleared == 1);
        elapsed.physics_enabled = true;
        CHECK(ab_game_frame_advance(view.frame, view.world, &elapsed, &controls, &plan,
                                    &error) &&
              plan.steps == 0 && queue.count == 0 && queue_check.applied == 1 &&
              queue_check.cleared == 2);
        CHECK(transform_checks(owner));
        CHECK(classic_contacts_checks(owner, pass == 0));
        CHECK(classic_property_lifetime_checks(owner));
        ab_classic_forces_close(forces);
        ab_classic_level_close(owner);
        owner = NULL;
    }
    source.derive_ground_position = true;
    source.ground_position = (AbVec2){NAN, NAN};
    CHECK(ab_classic_level_open(&source, physics, limits, true, &owner, &error));
    AbClassicLevelView derived = ab_classic_level_view(owner);
    const AbObjectBody *circle = &derived.bodies->objects[0].physics;
    const AbObjectBody *ground = &derived.bodies->objects[1].physics;
    CHECK(circle->dimensions.width == -1 && circle->dimensions.height == -1 &&
          circle->dimensions.radius == 0.5f);
    CHECK(derived.bodies->load_edges_present && derived.bodies->load_edges.x == 2 &&
          derived.bodies->load_edges.y == 0);
    AbVec2 position =
        derived.world->storage.bodies[ground->body].motion.mass.transform.position;
    CHECK(position.x == 1 && position.y == 5 && ground->dimensions.width == 2000 &&
          ground->dimensions.height == 10 && ground->dimensions.radius == -1);
    CHECK(dynamic_circle_checks(owner));
    ab_classic_level_close(owner);
    owner = NULL;
    CHECK(ab_classic_level_open(&source, physics, limits, true, &owner, &error));
    CHECK(dynamic_box_checks(owner));
    ab_classic_level_close(owner);
    ab_classic_level_close(NULL);
    ab_classic_forces_close(NULL);
    return true;
}

static bool placement_scale_checks(void) {
    AbDataTable empty = {0};
    AbDataEntry entry = {string("physicsToWorld"), number(20)};
    AbDataTable level = {&entry, 1, 1, NULL};
    float result = 17;
    CHECK(ab_level_placement_scale(&empty, 20, &result, NULL) && result == 5 &&
          ab_level_placement_scale(&level, 20, &result, NULL) && result == 1);
    const float values[] = {100, 0, -0.0f, -20, 1.3f};
    for (size_t index = 0; index < sizeof(values) / sizeof(*values); ++index) {
        entry.value = number(values[index]);
        CHECK(ab_level_placement_scale(&level, 20, &result, NULL) &&
              result == values[index] / 20 &&
              (result != 0 || signbit(result) == signbit(values[index])));
    }
    const AbDataValue invalid[] = {{.kind = AB_DATA_BOOLEAN},
                                   {.kind = AB_DATA_BOOLEAN, .bits = 1},
                                   string("20"),
                                   table(&empty),
                                   number(INFINITY),
                                   number(NAN)};
    for (size_t index = 0; index < sizeof(invalid) / sizeof(*invalid); ++index) {
        entry.value = invalid[index];
        result = 17;
        CHECK(!ab_level_placement_scale(&level, 20, &result, NULL) && result == 17);
    }
    CHECK(!ab_level_placement_scale(&empty, 0, &result, NULL) && result == 17 &&
          !ab_level_placement_scale(&empty, NAN, &result, NULL) && result == 17);
    return true;
}

int main(void) {
    if (!placement_scale_checks() || !bird_order_checks() || !owner_checks()) {
        return 1;
    }
    puts("Classic level ownership, partial cleanup, limits and frame stepping pass");
    return 0;
}
