#include "camera_system_internal.h"
#include "ab/camera_selection.h"
#include "ab/camera_zoom_fit.h"

static bool follow_service(void *context, AbCameraFollowCall *call, AbError *error) {
    AbCameraSystem *system = context;
    AbCameraLimitContext limits = {.pixel_size = system->pixel_size,
                                   .current_position = system->screen_position,
                                   .current_zoom = system->mover.current_zoom};
    bool okay = false;
    switch (call->operation) {
        case AB_CAMERA_FOLLOW_PREDICT:
        case AB_CAMERA_FOLLOW_TRANSFORM:
        case AB_CAMERA_FOLLOW_SEQUENCE: {
            AbCameraFollowHost *environment = system->follow_environment;
            if (!environment || !environment->query) {
                return ab_fail(error, "Missing owned follow-camera game service");
            }
            okay = environment->query(environment->context, call, error);
            break;
        }
        case AB_CAMERA_FOLLOW_PROJECT:
            okay = ab_camera_limits_project(
                &system->limits, (AbVec2){call->point.x, system->screen_position.y},
                &call->vector);
            break;
        case AB_CAMERA_FOLLOW_ZOOM:
            call->number = system->mover.current_zoom;
            okay = true;
            break;
        case AB_CAMERA_FOLLOW_SNAP: {
            size_t index;
            okay =
                ab_camera_snap_closest(system->limits.snaps, system->limits.snap_count,
                                       call->targets.values.position, &index);
            if (okay) {
                call->vector = system->limits.snaps[index].position;
                call->snap_id = (float)(index + 1);
            }
            break;
        }
        case AB_CAMERA_FOLLOW_VALIDATE:
            okay = ab_camera_limits_position(
                &system->limits, &limits, &call->targets.values, false, &call->values);
            break;
        case AB_CAMERA_FOLLOW_EXTENTS: {
            AbCameraViewport viewport;
            AbCameraViewportInput input = {
                call->point, system->pixel_size, {0, 0}, call->zoom};
            okay = ab_camera_viewport(&input, &viewport);
            if (okay) {
                call->number = viewport.top_left.y;
            }
            break;
        }
        case AB_CAMERA_FOLLOW_ZOOM_FIT: {
            AbVec2 projected;
            okay = ab_camera_limits_project(&system->limits, call->point, &projected);
            if (!okay) {
                break;
            }
            /* The default initializer supplies closeZoomLevel, while this
             * original method reads closeZoom on equality. Preserve absence. */
            if (call->point.y == projected.y) {
                return ab_fail(error, "Default camera zoom-fit closeZoom is absent");
            }
            okay =
                ab_camera_zoom_fit(projected.y, call->point.y, 0, system->pixel_size.y,
                                   system->limits.bottom, &call->number);
            break;
        }
        case AB_CAMERA_FOLLOW_SCREEN_X:
            call->number = system->screen_position.x;
            okay = true;
            break;
    }
    if (system->automatic_present && system->follow_active &&
        system->follow_environment) {
        system->follow_host.gravity = system->follow_environment->gravity;
    }
    return okay || ab_fail(error, "Cannot resolve owned follow-camera service");
}

bool ab_camera_system_follow_bird(AbCameraSystem *system,
                                  const AbCameraFollowBird *target,
                                  const bool *allow_cancel, const float *compensation,
                                  AbCameraFollowHost *environment, AbError *error) {
    if (!system) {
        return ab_fail(error, "Missing camera system for bird following");
    }
    if (!target) {
        system->automatic_present = false;
        system->follow_active = false;
        system->follow_environment = NULL;
        return true;
    }
    if (!ab_camera_follow_init(&system->follow, target, compensation, error)) {
        return false;
    }
    system->follow.allow_cancel = allow_cancel ? *allow_cancel : true;
    system->follow_environment = environment;
    system->follow_host = (AbCameraFollowHost){
        system, environment ? environment->gravity : 0, follow_service};
    system->follow_active = true;
    system->automatic_present = true;
    return true;
}

const AbCameraFollowBird *ab_camera_system_target_bird(const AbCameraSystem *system) {
    return system && system->automatic_present && system->follow_active
               ? system->follow.target
               : NULL;
}

bool ab_camera_system_remove_bird(AbCameraSystem *system,
                                  const AbCameraFollowBird *bird) {
    if (!system) {
        return false;
    }
    if (ab_camera_system_target_bird(system) == bird) {
        system->automatic_present = false;
        system->follow_active = false;
        system->follow_environment = NULL;
    }
    return true;
}

bool ab_camera_system_follow_targets(AbCameraSystem *system, float seconds,
                                     bool user_input, AbCameraTargets *targets,
                                     bool *present, AbError *error) {
    AbCameraFollowResult result;
    if (!ab_camera_follow_update(&system->follow, seconds, user_input,
                                 &system->follow_host, &result, error)) {
        return false;
    }
    *present = result.targets_present;
    if (result.done) {
        system->automatic_present = false;
        system->follow_active = false;
        system->follow_environment = NULL;
    }
    if (result.targets_present) {
        *targets = (AbCameraTargets){
            .values = result.targets.values,
            .movement_duration = result.targets.movement_duration,
            .zoom_duration =
                result.targets.zoom_duration_present ? result.targets.zoom_duration : 0,
            .movement_tween = AB_CAMERA_TWEEN_SMOOTHSTEP,
            .movement_tween_present = true};
        system->follow_curves = result.targets;
    }
    return true;
}

static bool follow_curve(void *context, AbCameraAxis axis, float time, float start,
                         float change, float duration, float *output, AbError *error) {
    (void)start;
    (void)change;
    AbCameraSystem *system = context;
    if (axis == AB_CAMERA_AXIS_Y) {
        return ab_camera_follow_y_tween(&system->follow_host, output, error);
    }
    return ab_camera_follow_zoom_tween(&system->follow_curves.zoom_curve, time,
                                       duration, output, error);
}

bool ab_camera_system_move(AbCameraSystem *system, float seconds, AbError *error) {
    AbCameraMoverCurves curves = {.context = system,
                                  .axes = (system->follow_curves.custom_y ? 2u : 0u) |
                                          (system->follow_curves.custom_zoom ? 4u : 0u),
                                  .evaluate = follow_curve};
    return ab_camera_mover_update_curves(&system->mover, seconds, &curves, error);
}
