#ifndef AB_TEST_CLASSIC_BLUES_CHILD_H
#define AB_TEST_CLASSIC_BLUES_CHILD_H

#include "ab/classic_blues_child.h"
#include "../tools/oracle/reference_blues_child.h"

typedef struct {
    PlayfieldTest *test;
    AbDataTable *birds;
    size_t object, count;
    uint32_t events[BC_MAX_EVENTS][BC_EVENT_WORDS];
    char name[32];
    AbSpan parent_name;
    bool okay;
} BluesChildCheck;

static bool bc_named(AbDataValue value, const char *name) {
    return value.kind == AB_DATA_STRING && value.string.size == strlen(name) &&
           (!value.string.size || !memcmp(value.string.data, name, value.string.size));
}

static bool bc_encode_c(BluesChildCheck *check, AbDataValue value, BcValue *output) {
    if (value.kind == AB_DATA_NIL)
        *output = (BcValue){0};
    else if (value.kind == AB_DATA_BOOLEAN)
        *output = (BcValue){1, value.bits != 0};
    else if (value.kind == AB_DATA_NUMBER)
        *output = (BcValue){3, value.bits};
    else if (value.kind == AB_DATA_STRING) {
        if (check->parent_name.size && value.string.size == check->parent_name.size &&
            !memcmp(value.string.data, check->parent_name.data, value.string.size)) {
            *output = (BcValue){4, 1};
            return true;
        }
        if (bc_named(value, check->name)) {
            *output = (BcValue){4, 6};
            return true;
        }
        for (uint32_t role = 0; role < 9; ++role) {
            if (role != 6 && bc_named(value, bc_strings[role])) {
                *output = (BcValue){4, role};
                return true;
            }
        }
        return false;
    } else if (value.kind == AB_DATA_TABLE) {
        AbClassicLevelView level = ab_classic_level_view(check->test->level);
        if (value.table == &check->test->blue_damage_factors)
            *output = (BcValue){5, 1};
        else if (check->object < level.bodies->count &&
                 value.table == &level.properties->objects[check->object].physical)
            *output = (BcValue){5, 2};
        else if (value.table && value.table->count == 1 &&
                 bc_named(value.table->entries[0].key,
                          "MuseumBluesTrajectoryRenderer") &&
                 value.table->entries[0].value.kind == AB_DATA_TABLE &&
                 value.table->entries[0].value.table &&
                 !value.table->entries[0].value.table->count)
            *output = (BcValue){5, 3};
        else
            return false;
    } else
        return false;
    return true;
}

static AbDataValue bc_value_c(PlayfieldTest *test, BcValue value) {
    if (!value.kind)
        return (AbDataValue){0};
    if (value.kind == 1)
        return (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = value.bits};
    if (value.kind == 3)
        return (AbDataValue){.kind = AB_DATA_NUMBER, .bits = value.bits};
    if (value.kind == 4)
        return (AbDataValue){.kind = AB_DATA_STRING,
                             .string = span(bc_strings[value.bits])};
    return (AbDataValue){.kind = AB_DATA_TABLE, .table = &test->blue_damage_factors};
}

