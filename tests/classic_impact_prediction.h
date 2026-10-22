#ifndef AB_TEST_CLASSIC_IMPACT_PREDICTION_H
#define AB_TEST_CLASSIC_IMPACT_PREDICTION_H

#include "ab/camera_follow_world.h"
#include "ab/camera_system.h"
#include "ab/classic_camera.h"

typedef struct {
    size_t radii, queries, prints, printed;
    size_t sequences;
    bool reject_observation;
} ClassicPredictionTest;

static bool classic_prediction_radius(void *context, float *output, AbError *error) {
    (void)error;
    ClassicPredictionTest *test = context;
    ++test->radii;
    *output = 0.5f;
    return true;
}

static bool classic_prediction_print(void *context, size_t count, AbError *error) {
    (void)error;
    ClassicPredictionTest *test = context;
    ++test->prints;
    test->printed = count;
    return true;
}

static bool classic_prediction_observe(void *context, AbVec2 first, AbVec2 second,
                                       const AbLevelRaycast *result, AbError *error) {
    (void)first;
    (void)second;
    (void)result;
    ClassicPredictionTest *test = context;
    ++test->queries;
    return !test->reject_observation || ab_fail(error, "Stop prediction observation");
}

static bool classic_prediction_checks(AbClassicLevel *level) {
    ClassicPredictionTest test = {0};
    AbBirdImpactWorldHost host = {&test, 2048, classic_prediction_radius,
                                  classic_prediction_print, classic_prediction_observe};
    AbBirdImpactInput bird = {.position = {-1, 1}, .velocity = {4, 0}};
    AbBirdImpactPrediction result = {0};
    AbClassicLevelView view = ab_classic_level_view(level);
    view.world->locked = true;
    bool okay = ab_classic_level_predict_impact(level, &bird, &host, &result, NULL);
    view.world->locked = false;
    CHECK(okay && result.point_present && result.name.kind == AB_DATA_STRING &&
          result.raycasts == test.queries && test.prints == 1 &&
          test.printed == test.queries && test.radii * 3 == test.queries * 2);
    AbBirdImpactPrediction saved = result;
    test = (ClassicPredictionTest){.reject_observation = true};
    CHECK(!ab_classic_level_predict_impact(level, &bird, &host, &result, NULL) &&
          test.queries == 1 && !test.radii && !test.prints &&
          !memcmp(&result, &saved, sizeof(result)) && !view.world->failed);
    test = (ClassicPredictionTest){0};
    host.radius = NULL;
    CHECK(!ab_classic_level_predict_impact(level, &bird, &host, &result, NULL) &&
          test.queries == 1 && !test.prints &&
          !memcmp(&result, &saved, sizeof(result)));
    host.radius = classic_prediction_radius;
    host.maximum_raycasts = 1;
    test = (ClassicPredictionTest){0};
    CHECK(!ab_classic_level_predict_impact(level, &bird, &host, &result, NULL) &&
          test.queries == 1 && test.radii == 1 && !test.prints &&
          !memcmp(&result, &saved, sizeof(result)));
    CHECK(!ab_classic_level_predict_impact(NULL, &bird, &host, &result, NULL));
    test = (ClassicPredictionTest){0};
    AbClassicImpactServices services = {&test, 2048, classic_prediction_print,
                                        classic_prediction_observe};
    CHECK(ab_classic_level_predict_object_impact(level, 0, &bird, &services, &result,
                                                 NULL) &&
          result.point_present && result.raycasts == test.queries && !test.radii &&
          test.prints == 1);
    saved = result;
    AbSpan radius_key = {(const uint8_t *)"radius", 6};
    AbDataValue radius = view.properties->objects[0].physical_entries[4].value;
    CHECK(ab_level_property_set(
        view.properties, 0, radius_key,
        (AbDataValue){.kind = AB_DATA_TABLE, .table = &view.properties->registry},
        NULL));
    test = (ClassicPredictionTest){0};
    CHECK(!ab_classic_level_predict_object_impact(level, 0, &bird, &services, &result,
                                                  NULL) &&
          test.queries == 1 && !test.prints &&
          !memcmp(&result, &saved, sizeof(result)));
    CHECK(ab_level_property_set(view.properties, 0, radius_key, radius, NULL));
    return true;
}

