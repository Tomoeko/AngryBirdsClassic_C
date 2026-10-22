#ifndef AB_TEST_CLASSIC_BLUES_SPLIT_H
#define AB_TEST_CLASSIC_BLUES_SPLIT_H

#include "ab/classic_bird_specialty.h"

typedef struct {
    PlayfieldTest *test;
    AbClassicPlayfieldView view;
    AbClassicBirdSpecialty *specialty;
    AbDataTable *parent_behavior;
    size_t parent, initial_count, initial_audio, events;
} BluesSplitCheck;

static bool blue_split_event(void *context, AbDataTable *event, AbError *error) {
    BluesSplitCheck *check = context;
    PlayfieldTest *test = check->test;
    AbClassicLevelView level = ab_classic_level_view(test->level);
    size_t third = check->initial_count + 2;
    AbClassicBirdView birds = ab_classic_birds_view(test->birds);
    CHECK(!check->events &&
          bc_named(field(event, "id"), "EID_BIRD_SPECIALTY_TRIGGERED") &&
          level.bodies->count == check->initial_count + 3 &&
          !level.removed_objects[check->parent] &&
          test->flying.table == &level.properties->objects[third].physical &&
          birds.other_birds->count == 2 &&
          field(check->parent_behavior, "split").kind == AB_DATA_NIL &&
          ab_classic_trajectories_view(check->view.trajectories).tracks == 4 &&
          ab_audio_mixer_view(test->mixer).pending_count == check->initial_audio + 1 &&
          !ab_classic_bird_specialty_trigger_flying(check->specialty, 0, NULL));
    uintptr_t hat = 0;
    bool present = true;
    CHECK(ab_string_map_get(test->blue_hats, level.bodies->objects[check->parent].name,
                            &hat, &present, error) &&
          !present &&
          ab_string_map_get(test->blue_hats, level.bodies->objects[third].name, &hat,
                            &present, error) &&
          present && hat);
    ++check->events;
    return true;
}

/* Prepared launch/settings/hat state; the actual enclosing specialty now owns
 * every subsequent branch/common event/behavior/removal effect. No native
 * whole-branch parity or fresh bootstrap is claimed by this C composition. */