static bool bc_snapshot_c(BluesChildCheck *check, uint32_t output[BC_SNAPSHOT_WORDS]) {
    memset(output, 0, BC_SNAPSHOT_WORDS * sizeof(*output));
    AbClassicLevelView level = ab_classic_level_view(check->test->level);
    AbDataValue resolved = field(&level.properties->world, check->name);
    if (resolved.kind == AB_DATA_NIL)
        return true;
    CHECK(resolved.kind == AB_DATA_TABLE && check->object < level.bodies->count &&
          resolved.table == &level.properties->objects[check->object].physical);
    AbLevelPropertyObject *object = level.properties->objects + check->object;
    output[0] = 1;
    for (size_t index = 0; index < BC_FIELD_COUNT; ++index) {
        BcValue value;
        CHECK(bc_encode_c(check, field(resolved.table, bc_fields[index]), &value));
        output[1 + index * 2] = value.kind;
        output[2 + index * 2] = value.bits;
    }
    size_t start = 1 + BC_FIELD_COUNT * 2;
    const AbGameFrameObject *frame = level.frame->objects + check->object;
    const AbBodyMassState *body = &level.world->storage.bodies[frame->body].motion.mass;
    const AbBodyPose poses[] = {frame->history[0], frame->history[1], frame->rendered};
    for (size_t index = 0; index < 3; ++index) {
        output[start + index * 3] = bits(poses[index].position.x);
        output[start + index * 3 + 1] = bits(poses[index].position.y);
        output[start + index * 3 + 2] = bits(poses[index].angle);
    }
    output[start + 9] = (uint32_t)body->type;
    output[start + 10] = bits(body->mass);
    output[start + 11] = bits(body->inverse_mass);
    output[start + 12] = bits(body->inertia);
    output[start + 13] = bits(body->inverse_inertia);
    output[start + 14] = bits(body->linear_velocity.x);
    output[start + 15] = bits(body->linear_velocity.y);
    output[start + 16] = frame->collider;
    start += BC_CORE_WORDS;
    AbDataValue bird = field(check->birds, check->name);
    output[start] = bird.kind == AB_DATA_TABLE && bird.table == resolved.table;
    AbDataValue script = field(resolved.table, "gameObjectScript");
    if (script.kind == AB_DATA_TABLE) {
        CHECK(script.table == &object->script);
        output[start + 1] = 1;
        output[start + 2] =
            field(script.table, "physicsObject").table == resolved.table;
        output[start + 3] = bc_named(field(script.table, "name"), check->name);
        AbDataValue groups = field(script.table, "behaviors");
        CHECK(groups.kind == AB_DATA_TABLE && groups.table == &object->behaviors);
        output[start + 4] = (uint32_t)groups.table->count;
        AbDataValue group = field(groups.table, "MuseumBluesTrajectoryRenderer");
        CHECK(group.kind == AB_DATA_TABLE && group.table->count == 1);
        output[start + 5] = (uint32_t)group.table->count;
        AbDataValue behavior = group.table->entries[0].value;
        CHECK(behavior.kind == AB_DATA_TABLE);
        const char *const flags[] = {"isCluster", "flying"};
        for (size_t flag = 0; flag < 2; ++flag) {
            BcValue value;
            CHECK(bc_encode_c(check, field(behavior.table, flags[flag]), &value));
            output[start + 6 + flag * 2] = value.kind;
            output[start + 7 + flag * 2] = value.bits;
        }
        output[start + 10] = field(behavior.table, "gameObject").table == script.table;
        output[start + 11] =
            field(behavior.table, "physicsObject").table == resolved.table;
    }
    return true;
}

static void bc_trace_c(void *context, AbClassicBluesChildStage stage,
                       const AbDataValue *arguments, size_t count) {
    BluesChildCheck *check = context;
    if (!check->okay || count > 10 || check->count == BC_MAX_EVENTS) {
        check->okay = false;
        return;
    }
    uint32_t *event = check->events[check->count++];
    memset(event, 0, BC_EVENT_WORDS * sizeof(*event));
    event[0] = (uint32_t)stage;
    event[1] = (uint32_t)count;
    for (size_t index = 0; index < count; ++index) {
        BcValue value;
        if (!bc_encode_c(check, arguments[index], &value)) {
            check->okay = false;
            return;
        }
        event[2 + index * 2] = value.kind;
        event[3 + index * 2] = value.bits;
    }
    check->okay = bc_snapshot_c(check, event + BC_ARGUMENT_WORDS);
}

static bool bc_compare_c(const uint32_t *actual, const uint32_t *expected, size_t count,
                         unsigned profile, unsigned child, const char *label,
                         size_t event) {
    for (size_t index = 0; index < count; ++index) {
        if (actual[index] != expected[index]) {
            fprintf(stderr, "Blues %u/%u %s%zu[%zu]: %08" PRIx32 " != %08" PRIx32 "\n",
                    profile, child, label, event, index, actual[index],
                    expected[index]);
            return false;
        }
    }
    return true;
}

