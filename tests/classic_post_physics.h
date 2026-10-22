#ifndef AB_TEST_CLASSIC_POST_PHYSICS_H
#define AB_TEST_CLASSIC_POST_PHYSICS_H

#include "ab/classic_post_physics.h"
#include "ab/object_definition.h"

static float post_number(uint32_t word) {
    float result;
    memcpy(&result, &word, sizeof(result));
    return result;
}

static bool post_words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, output + index) == 1 &&
              fgetc(file) == (index + 1 == count ? '\n' : ' '));
    }
    return true;
}

static bool post_script_words(AbClassicLevelView level, size_t index,
                              const uint32_t expected[7]) {
    const AbBirdJumpState *pose = &level.character_objects[index].jump;
    const AbClassicMotionProperties *motion = level.motion_properties + index;
    CHECK(motion->initialized && bits(pose->position.x) == expected[0] &&
          bits(pose->position.y) == expected[1] &&
          bits(motion->velocity.x) == expected[2] &&
          bits(motion->velocity.y) == expected[3] &&
          bits(pose->rotation) == expected[4] &&
          motion->sleeping == (expected[5] != 0) &&
          motion->outside_boundaries == (expected[6] != 0));
    return true;
}

#if !defined(AB_TEST_POST_NO_OWNER)
static bool post_owner_checks(AbClassicLevel *level, AbClassicPostPhysics **output) {
    AbClassicPostConfig config = {
        .minimum_x = -49, .maximum_x = 89, .storage_bytes = 65536};
    AbClassicPostPhysics *post = NULL;
    CHECK(!ab_classic_post_physics_open(NULL, config, &post, NULL) && !post &&
          !ab_classic_post_physics_open(level, config, NULL, NULL) &&
          !ab_classic_post_physics_update(NULL, false, NULL) &&
          ab_classic_post_physics_bytes(NULL) == 0 &&
          ab_classic_post_physics_view(NULL).count == 0 &&
          ab_classic_post_physics_open(level, config, &post, NULL));
    size_t bytes = ab_classic_post_physics_bytes(post);
    ab_classic_post_physics_close(post);
    post = NULL;
    config.storage_bytes = bytes - 1;
    CHECK(!ab_classic_post_physics_open(level, config, &post, NULL) && !post);
    config.storage_bytes = bytes;
    CHECK(ab_classic_post_physics_open(level, config, output, NULL));
    post = *output;
    AbClassicPostView view = ab_classic_post_physics_view(post);
    AbClassicLevelView loaded = ab_classic_level_view(level);
    CHECK(view.count == 23 && loaded.goal_count == 1 && loaded.bird_count == 3 &&
          !view.activity->initialized && view.ignored_definitions->kind == AB_DATA_NIL);
    AbBodyMotion saved[23];
    for (size_t index = 0; index < view.count; ++index) {
        CHECK(view.objects[index].pink_timer.kind == AB_DATA_NIL &&
              view.objects[index].bomb_timer.kind == AB_DATA_NIL &&
              view.objects[index].shoot_delay.kind == AB_DATA_NIL &&
              view.objects[index].in_bubble.kind == AB_DATA_NIL);
        AbBodyMotion *body =
            &loaded.world->storage.bodies[loaded.frame->objects[index].body].motion;
        saved[index] = *body;
        body->awake = false;
        body->mass.linear_velocity = (AbVec2){0};
        body->mass.angular_velocity = 0;
        CHECK(ab_classic_level_finish_object(level, index, NULL));
    }
    CHECK(ab_classic_post_physics_update(post, false, NULL) &&
          !view.activity->initialized);
    size_t pig = loaded.goal_order[0];
    AbBodyMotion *body =
        &loaded.world->storage.bodies[loaded.frame->objects[pig].body].motion;
    body->awake = true;
    body->mass.linear_velocity.x = nextafterf(3, INFINITY);
    AbObjectPropertyPlan plan;
    CHECK(ab_classic_level_property_plan(level, pig, -49, 89, &plan, NULL) &&
          plan.publish);
    AbBodyMotion before = *body;
    AbGameFrameObject frame = loaded.frame->objects[pig];
    CHECK(ab_classic_post_physics_update(post, true, NULL) &&
          view.activity->initialized && view.activity->has_awake &&
          view.activity->moving && view.activity->above_tolerance &&
          loaded.motion_properties[pig].previous_awake &&
          memcmp(&before, body, sizeof(before)) == 0 &&
          memcmp(&frame, loaded.frame->objects + pig, sizeof(frame)) == 0);
    uint32_t properties[] = {
        bits(plan.values.pose.position.x), bits(plan.values.pose.position.y),
        bits(plan.values.velocity.x),      bits(plan.values.velocity.y),
        bits(plan.values.pose.angle),      plan.values.sleeping,
        plan.values.outside_boundaries};
    CHECK(post_script_words(loaded, pig, properties));
    AbDataEntry ignored = {
        {.kind = AB_DATA_STRING, .string = loaded.bodies->objects[pig].definition_name},
        {.kind = AB_DATA_BOOLEAN, .bits = 0}};
    AbDataTable ignored_table = {&ignored, 1, 1, NULL};
    *view.ignored_definitions =
        (AbDataValue){.kind = AB_DATA_TABLE, .table = &ignored_table};
    CHECK(ab_classic_post_physics_update(post, true, NULL) && !view.activity->moving &&
          view.activity->has_awake && view.activity->above_tolerance);
    *view.ignored_definitions = (AbDataValue){0};
    body->awake = false;
    body->mass.linear_velocity.x = 0;
    CHECK(
        ab_classic_post_physics_update(post, true, NULL) && view.activity->has_awake &&
        !view.activity->moving && loaded.motion_properties[pig].sleeping &&
        !loaded.motion_properties[pig].previous_awake &&
        ab_classic_post_physics_update(post, true, NULL) && !view.activity->has_awake);
    size_t bird = loaded.bird_order[0];
    view.objects[bird].bomb_timer = (AbDataValue){.kind = AB_DATA_BOOLEAN};
    CHECK(ab_classic_post_physics_update(post, true, NULL) && view.activity->moving &&
          !view.activity->has_awake);
    view.objects[bird].bomb_timer = (AbDataValue){0};
    body->awake = true;
    view.objects[pig].in_bubble = (AbDataValue){.kind = AB_DATA_NUMBER, .bits = 1};
    CHECK(ab_classic_post_physics_update(post, true, NULL) && !view.activity->moving);
    view.objects[pig].in_bubble = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1};
    CHECK(ab_classic_post_physics_update(post, true, NULL) && view.activity->moving);
    view.objects[pig].in_bubble = (AbDataValue){0};
    for (size_t index = 0; index < view.count; ++index) {
        loaded.world->storage.bodies[loaded.frame->objects[index].body].motion =
            saved[index];
    }
    return true;
}