typedef struct {
    AbSpan name;
    size_t calls;
} ClassicMissingProperty;

static bool classic_missing_property(void *context, AbSpan name, AbError *error) {
    (void)error;
    ClassicMissingProperty *missing = context;
    CHECK(name.size == missing->name.size &&
          !memcmp(name.data, missing->name.data, name.size));
    ++missing->calls;
    return true;
}

static bool classic_property_lifetime_checks(AbClassicLevel *level) {
    AbClassicLevelView view = ab_classic_level_view(level);
    AbDataValue before = {0}, after = {0};
    AbSpan key = {(const uint8_t *)"radius", 6};
    CHECK(ab_level_property_get(view.properties, 0, key, &before, NULL) &&
          before.kind == AB_DATA_NUMBER &&
          ab_classic_level_forget_object(level, 0, NULL) && view.removed_objects[0] &&
          ab_level_property_get(view.properties, 0, key, &after, NULL) &&
          after.kind == before.kind && after.bits == before.bits);
    ClassicMissingProperty missing = {view.bodies->objects[0].name, 0};
    view.properties->lookup.context = &missing;
    view.properties->lookup.missing = classic_missing_property;
    CHECK(ab_level_properties_forget(view.properties, 0, NULL) &&
          ab_level_properties_forget(view.properties, 0, NULL) &&
          ab_level_property_get(view.properties, 0, key, &after, NULL) &&
          after.kind == AB_DATA_NIL && missing.calls == 1);
    view.properties->lookup.context = NULL;
    view.properties->lookup.missing = NULL;
    return true;
}

typedef struct {
    const AbCameraSequence *shot, *end;
    size_t loads, logs;
} ClassicCameraResources;

static bool classic_camera_sequence_load(void *context, const char *name,
                                         const AbCameraSequence **output,
                                         AbError *error) {
    (void)error;
    ClassicCameraResources *resources = context;
    CHECK(!strcmp(name, "FollowShotBirdCameraSequence") ||
          !strcmp(name, "FollowShotBirdEndCameraSequence"));
    *output = !strcmp(name, "FollowShotBirdCameraSequence") ? resources->shot
                                                            : resources->end;
    ++resources->loads;
    return true;
}

static bool classic_camera_sequence_log(void *context, AbSpan message, AbError *error) {
    (void)error;
    ClassicCameraResources *resources = context;
    const char *expected = resources->logs % 2u
                               ? "<CameraSequenceManager> starting new camera sequence"
                               : "<CameraSequenceManager> loading sequence from file: "
                                 "FollowShotBirdCameraSequence";
    CHECK(message.size == strlen(expected) &&
          !memcmp(message.data, expected, message.size));
    ++resources->logs;
    return true;
}