static bool check_blues_child(PlayfieldTest *test, AbClassicPlayfieldView view,
                              size_t parent_index, FILE *reference, AbError *error) {
    char header[96];
    CHECK(fgets(header, sizeof(header), reference) &&
          !strcmp(header, "AB_ANDROID_BLUES_CHILD_V1 16 3\n"));
    AbClassicLevelView level = ab_classic_level_view(test->level);
    AbDataTable *parent = &level.properties->objects[parent_index].physical;
    AbDataValue saved[BC_PARENT_FIELDS];
    for (size_t index = 0; index < BC_PARENT_FIELDS; ++index)
        saved[index] = field(parent, bc_parent_fields[index]);
    AbClassicBirdView birds = ab_classic_birds_view(test->birds);
    CHECK(birds.birds && birds.birds_field->kind == AB_DATA_TABLE &&
          birds.birds_field->table == birds.birds);
    BluesChildCheck check = {.test = test, .birds = birds.birds, .okay = true};
    AbClassicBluesChild owner;
    CHECK(ab_classic_blues_child_initialize(
        (AbClassicBluesChildConfig){
            .playfield = view,
            .birds = birds.birds_field,
            .names = {test->blue_names, sizeof(test->blue_names)},
            .trace_context = &check,
            .trace = bc_trace_c},
        &owner, error));
    size_t initial_count = level.bodies->count;
    size_t initial_tracks = ab_classic_trajectories_view(view.trajectories).tracks;
    const AbLevelPropertyObject *stable = level.properties->objects;
    for (unsigned profile = 0; profile < BC_PROFILES; ++profile) {
        for (unsigned child = 0; child < BC_CHILDREN; ++child) {
            BcValue input[BC_PARENT_FIELDS];
            uint32_t offset[2], summary[7];
            char suffix[4];
            bc_case(profile, child, input, offset, suffix);
            snprintf(check.name, sizeof(check.name), "%s%s", bc_strings[1], suffix);
            check.object = level.bodies->count;
            check.count = 0;
            check.okay = true;
            AbObjectPropertyAccess access = {
                .physics_object = {.kind = AB_DATA_TABLE, .table = parent}};
            for (size_t index = 0; index < BC_PARENT_FIELDS; ++index)
                CHECK(ab_object_property_set(&access, span(bc_parent_fields[index]),
                                             bc_value_c(test, input[index]), error));
            size_t created = SIZE_MAX;
            AbSpan name = {0};
            CHECK(ab_classic_blues_child_create(
                      &owner, parent_index, span(suffix),
                      (AbVec2){from_bits(offset[0]), from_bits(offset[1])}, &created,
                      &name, error) &&
                  check.okay);
            CHECK(read_words(reference, summary, 7) && summary[0] == profile &&
                  summary[1] == child && summary[2] == offset[0] &&
                  summary[3] == offset[1] && summary[4] == check.count &&
                  summary[5] == 1 && summary[6] == 1 && created == check.object &&
                  name.size == strlen(check.name) &&
                  !memcmp(name.data, check.name, name.size));
            for (size_t event = 0; event < check.count; ++event) {
                uint32_t expected[BC_EVENT_WORDS];
                CHECK(read_words(reference, expected, BC_EVENT_WORDS) &&
                      bc_compare_c(check.events[event], expected, BC_EVENT_WORDS,
                                   profile, child, "event", event));
            }
            uint32_t final[BC_SNAPSHOT_WORDS], expected[BC_SNAPSHOT_WORDS];
            CHECK(bc_snapshot_c(&check, final) &&
                  read_words(reference, expected, BC_SNAPSHOT_WORDS) &&
                  bc_compare_c(final, expected, BC_SNAPSHOT_WORDS, profile, child,
                               "final", 0));
            size_t total = (size_t)profile * BC_CHILDREN + child + 1;
            CHECK(level.properties->objects == stable &&
                  ab_classic_trajectories_view(view.trajectories).tracks ==
                      initial_tracks + total);
            if (check.count == 8) {
                AbClassicSceneView scene =
                    ab_classic_scene_view(ab_classic_render_scene_objects(view.scene));
                CHECK(bits(scene.items[created].pose.scale.x) == check.events[7][5] &&
                      bits(scene.items[created].pose.scale.y) == check.events[7][7]);
            }
        }
    }
    AbObjectPropertyAccess access = {
        .physics_object = {.kind = AB_DATA_TABLE, .table = parent}};
    for (size_t index = 0; index < BC_PARENT_FIELDS; ++index)
        CHECK(ab_object_property_set(&access, span(bc_parent_fields[index]),
                                     saved[index], error));
    CHECK(level.bodies->count == initial_count + BC_PROFILES * BC_CHILDREN &&
          ab_classic_render_scene_sync(view.scene, error) &&
          ab_classic_scene_sync_visuals(ab_classic_render_scene_objects(view.scene),
                                        view.visuals, error));
    /* Separate C ownership integration after the captured source child calls.
     * Register targets before any later camera update; no source init effects. */
    AbGameObjectCoreRegistry *physical =
        ab_classic_object_removal_registry(test->physical);
    size_t native_birds = physical->bird_count;
    size_t physical_bytes = ab_classic_object_removal_bytes(test->physical);
    AbShotBird *stable_shot = ab_classic_shots_bird(view.shots, parent_index);
    const char *stable_name = stable_shot->name;
    AbShotState unchanged_state = *ab_classic_shots_state(view.shots);
    size_t shot_bytes = ab_classic_shots_bytes(view.shots);
    size_t bird_bytes = ab_classic_birds_bytes(test->birds);
    for (size_t index = initial_count; index < level.bodies->count; ++index) {
        native_birds += level.bodies->objects[index].physics.controllable;
        CHECK(!ab_classic_birds_register_object(test->birds, index, NULL) &&
              !ab_classic_birds_register_object(test->birds, index + 1, NULL) &&
              ab_classic_shots_register_circle(view.shots, index, error) &&
              ab_classic_camera_register_object(view.camera, index, error) &&
              ab_classic_post_physics_register_circle(test->blue_post, index, error) &&
              ab_classic_object_removal_register_circle(test->physical, index, error) &&
              ab_classic_birds_register_object(test->birds, index, error) &&
              !ab_classic_birds_register_object(test->birds, index, NULL) &&
              physical->bird_count == native_birds &&
              ab_classic_object_removal_bytes(test->physical) == physical_bytes);
        AbShotBird *bird = ab_classic_shots_bird(view.shots, index);
        AbSpan name = level.bodies->objects[index].name;
        CHECK(bird && bird->shot && !bird->has_collided && bird->has_script &&
              bird->mass == level.bodies->objects[index].initial_mass &&
              strlen(bird->name) == name.size &&
              !memcmp(bird->name, name.data, name.size) &&
              !strcmp(bird->sprite, "BIRD_BLUE") &&
              ab_classic_shots_bird(view.shots, parent_index) == stable_shot &&
              stable_shot->name == stable_name &&
              ab_classic_shots_bytes(view.shots) == shot_bytes &&
              ab_classic_birds_bytes(test->birds) == bird_bytes &&
              ab_classic_birds_view(test->birds).birds_field == birds.birds_field &&
              !memcmp(&unchanged_state, ab_classic_shots_state(view.shots),
                      sizeof(unchanged_state)));
    }
    CHECK(fgetc(reference) == '\n' && fgets(header, sizeof(header), reference) &&
          !strcmp(header, "AB Blues child reference complete\n") &&
          fgetc(reference) == EOF);
    printf("Actual Blues child construction: 48 native cases, ordered "
           "arguments/state/body/script snapshots pass\n");
    return true;
}