static bool check_blue_split(PlayfieldTest *test, AbClassicPlayfieldView view,
                             size_t parent, AbError *error) {
    AbClassicLevelView level = ab_classic_level_view(test->level);
    size_t count = level.bodies->count;
    AbDataTable *physical = &level.properties->objects[parent].physical;
    AbDataValue group = field(&level.properties->objects[parent].behaviors,
                              "MuseumBluesTrajectoryRenderer");
    CHECK(group.kind == AB_DATA_TABLE && group.table->count == 1);
    AbDataTable *parent_behavior = group.table->entries[0].value.table;
    AbDataValue definition;
    CHECK(ab_object_definition_lookup(&level.properties->lookup,
                                      level.bodies->objects[parent].name, &definition,
                                      error) &&
          definition.kind == AB_DATA_TABLE);
    AbDataValue sprite = field(definition.table, "sprite");
    AbSpan tutorial_path[] = {span("tutorials"), sprite.string, span("showHelp")};
    CHECK(sprite.kind == AB_DATA_STRING &&
          ab_settings_statistics_write(
              test->statistics, tutorial_path, 3,
              (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1}, error));
    AbDataValue tutorials =
        field(ab_settings_statistics_root(test->statistics), "tutorials");
    const AbDataValue *tutorial = ab_data_get(tutorials.table, sprite);
    CHECK(tutorial && tutorial->kind == AB_DATA_TABLE);
    AbBirdCollisionHat hat = {.object_name = level.bodies->objects[parent].name};
    CHECK(ab_string_map_set(test->blue_hats, level.bodies->objects[parent].name,
                            (uintptr_t)&hat, true, error));
    AbVec2 position = level.character_objects[parent].jump.position;
    AbVec2 velocity = {3, 4};
    AbBodyMotion *body =
        &level.world->storage.bodies[level.bodies->objects[parent].physics.body].motion;
    AbObjectPropertyPlan publication;
    CHECK(ab_world_set_body_type(level.world,
                                 level.bodies->objects[parent].physics.body,
                                 AB_BODY_DYNAMIC, error) &&
          ab_body_velocity_set(body, velocity) &&
          ab_classic_level_property_plan(test->level, parent, -100000, 100000,
                                         &publication, error) &&
          publication.publish &&
          ab_classic_level_publish_properties(test->level, parent, &publication.values,
                                              error) &&
          ab_level_property_set(level.properties, parent, span("shot"),
                                (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1},
                                error));
    position = (AbVec2){from_bits(field(physical, "x").bits),
                        from_bits(field(physical, "y").bits)};
    CHECK(field(physical, "xVel").bits == bits(velocity.x) &&
          field(physical, "yVel").bits == bits(velocity.y));
    test->flying = (AbDataValue){.kind = AB_DATA_TABLE, .table = physical};
    test->globals.specialty_available =
        (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1};
    CHECK(ab_classic_birds_sync(test->birds, error));
    AbClassicBluesSplit split;
    CHECK(ab_classic_blues_split_initialize(
        (AbClassicBluesSplitConfig){
            .playfield = view,
            .birds = test->birds,
            .physical = test->physical,
            .settings = test->statistics,
            .audio = test->audio,
            .hats = test->blue_hats,
            .teleporters = test->teleporters,
            .flying = &test->flying,
            .names = {test->split_names, sizeof(test->split_names)}},
        &split, error));
    AbUiFrameState notification_state = {0};
    AbUiFrameFields notification_fields = {
        .name = {.kind = AB_UI_POINTER_TEXT, .text = span("notifications")}};
    AbUiInitBoundaries boundaries = {0};
    CHECK(ab_ui_frame_init(&notification_state, &notification_fields, &boundaries,
                           error));
    AbUiFrameNode notifications = {.state = &notification_state};
    AbClassicBirdSpecialty *specialty = NULL;
    CHECK(ab_classic_bird_specialty_open(
        (AbClassicBirdSpecialtyConfig){.shots = view.shots,
                                       .behaviors = view.behaviors,
                                       .audio = test->audio,
                                       .events = test->events,
                                       .notifications = &notifications,
                                       .flying = &test->flying,
                                       .red_available = &test->red_special,
                                       .notifications_capacity = 1,
                                       .storage_bytes = 16384,
                                       .settings = test->statistics,
                                       .blues = &split},
        &specialty, error));
    size_t initial_audio = ab_audio_mixer_view(test->mixer).pending_count;
    size_t native_birds =
        ab_classic_object_removal_registry(test->physical)->bird_count;
    BluesSplitCheck check = {test,   view,  specialty,     parent_behavior,
                             parent, count, initial_audio, 0};
    AbEventListener listener = {span("BluesSplitCheck"), &check, blue_split_event};
    AbDataValue event_id = {.kind = AB_DATA_STRING,
                            .string = span("EID_BIRD_SPECIALTY_TRIGGERED")};
    CHECK(ab_event_manager_add(test->events, event_id, &listener, error) &&
          ab_classic_bird_specialty_trigger_flying(specialty, 0, error) &&
          check.events == 1 && level.removed_objects[parent] &&
          bc_named(test->globals.bird_specialty, "CLUSTER_BOMB") &&
          !test->globals.specialty_available.bits &&
          field(physical, "specialtyTriggered").bits &&
          field(parent_behavior, "split").bits &&
          field(tutorial->table, "showHelp").kind == AB_DATA_BOOLEAN &&
          !field(tutorial->table, "showHelp").bits &&
          ab_classic_trajectories_view(view.trajectories).tracks == 3 &&
          ab_classic_trajectories_view(view.trajectories).points == 1 &&
          ab_audio_mixer_view(test->mixer).pending_count == initial_audio + 2 &&
          ab_classic_object_removal_registry(test->physical)->bird_count ==
              native_birds + 2);
    const AbVec2 offsets[] = {{-0.8f, 0.6f}, {0, 0}, {0.8f, -0.6f}};
    for (size_t index = 0; index < 3; ++index) {
        AbLevelPropertyObject *child = level.properties->objects + count + index;
        AbDataValue child_group =
            field(&child->behaviors, "MuseumBluesTrajectoryRenderer");
        CHECK(child_group.kind == AB_DATA_TABLE && child_group.table->count == 1);
        AbDataValue cluster =
            field(child_group.table->entries[0].value.table, "isCluster");
        CHECK(cluster.kind == AB_DATA_BOOLEAN && !cluster.bits);
        const uint32_t actual[] = {
            field(&child->physical, "x").bits, field(&child->physical, "y").bits,
            field(&child->physical, "xVel").bits, field(&child->physical, "yVel").bits};
        const uint32_t expected[] = {bits(position.x + offsets[index].x),
                                     bits(position.y + offsets[index].y),
                                     bits(velocity.x + offsets[index].x * 7.0f),
                                     bits(velocity.y + offsets[index].y * 7.0f)};
        for (size_t component = 0; component < 4; ++component) {
            if (actual[component] != expected[component])
                fprintf(stderr,
                        "Split child %zu field %zu: %08" PRIx32 " != %08" PRIx32 "\n",
                        index, component, actual[component], expected[component]);
            CHECK(actual[component] == expected[component]);
        }
        CHECK(ab_classic_post_physics_register_circle(test->blue_post, count + index,
                                                      error));
    }
    AbDataValue third_group = field(&level.properties->objects[count + 2].behaviors,
                                    "MuseumBluesTrajectoryRenderer");
    CHECK(test->flying.table == &level.properties->objects[count + 2].physical &&
          ab_classic_shots_state(view.shots)->flying ==
              ab_classic_shots_bird(view.shots, count + 2) &&
          ab_classic_camera_view(view.camera).flying_object == count + 2 &&
          field(third_group.table->entries[0].value.table, "originalCluster").table ==
              physical &&
          ab_classic_render_scene_sync(view.scene, error) &&
          ab_classic_scene_sync_visuals(ab_classic_render_scene_objects(view.scene),
                                        view.visuals, error) &&
          ab_event_manager_remove(test->events, event_id, &listener, error) &&
          ab_string_map_set(test->blue_hats, level.bodies->objects[count + 2].name, 0,
                            false, error));
    ab_classic_bird_specialty_close(specialty);
    /* An actual present teleporter reaches the explicit unfinished native
     * clone boundary. Preserve preceding children/puff/hat effects and retain
     * the prior published flying/otherBirds identities. Discard afterward. */
    size_t before_failure = level.bodies->count;
    size_t audio_before_failure = ab_audio_mixer_view(test->mixer).pending_count;
    AbDataTable *other_before_failure = ab_classic_birds_view(test->birds).other_birds;
    AbDataValue flying_before_failure = test->flying;
    AbGameTeleporter teleporter = {.kind = AB_GAME_TELEPORTER_SIMPLE};
    CHECK(ab_string_map_set(test->teleporters, level.bodies->objects[count].name,
                            (uintptr_t)&teleporter, true, error) &&
          ab_string_map_set(test->blue_hats, level.bodies->objects[count].name,
                            (uintptr_t)&hat, true, error));
    AbClassicBluesSplitResult rejected = {.objects = {SIZE_MAX, SIZE_MAX, SIZE_MAX}};
    AbError missing = {0};
    CHECK(!ab_classic_blues_split_create(&split, count, &rejected, &missing) &&
          strstr(missing.message, "Active Blues teleportation") &&
          rejected.objects[0] == SIZE_MAX && rejected.objects[1] == SIZE_MAX &&
          rejected.objects[2] == SIZE_MAX &&
          level.bodies->count == before_failure + 3 && !level.removed_objects[count] &&
          test->flying.table == flying_before_failure.table &&
          ab_classic_birds_view(test->birds).other_birds == other_before_failure &&
          ab_classic_trajectories_view(view.trajectories).tracks == 6 &&
          ab_classic_trajectories_view(view.trajectories).points == 2 &&
          ab_audio_mixer_view(test->mixer).pending_count == audio_before_failure &&
          !ab_classic_blues_split_create(&split, count, &rejected, NULL) &&
          level.bodies->count == before_failure + 3);
    uintptr_t transferred = 0;
    bool present = false;
    CHECK(ab_string_map_get(test->blue_hats,
                            level.bodies->objects[before_failure + 2].name,
                            &transferred, &present, error) &&
          present && transferred == (uintptr_t)&hat &&
          ab_string_map_set(test->blue_hats,
                            level.bodies->objects[before_failure + 2].name, 0, false,
                            error) &&
          ab_string_map_set(test->teleporters, level.bodies->objects[count].name, 0,
                            false, error));
    printf("Actual Blues specialty: split, hat, tutorial, camera, audio, ordered "
           "event and parent removal integration pass\n");
    return true;
}

#endif