static bool classic_owned_camera_checks(AbClassicLevel *level) {
    AbClassicLevelView view = ab_classic_level_view(level);
    AbCameraSnap snaps[] = {{{-200, 0}, 2}, {{500, 0}, 1}};
    AbDataEntry target = {
        {.kind = AB_DATA_STRING, .string = {(const uint8_t *)"target", 6}},
        {.kind = AB_DATA_STRING, .string = {(const uint8_t *)"flyingBird", 10}}};
    AbDataTable properties = {&target, 1, 1, NULL};
    AbCameraSequenceStage stage = {.action = {(const uint8_t *)"followTarget", 12},
                                   .properties = &properties,
                                   .interrupt_on_input = true};
    AbCameraSequence shot = {&stage, 1}, end = {0};
    ClassicCameraResources resources = {&shot, &end, 0, 0};
    ClassicPredictionTest test = {0};
    float gravity = 0;
    AbClassicCameraConfig config = {.level = level,
                                    .snaps = snaps,
                                    .snap_count = 2,
                                    .pixel_size = {320, 180},
                                    .absolute_bottom = 200,
                                    .horizontal_leeway = 200,
                                    .gravity = &gravity,
                                    .shot_sequence = &shot,
                                    .end_sequence = &end,
                                    .resource_context = &resources,
                                    .load_sequence = classic_camera_sequence_load,
                                    .sequence_log = classic_camera_sequence_log,
                                    .prediction = {&test, 2048,
                                                   classic_prediction_print,
                                                   classic_prediction_observe},
                                    .storage_bytes = 65536};
    AbClassicCamera *camera = NULL;
    CHECK(!ab_classic_camera_open(NULL, &camera, NULL) && !camera);
    size_t storage = config.storage_bytes;
    config.storage_bytes = 1;
    CHECK(!ab_classic_camera_open(&config, &camera, NULL) && !camera);
    config.storage_bytes = storage;
    config.sequence_log = NULL;
    CHECK(ab_classic_camera_open(&config, &camera, NULL) &&
          !ab_classic_camera_on_launch(camera, NULL) && resources.loads == 1 &&
          resources.logs == 0 &&
          !ab_camera_system_target_bird(ab_classic_camera_system(camera)));
    ab_classic_camera_close(camera);
    resources.loads = 0;
    config.sequence_log = classic_camera_sequence_log;
    CHECK(ab_classic_camera_open(&config, &camera, NULL));
    AbCameraSystem *system = ab_classic_camera_system(camera);
    CHECK(system && !ab_classic_camera_set_flying(camera, 2, NULL) &&
          ab_classic_camera_set_flying(camera, 0, NULL) &&
          ab_classic_camera_on_launch(camera, NULL));
    const AbCameraFollowBird *bird = ab_camera_system_target_bird(system);
    AbClassicCameraView camera_view = ab_classic_camera_view(camera);
    CHECK(camera_view.target_object == 0 && camera_view.flying_object == 0 &&
          camera_view.loaded_sequence && camera_view.loaded_sequence->count == 1 &&
          bird && !bird->launch_present &&
          ab_classic_camera_update(camera, 0.1f, &(AbCameraSystemInput){0}, NULL) &&
          !test.queries && !test.prints && !bird->launch_present);
    CHECK(
        ab_level_properties_publish_launch(view.properties, 0, (AbVec2){4, 0}, NULL) &&
        !ab_level_properties_publish_launch(view.properties, 0, (AbVec2){9, 10}, NULL));
    AbVec2 saved = view.character_objects[0].jump.position;
    view.character_objects[0].jump.position = (AbVec2){-1, 1};
    CHECK(ab_classic_camera_update(camera, 0.1f, &(AbCameraSystemInput){0}, NULL) &&
          test.queries && test.prints == 1 && !test.radii && bird->launch_present &&
          bird->launch_velocity.x == 4 && bird->launch_velocity.y == 0 &&
          bird->position.x == -1 && bird->position.y == 1);
    CHECK(ab_classic_camera_remove_bird(camera, 0, NULL) &&
          !ab_camera_system_target_bird(system) &&
          ab_classic_camera_set_flying(camera, SIZE_MAX, NULL));
    AbDataValue saved_key = view.properties->world_entries[0].key;
    view.properties->world_entries[0].key = target.value;
    CHECK(ab_classic_camera_on_launch(camera, NULL) &&
          ab_camera_system_target_bird(system) == bird);
    view.properties->world_entries[0].key = saved_key;
    CHECK(ab_classic_camera_blues_initialize(camera, NULL) &&
          !ab_camera_system_target_bird(system) && resources.loads == 3 &&
          resources.logs == 6);
    view.character_objects[0].jump.position = saved;
    ab_classic_camera_close(camera);
    AbDataValue launch = {0};
    CHECK(ab_level_property_get(view.properties, 0,
                                (AbSpan){(const uint8_t *)"launchVelocity", 14},
                                &launch, NULL) &&
          launch.kind == AB_DATA_TABLE &&
          launch.table == &view.properties->objects[0].launch_velocity &&
          launch.table->entries[0].value.bits == UINT32_C(0x40800000) &&
          launch.table->entries[1].value.bits == 0);
    ab_classic_camera_close(NULL);
    return true;
}