/* Controlled outer split fields, followed by real class/camera/audio/particle/
 * retirement owners. This does not execute the unported full outer branch. */
static bool check_blue_lifecycle(PlayfieldTest *test, AbClassicPlayfieldView view,
                                 size_t parent, AbError *error) {
    AbClassicLevelView level = ab_classic_level_view(test->level);
    /* Profile one has dynamic bodies and a truthy numeric controllable value. */
    size_t first = level.bodies->count - BC_PROFILES * BC_CHILDREN + BC_CHILDREN;
    AbClassicBirdView initial = ab_classic_birds_view(test->birds);
    AbDataValue names[2] = {
        {.kind = AB_DATA_STRING, .string = level.bodies->objects[first].name},
        {.kind = AB_DATA_STRING, .string = level.bodies->objects[first + 1].name}};
    CHECK(ab_classic_birds_replace_other(test->birds, names, 2, error));
    AbClassicBirdView split = ab_classic_birds_view(test->birds);
    CHECK(split.other_birds != initial.other_birds && !initial.other_birds->count &&
          split.other_birds->count == 2 &&
          split.other_birds_field == initial.other_birds_field &&
          split.other_birds_field->table == split.other_birds &&
          bc_named(split.other_birds->entries[0].value, "source_bluea01") &&
          bc_named(split.other_birds->entries[1].value, "source_blueb01"));
    test->flying = (AbDataValue){
        .kind = AB_DATA_TABLE, .table = &level.properties->objects[first + 2].physical};
    test->globals.specialty_available = (AbDataValue){.kind = AB_DATA_BOOLEAN};
    CHECK(ab_classic_birds_sync(test->birds, error) &&
          ab_classic_shots_state(view.shots)->flying ==
              ab_classic_shots_bird(view.shots, first + 2) &&
          ab_classic_camera_view(view.camera).flying_object == first + 2 &&
          ab_classic_behaviors_add(view.behaviors, first + 2,
                                   span("MuseumBluesCameraFollowBehavior"),
                                   (AbDataValue){0}, error));
    AbDataValue group = field(&level.properties->objects[first + 2].behaviors,
                              "MuseumBluesTrajectoryRenderer");
    CHECK(group.kind == AB_DATA_TABLE && group.table->count == 1);
    AbDataValue behavior = group.table->entries[0].value;
    AbObjectPropertyAccess access = {.physics_object = behavior};
    CHECK(ab_object_property_set(
        &access, span("originalCluster"),
        (AbDataValue){.kind = AB_DATA_TABLE,
                      .table = &level.properties->objects[parent].physical},
        error));
    size_t ground = 0;
    while (ground < level.bodies->count &&
           !bc_named((AbDataValue){.kind = AB_DATA_STRING,
                                   .string = level.bodies->objects[ground].name},
                     "ground"))
        ++ground;
    CHECK(ground < level.bodies->count);
    const size_t moving[] = {first, first + 1, first + BC_CHILDREN};
    for (size_t index = 0; index < 3; ++index) {
        AbVec2 position = level.character_objects[moving[index]].jump.position;
        position.x += 8;
        CHECK(
            ab_classic_shots_set_position(view.shots, moving[index], position, error));
    }
    CHECK(ab_classic_trajectories_update(view.trajectories, error) &&
          ab_classic_trajectories_view(view.trajectories).points == 3);
    size_t tracks = ab_classic_trajectories_view(view.trajectories).tracks;
    AbBlockCollisionNotice collision = {0};
    CHECK(ab_classic_behaviors_collision(view.behaviors, first + 2, ground, &collision,
                                         error) &&
          ab_classic_trajectories_view(view.trajectories).tracks == tracks - 1 &&
          ab_classic_trajectories_view(view.trajectories).points == 2 &&
          ab_classic_behaviors_collision(view.behaviors, first, ground, &collision,
                                         error) &&
          ab_classic_trajectories_view(view.trajectories).tracks == tracks - 2 &&
          ab_classic_trajectories_view(view.trajectories).points == 2);
    size_t native_birds =
        ab_classic_object_removal_registry(test->physical)->bird_count;
    CHECK(ab_classic_birds_remove(test->birds, first, (AbDataValue){0}, error) &&
          level.removed_objects[first] &&
          test->flying.table == &level.properties->objects[first + 2].physical);
    /* A zero-speed child with an expired timer follows the actual retirement
     * path. Cancel following first, as a source camera transition may do. */
    CHECK(ab_classic_camera_remove_bird(view.camera, first + 2, error));
    AbBodyMotion *motion =
        &level.world->storage.bodies[level.bodies->objects[first + 2].physics.body]
             .motion;
    AbObjectPropertyPlan publication;
    CHECK(ab_body_velocity_set(motion, (AbVec2){0}) &&
          ab_classic_level_property_plan(test->level, first + 2, -100000, 100000,
                                         &publication, error) &&
          publication.publish &&
          ab_classic_level_publish_properties(test->level, first + 2,
                                              &publication.values, error));
    access.physics_object = (AbDataValue){
        .kind = AB_DATA_TABLE, .table = &level.properties->objects[first + 2].physical};
    CHECK(ab_object_property_set(
        &access, span("isReadyForRemoveTimer"),
        (AbDataValue){.kind = AB_DATA_NUMBER, .bits = bits(-0.01f)}, error));
    bool alive = true;
    CHECK(ab_classic_birds_update(test->birds, first + 2, 0, &alive, error) && !alive &&
          level.removed_objects[first + 2] && test->flying.kind == AB_DATA_NIL &&
          !ab_classic_shots_state(view.shots)->flying &&
          ab_classic_camera_view(view.camera).flying_object == SIZE_MAX &&
          ab_classic_object_removal_registry(test->physical)->bird_count ==
              native_birds - 2 &&
          field(split.birds, "source_bluea01").kind == AB_DATA_NIL &&
          field(split.birds, "source_bluec01").kind == AB_DATA_NIL &&
          field(&level.properties->world, "source_bluea01").kind == AB_DATA_NIL &&
          field(&level.properties->world, "source_bluec01").kind == AB_DATA_NIL &&
          bc_named(split.other_birds->entries[0].value, "source_bluea01") &&
          bc_named(split.other_birds->entries[1].value, "source_blueb01"));
    AbDataTable *retained = split.other_birds;
    CHECK(ab_classic_birds_reset_other(test->birds, error) &&
          ab_classic_birds_view(test->birds).other_birds != retained &&
          bc_named(retained->entries[0].value, "source_bluea01"));
    AbDataValue invalid = {.kind = AB_DATA_NUMBER};
    AbDataTable *current = ab_classic_birds_view(test->birds).other_birds;
    CHECK(!ab_classic_birds_replace_other(test->birds, &invalid, 1, NULL) &&
          ab_classic_birds_view(test->birds).other_birds == current);
    size_t generations = 2 * level.bird_count + 2;
    size_t bytes = ab_classic_birds_bytes(test->birds);
    CHECK(!ab_classic_birds_replace_other(test->birds, names, 131, NULL) &&
          ab_classic_birds_view(test->birds).other_birds == current);
    /* The initial, split and reset tables consumed three generations. Repeated
     * splits must preserve every old alias through the explicit final bound. */
    for (size_t generation = 3; generation < generations; ++generation) {
        CHECK(ab_classic_birds_replace_other(test->birds, names, 2, error));
        AbDataTable *next = ab_classic_birds_view(test->birds).other_birds;
        CHECK(next != current && next != retained && next->count == 2 &&
              !initial.other_birds->count &&
              bc_named(retained->entries[0].value, "source_bluea01") &&
              ab_classic_birds_bytes(test->birds) == bytes);
        current = next;
    }
    CHECK(!ab_classic_birds_reset_other(test->birds, NULL) &&
          !ab_classic_birds_replace_other(test->birds, names, 2, NULL) &&
          ab_classic_birds_view(test->birds).other_birds == current &&
          current->count == 2 && ab_classic_birds_bytes(test->birds) == bytes);
    return true;
}

#endif