#endif

static bool post_native_checks(AbClassicLevel *level, AbClassicPostPhysics *post,
                               FILE *file) {
    char marker[96];
    CHECK(fgets(marker, sizeof(marker), file));
    bool removed = !strcmp(marker, "AB_CLASSIC_POST_REMOVED_V1 1 22\n");
    CHECK(removed || !strcmp(marker, "AB_CLASSIC_POST_NATIVE_V1 3 23\n"));
    AbClassicLevelView loaded = ab_classic_level_view(level);
    AbClassicPostView view = ab_classic_post_physics_view(post);
    CHECK(view.count == 23);
    if (removed) {
        char name[128];
        size_t count = 0;
        CHECK(fscanf(file, "%127s\n", name) == 1);
        for (size_t index = 0; index < view.count; ++index) {
            if (loaded.removed_objects[index]) {
                AbSpan key = loaded.bodies->objects[index].name;
                CHECK(key.size == strlen(name) && !memcmp(key.data, name, key.size));
                ++count;
            }
        }
        CHECK(count == 1 && loaded.world->body_count == 22);
    }
    size_t compared = 0;
    size_t frame_count = removed ? 1 : 3;
    for (size_t frame_index = 0; frame_index < frame_count; ++frame_index) {
        uint32_t bounds[2], expected[23][7], next_awake[23];
        AbBodyMotion bodies[23];
        AbGameFrameObject frames[23];
        CHECK(post_words(file, bounds, 2) && bounds[0] == UINT32_C(0xffffffcf) &&
              bounds[1] == 89);
        for (size_t ordered = 0; ordered < view.count; ++ordered) {
            size_t index = view.source_indices[ordered];
            if (loaded.removed_objects[index]) {
                continue;
            }
            char name[128], rolling[128];
            uint32_t input[18], rendered[3], before[7], after[7], awake[1];
            CHECK(fscanf(file, "%127s\n%127s\n", name, rolling) == 2 &&
                  post_words(file, input, 18) && post_words(file, rendered, 3) &&
                  post_words(file, before, 7) && post_words(file, after, 7) &&
                  post_words(file, awake, 1));
            const AbLevelObject *source = loaded.bodies->objects + index;
            CHECK(source->name.size == strlen(name) &&
                  memcmp(source->name.data, name, source->name.size) == 0 &&
                  loaded.frame->objects[index].collider == input[0] &&
                  (source->physics.dimensions.radius < 0 ? 1u : 0u) == input[1] &&
                  bits(source->physics.dimensions.radius) == input[13] &&
                  input[4] == 0 && input[5] == 0 && input[6] == 0 && input[8] == 0 &&
                  input[14] == 0);
            AbObjectDefinition definition;
            CHECK(ab_object_definition_open(loaded.catalog, source->definition_name,
                                            NULL, &definition, NULL));
            const AbDataValue *sound = ab_data_get(
                definition.material, (AbDataValue){.kind = AB_DATA_STRING,
                                                   .string = string("rollingSound")});
            CHECK(strcmp(rolling, "-") == 0
                      ? !sound || sound->kind == AB_DATA_NIL ||
                            (sound->kind == AB_DATA_BOOLEAN && !sound->bits) ||
                            (sound->kind == AB_DATA_STRING && !sound->string.size)
                      : sound && sound->kind == AB_DATA_STRING &&
                            sound->string.size == strlen(rolling) &&
                            memcmp(sound->string.data, rolling, sound->string.size) ==
                                0);
            AbBodyMotion *body =
                &loaded.world->storage.bodies[loaded.frame->objects[index].body].motion;
            body->awake = input[3] != 0;
            CHECK(ab_classic_level_finish_object(level, index, NULL));
            body->awake = input[2] != 0;
            body->mass.type = (AbBodyType)input[15];
            body->mass.linear_velocity =
                (AbVec2){post_number(input[9]), post_number(input[10])};
            body->mass.angular_velocity = post_number(input[11]);
            body->mass.mass = post_number(input[12]);
            body->mass.transform.sine = post_number(input[16]);
            body->mass.transform.cosine = post_number(input[17]);
            loaded.frame->objects[index].rendered =
                (AbBodyPose){{post_number(rendered[0]), post_number(rendered[1])},
                             post_number(rendered[2])};
            AbObjectProperties properties = {
                .pose = {{post_number(before[0]), post_number(before[1])},
                         post_number(before[4])},
                .velocity = {post_number(before[2]), post_number(before[3])},
                .sleeping = before[5] != 0,
                .outside_boundaries = before[6] != 0};
            CHECK(ab_classic_level_publish_properties(level, index, &properties, NULL));
            view.objects[index] = (AbClassicPostObject){0};
            bodies[index] = *body;
            frames[index] = loaded.frame->objects[index];
            memcpy(expected[index], after, sizeof(after));
            next_awake[index] = awake[0];
        }
        uint32_t activity[3];
        CHECK(post_words(file, activity, 3) &&
              ab_classic_post_physics_update(post, true, NULL) &&
              view.activity->initialized &&
              view.activity->has_awake == (activity[0] != 0) &&
              view.activity->moving == (activity[1] != 0) &&
              view.activity->above_tolerance == (activity[2] != 0));
        for (size_t index = 0; index < view.count; ++index) {
            if (loaded.removed_objects[index]) {
                continue;
            }
            CHECK(
                post_script_words(loaded, index, expected[index]) &&
                loaded.motion_properties[index].previous_awake ==
                    (next_awake[index] != 0) &&
                memcmp(bodies + index,
                       &loaded.world->storage.bodies[loaded.frame->objects[index].body]
                            .motion,
                       sizeof(*bodies)) == 0 &&
                memcmp(frames + index, loaded.frame->objects + index,
                       sizeof(*frames)) == 0);
            ++compared;
        }
    }
    CHECK(fgets(marker, sizeof(marker), file) &&
          strcmp(marker, "AB classic post-physics complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("Compared %zu native post-physics object visits\n", compared);
    return true;
}

#endif
