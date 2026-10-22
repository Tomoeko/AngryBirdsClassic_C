#include "ab/camera_follow_world.h"
#include "ab/coordinates.h"

typedef struct {
    AbCameraFollowWorld *binding;
    const AbCameraFollowBird *bird;
    AbBirdImpactInput *input;
} FollowPrediction;

static bool prediction_radius(void *context, float *output, AbError *error) {
    FollowPrediction *prediction = context;
    AbBirdImpactWorldHost *host = prediction->binding->config.prediction;
    if (!host->radius) {
        return ab_fail(error, "Missing camera prediction radius service");
    }
    if (!host->radius(host->context, output, error)) {
        return false;
    }
    prediction->input->velocity = prediction->bird->launch_velocity;
    prediction->input->gravity = *prediction->binding->config.gravity;
    return true;
}

static bool prediction_print(void *context, size_t count, AbError *error) {
    FollowPrediction *prediction = context;
    AbBirdImpactWorldHost *host = prediction->binding->config.prediction;
    return host->print ? host->print(host->context, count, error)
                       : ab_fail(error, "Missing camera prediction print service");
}

static bool prediction_observe(void *context, AbVec2 first, AbVec2 second,
                               const AbLevelRaycast *result, AbError *error) {
    FollowPrediction *prediction = context;
    AbBirdImpactWorldHost *host = prediction->binding->config.prediction;
    return !host->observe || host->observe(host->context, first, second, result, error);
}

static bool predict(AbCameraFollowWorld *binding, AbCameraFollowCall *call,
                    AbError *error) {
    if (!call->bird || !call->bird->launch_present) {
        return ab_fail(error, "Missing launched bird for camera prediction");
    }
    AbBirdImpactInput input = {.position = call->bird->position,
                               .velocity = call->bird->launch_velocity,
                               .gravity = *binding->config.gravity};
    FollowPrediction prediction = {binding, call->bird, &input};
    AbBirdImpactWorldHost host = {
        &prediction, binding->config.prediction->maximum_raycasts, prediction_radius,
        prediction_print, prediction_observe};
    return ab_bird_impact_predict_world(&input, binding->config.world, &host,
                                        &call->prediction, error);
}

static bool query(void *context, AbCameraFollowCall *call, AbError *error) {
    AbCameraFollowWorld *binding = context;
    if (!call) {
        return ab_fail(error, "Missing camera world-service request");
    }
    bool okay;
    switch (call->operation) {
        case AB_CAMERA_FOLLOW_PREDICT:
            okay = predict(binding, call, error);
            break;
        case AB_CAMERA_FOLLOW_TRANSFORM:
            okay = ab_coordinate_point(NULL, AB_SPACE_PHYSICS, AB_SPACE_WORLD,
                                       call->point, &call->vector);
            if (!okay)
                return ab_fail(error, "Unsupported camera prediction coordinates");
            break;
        case AB_CAMERA_FOLLOW_SEQUENCE:
            if (!binding->config.start_sequence)
                return ab_fail(error, "Missing camera end-sequence service");
            okay = binding->config.start_sequence(
                binding->config.context, "FollowShotBirdEndCameraSequence", error);
            break;
        default:
            return ab_fail(error, "Camera world binding received an internal query");
    }
    binding->host.gravity = *binding->config.gravity;
    return okay;
}

bool ab_camera_follow_world_initialize(AbCameraFollowWorld *binding,
                                       const AbCameraFollowWorldConfig *config,
                                       AbError *error) {
    if (!binding || !config || !config->world || !config->prediction ||
        !config->gravity) {
        return ab_fail(error, "Invalid camera world-service binding");
    }
    AbCameraFollowWorldConfig selected = *config;
    *binding = (AbCameraFollowWorld){.host = {binding, *selected.gravity, query},
                                     .config = selected};
    return true;
}