static bool classic_camera_world_start(void *context, const char *name,
                                       AbError *error) {
    (void)error;
    ClassicPredictionTest *test = context;
    CHECK(!strcmp(name, "FollowShotBirdEndCameraSequence"));
    ++test->sequences;
    return true;
}

static bool classic_camera_world_checks(AbClassicLevel *level) {
    AbClassicLevelView view = ab_classic_level_view(level);
    AbBirdImpactWorld world = {view.world, view.bodies, view.removed_objects};
    ClassicPredictionTest test = {0};
    AbBirdImpactWorldHost prediction = {&test, 2048, classic_prediction_radius,
                                        classic_prediction_print,
                                        classic_prediction_observe};
    float gravity = 0;
    AbCameraFollowWorldConfig config = {&world, &prediction, &gravity, &test,
                                        classic_camera_world_start};
    AbCameraFollowWorld binding = {0};
    CHECK(ab_camera_follow_world_initialize(&binding, &config, NULL));
    AbCameraFollowWorld saved = binding;
    CHECK(!ab_camera_follow_world_initialize(&binding, NULL, NULL) &&
          !memcmp(&binding, &saved, sizeof(binding)));
    AbCameraFollowBird bird = {
        .position = {-1, 1}, .launch_velocity = {4, 0}, .launch_present = true};
    AbCameraFollowCall call = {.operation = AB_CAMERA_FOLLOW_PREDICT, .bird = &bird};
    CHECK(binding.host.query(binding.host.context, &call, NULL) &&
          call.prediction.point_present && call.prediction.raycasts == test.queries &&
          test.prints == 1 && test.printed == test.queries);
    call =
        (AbCameraFollowCall){.operation = AB_CAMERA_FOLLOW_TRANSFORM, .point = {1, 2}};
    CHECK(binding.host.query(binding.host.context, &call, NULL) &&
          call.vector.x == 20 && call.vector.y == 40);
    call.operation = AB_CAMERA_FOLLOW_SEQUENCE;
    CHECK(binding.host.query(binding.host.context, &call, NULL) && test.sequences == 1);
    call.operation = AB_CAMERA_FOLLOW_ZOOM;
    CHECK(!binding.host.query(binding.host.context, &call, NULL) &&
          !binding.host.query(binding.host.context, NULL, NULL));
    AbCameraSnap snaps[] = {{{-200, 0}, 2}, {{500, 0}, 1}};
    AbCameraSystem *system = NULL;
    CHECK(ab_camera_system_open(snaps, 2, (AbVec2){320, 180}, 200, 200, SIZE_MAX,
                                &system, NULL));
    test = (ClassicPredictionTest){0};
    CHECK(ab_camera_system_follow_bird(system, &bird, NULL, NULL, &binding.host, NULL));
    view.world->locked = true;
    bool okay = ab_camera_system_update(system, 0.1f, &(AbCameraSystemInput){0}, NULL);
    view.world->locked = false;
    AbCameraSystemView camera = ab_camera_system_view(system);
    CHECK(okay && camera.follow && camera.follow->hit_present && camera.viewport &&
          test.queries && test.prints == 1);
    CHECK(ab_camera_system_update(system, 0.1f,
                                  &(AbCameraSystemInput){.user_input = true}, NULL) &&
          !ab_camera_system_target_bird(system));
    ab_camera_system_close(system);
    return true;
}

#endif
