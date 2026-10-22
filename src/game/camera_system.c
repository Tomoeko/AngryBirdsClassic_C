#include "ab/camera_system.h"
#include "ab/camera_selection.h"
#include "camera_system_internal.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static bool named(AbSpan value, const char *name) {
    size_t size = strlen(name);
    return value.size == size && value.data && memcmp(value.data, name, size) == 0;
}

static AbDataValue field(const AbDataTable *properties, const char *name) {
    if (!properties || (properties->count && !properties->entries)) {
        return (AbDataValue){0};
    }
    const AbDataValue *value = ab_data_get(
        properties, (AbDataValue){.kind = AB_DATA_STRING,
                                  .string = {(const uint8_t *)name, strlen(name)}});
    return value ? *value : (AbDataValue){0};
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL &&
           (value.kind != AB_DATA_BOOLEAN || value.bits != 0);
}

static bool numeric(AbDataValue value, float *output) {
    if (value.kind != AB_DATA_NUMBER) {
        return false;
    }
    memcpy(output, &value.bits, sizeof(*output));
    return isfinite(*output);
}

static AbCameraLimitContext limit_context(const AbCameraSystem *system) {
    return (AbCameraLimitContext){.pixel_size = system->pixel_size,
                                  .current_position = system->screen_position,
                                  .current_zoom = system->mover.current_zoom};
}

static bool default_tweens(const AbDataTable *properties, bool axes, AbError *error) {
    const char *names[] = {"tweenFunction", "movementTweenFunction",
                           "zoomTweenFunction"};
    for (size_t index = 0; index < (axes ? 3u : 1u); ++index) {
        if (truthy(field(properties, names[index]))) {
            return ab_fail(error, "Camera action tween selection requires recovery");
        }
    }
    return true;
}

static const AbCameraSnap *snap_point(const AbCameraSystem *system, AbDataValue id) {
    if (!system->limits.snap_count) {
        return NULL;
    }
    if (id.kind == AB_DATA_STRING) {
        return named(id.string, "bird") ? system->limits.snaps
               : named(id.string, "castle")
                   ? system->limits.snaps + system->limits.snap_count - 1
                   : NULL;
    }
    float number = 0;
    if (!numeric(id, &number) || number < 1 ||
        number > (float)system->limits.snap_count) {
        return NULL;
    }
    size_t index = (size_t)number;
    return (float)index == number ? system->limits.snaps + index - 1 : NULL;
}

static bool velocity_move(AbCameraSystem *system, AbCameraSequenceStage *stage,
                          AbVec2 target, AbCameraMove *move, AbError *error) {
    AbDataValue velocity = field(stage->properties, "velocity");
    float speed = 0;
    if (!truthy(velocity)) {
        return true;
    }
    if (!numeric(velocity, &speed)) {
        return ab_fail(error, "Unsupported camera action velocity");
    }
    if (speed == 0) {
        return true;
    }
    float x = system->screen_position.x - target.x;
    float y = system->screen_position.y - target.y;
    float xx = x * x;
    float yy = y * y;
    float distance = (float)sqrt((double)(xx + yy));
    float duration = distance / speed;
    if (!isfinite(duration)) {
        return ab_fail(error, "Nonfinite camera action velocity duration");
    }
    stage->velocity_duration = duration;
    stage->duration = duration;
    stage->duration_present = true;
    stage->condition = AB_CAMERA_CONDITION_VELOCITY;
    move->duration = duration;
    move->duration_present = true;
    return true;
}

static bool begin_move(AbCameraSystem *system, AbCameraSequenceStage *stage,
                       bool to_snap, AbError *error) {
    if (!default_tweens(stage->properties, to_snap, error)) {
        return false;
    }
    AbCameraMove move = {.duration = stage->duration,
                         .duration_present = stage->duration_present};
    AbDataValue zoom = field(stage->properties, "zoom");
    if (to_snap) {
        const AbCameraSnap *snap =
            snap_point(system, field(stage->properties, "snapPoint"));
        if (!snap) {
            return ab_fail(error, "Invalid camera snap identifier");
        }
        if (!velocity_move(system, stage, snap->position, &move, error)) {
            return false;
        }
        AbDataValue validate = field(stage->properties, "validateValues");
        return ab_camera_system_goto_snap(
            system, (size_t)(snap - system->limits.snaps), truthy(zoom),
            validate.kind == AB_DATA_NIL || truthy(validate), &move, error);
    }
    AbDataValue position = field(stage->properties, "position");
    if (position.kind != AB_DATA_TABLE || !position.table) {
        return ab_fail(error, "Invalid camera action target position");
    }
    AbCameraValues values = {0};
    AbDataValue x = field(position.table, "x");
    AbDataValue y = field(position.table, "y");
    if ((truthy(x) && !numeric(x, &values.position.x)) ||
        (truthy(y) && !numeric(y, &values.position.y)) ||
        (truthy(zoom) && !numeric(zoom, &values.zoom))) {
        return ab_fail(error, "Invalid camera action target position or zoom");
    }
    values.present = (truthy(x) ? AB_CAMERA_VALUE_X : 0u) |
                     (truthy(y) ? AB_CAMERA_VALUE_Y : 0u) |
                     (truthy(zoom) ? AB_CAMERA_VALUE_ZOOM : 0u);
    AbDataValue velocity = field(stage->properties, "velocity");
    float speed = 0;
    if (truthy(velocity) && numeric(velocity, &speed) && speed != 0 &&
        (values.present & 3u) != 3u) {
        return ab_fail(error, "Velocity camera position requires both coordinates");
    }
    if (!velocity_move(system, stage, values.position, &move, error)) {
        return false;
    }
    return ab_camera_system_goto_position(system, &values, &move, error);
}

static bool begin_follow(AbCameraSystem *system, AbCameraSequenceStage *stage,
                         AbError *error) {
    if (!system->follow_resolver) {
        return ab_fail(error, "Missing camera sequence follow-target resolver");
    }
    const AbCameraFollowBird *bird = NULL;
    AbCameraFollowHost *environment = NULL;
    if (!system->follow_resolver(system->follow_resolver_context,
                                 field(stage->properties, "target"), &bird,
                                 &environment, error)) {
        return false;
    }
    if (!bird) {
        return ab_camera_system_follow_bird(system, NULL, NULL, NULL, NULL, error);
    }
    AbDataValue compensation = field(stage->properties, "velocityCompensation");
    float number = 0;
    const float *value = NULL;
    if (truthy(compensation)) {
        if (!numeric(compensation, &number)) {
            return ab_fail(error, "Unsupported sequence follow compensation");
        }
        value = &number;
    }
    return ab_camera_system_follow_bird(system, bird, &stage->interrupt_on_input, value,
                                        environment, error);
}

bool ab_camera_system_bind_follow(AbCameraSystem *system, void *context,
                                  AbCameraSystemFollowResolver resolver) {
    if (!system) {
        return false;
    }
    system->follow_resolver = resolver;
    system->follow_resolver_context = context;
    return true;
}

static bool sequence_host(void *context, AbCameraSequenceManager *manager,
                          AbCameraSequenceCall *call, AbError *error) {
    (void)manager;
    AbCameraSystem *system = context;
    if (call->operation == AB_CAMERA_SEQUENCE_DONE) {
        return !system->done || system->done(system->done_context, system, error);
    }
    if (call->operation != AB_CAMERA_SEQUENCE_DELEGATE || !call->stage) {
        return ab_fail(error, "Custom camera sequence condition requires a binding");
    }
    AbCameraSequenceStage *stage = call->stage;
    if (!default_tweens(stage->properties, false, error)) {
        return false;
    }
    if (named(stage->action, "wait")) {
        return true;
    }
    if (named(stage->action, "followTarget")) {
        return begin_follow(system, stage, error);
    }
    if (named(stage->action, "lockCameraLimits")) {
        system->limits.enabled = true;
        return true;
    }
    if (named(stage->action, "unlockCameraLimits")) {
        system->limits.enabled = false;
        return true;
    }
    if (named(stage->action, "disableInput")) {
        system->input_enabled = false;
        return true;
    }
    if (named(stage->action, "enableInput")) {
        system->input_enabled = true;
        return true;
    }
    if (named(stage->action, "moveToSnapPoint")) {
        return begin_move(system, stage, true, error);
    }
    if (named(stage->action, "moveToPosition")) {
        return begin_move(system, stage, false, error);
    }
    return ab_fail(error, "Camera sequence action requires recovery");
}

bool ab_camera_system_movement_tweens(AbCameraSystem *system, AbCameraTween movement,
                                      AbCameraTween zoom) {
    return system && ab_camera_mover_tweens(&system->mover, movement, zoom);
}

bool ab_camera_system_open(const AbCameraSnap *snaps, size_t count, AbVec2 pixel_size,
                           float absolute_bottom, float horizontal_leeway,
                           size_t allocation_limit, AbCameraSystem **output,
                           AbError *error) {
    if (!output) {
        return ab_fail(error, "Missing camera system output");
    }
    *output = NULL;
    AbCameraLimits limits;
    if (count >= UINT32_C(0x1000000) || allocation_limit < sizeof(AbCameraSystem) ||
        !ab_camera_limits_initialize(snaps, count, pixel_size, absolute_bottom,
                                     horizontal_leeway, &limits)) {
        return ab_fail(error,
                       "Invalid camera system configuration or allocation limit");
    }
    AbCameraSystem *system = calloc(1, sizeof(*system));
    if (!system) {
        return ab_fail(error, "Cannot allocate camera system owner");
    }
    system->limits = limits;
    system->pixel_size = pixel_size;
    system->input_enabled = true;
    if (!ab_camera_mover_init(&system->mover) ||
        !ab_camera_sequence_init(&system->sequence, sequence_host, system, error)) {
        ab_camera_system_close(system);
        return false;
    }
    *output = system;
    return true;
}

void ab_camera_system_close(AbCameraSystem *system) {
    free(system);
}

bool ab_camera_system_query_viewport(const AbCameraSystem *system,
                                     AbCameraViewport *output) {
    if (!system || !output)
        return false;
    if (system->viewport_present) {
        *output = system->viewport;
        return true;
    }
    return ab_camera_viewport(&(AbCameraViewportInput){system->screen_position,
                                                       system->pixel_size,
                                                       {0, 0},
                                                       system->mover.current_zoom},
                              output);
}

bool ab_camera_system_resize(AbCameraSystem *system, AbVec2 pixel_size,
                             AbError *error) {
    AbCameraLimits limits;
    if (!system || !isfinite(pixel_size.x) || !isfinite(pixel_size.y) ||
        pixel_size.x <= 0 || pixel_size.y <= 0 ||
        !ab_camera_limits_initialize(system->limits.snaps, system->limits.snap_count,
                                     pixel_size, system->limits.absolute_bottom,
                                     system->limits.horizontal_leeway, &limits))
        return ab_fail(error, "Invalid camera drawable-size change");
    limits.enabled = system->limits.enabled;
    system->limits = limits;
    system->pixel_size = pixel_size;
    system->viewport_present = false;
    return true;
}

AbCameraSystemView ab_camera_system_view(const AbCameraSystem *system) {
    return system ? (AbCameraSystemView){.mover = &system->mover,
                                         .limits = &system->limits,
                                         .sequence = &system->sequence,
                                         .viewport = system->viewport_present
                                                         ? &system->viewport
                                                         : NULL,
                                         .pixel_size = system->pixel_size,
                                         .saved_toggle_position =
                                             system->saved_toggle_position,
                                         .saved_toggle_position_present =
                                             system->saved_toggle_position_present,
                                         .automatic_present = system->automatic_present,
                                         .input_enabled = system->input_enabled,
                                         .screen_position = system->screen_position,
                                         .follow = system->automatic_present &&
                                                           system->follow_active
                                                       ? &system->follow
                                                       : NULL,
                                         .follow_curves =
                                             system->follow_curves.custom_y ||
                                                     system->follow_curves.custom_zoom
                                                 ? &system->follow_curves
                                                 : NULL}
                  : (AbCameraSystemView){0};
}

static bool queue_move(AbCameraSystem *system, AbCameraValues values,
                       const AbCameraMove *move, bool to_snap, bool validate,
                       AbError *error) {
    if (!system || (values.present & ~7u)) {
        return ab_fail(error, "Invalid queued camera move");
    }
    CameraGoTo next = {.position = system->mover.current_position,
                       .zoom = system->mover.current_zoom,
                       .validate = validate,
                       .move = move ? *move : (AbCameraMove){0}};
    if (values.present & AB_CAMERA_VALUE_X) {
        next.position.x = values.position.x;
    }
    if (values.present & AB_CAMERA_VALUE_Y) {
        next.position.y = values.position.y;
    }
    if (values.present & AB_CAMERA_VALUE_ZOOM) {
        next.zoom = values.zoom;
    }
    next.duration = next.move.duration_present ? next.move.duration : 0;
    if (!to_snap && !next.move.duration_present) {
        float x = next.position.x - system->mover.current_position.x;
        float y = next.position.y - system->mover.current_position.y;
        float xx = x * x;
        float yy = y * y;
        next.duration = sqrtf(xx + yy) / 1000.0f;
    }
    if (!isfinite(next.position.x) || !isfinite(next.position.y) ||
        !isfinite(next.zoom) || !isfinite(next.duration) ||
        (next.move.movement_tween_present &&
         (unsigned)next.move.movement_tween > AB_CAMERA_TWEEN_SMOOTHSTEP) ||
        (next.move.zoom_tween_present &&
         (unsigned)next.move.zoom_tween > AB_CAMERA_TWEEN_SMOOTHSTEP)) {
        return ab_fail(error, "Unsupported queued camera values or tweens");
    }
    system->automatic = next;
    system->automatic_present = true;
    system->follow_active = false;
    return true;
}

bool ab_camera_system_goto_snap(AbCameraSystem *system, size_t index,
                                bool use_snap_zoom, bool validate,
                                const AbCameraMove *move, AbError *error) {
    if (!system || index >= system->limits.snap_count) {
        return ab_fail(error, "Invalid queued camera snap");
    }
    const AbCameraSnap *snap = system->limits.snaps + index;
    return queue_move(
        system, (AbCameraValues){snap->position, snap->zoom, use_snap_zoom ? 7u : 3u},
        move, true, validate, error);
}

bool ab_camera_system_goto_position(AbCameraSystem *system,
                                    const AbCameraValues *values,
                                    const AbCameraMove *move, AbError *error) {
    return values ? queue_move(system, *values, move, false, false, error)
                  : ab_fail(error, "Missing queued camera position");
}

bool ab_camera_system_toggle_zoom(AbCameraSystem *system, const AbVec2 *position,
                                  bool constrain_input, AbError *error) {
    if (!system) {
        return ab_fail(error, "Missing camera zoom toggle owner");
    }
    float relative;
    if (!ab_camera_relative_zoom(system->mover.current_zoom, system->limits.close_zoom,
                                 system->limits.far_zoom, &relative)) {
        return ab_fail(error, "Cannot resolve camera zoom toggle");
    }
    AbCameraValues wanted = {system->mover.current_position, system->limits.far_zoom,
                             7};
    if (relative < 0.5f) {
        system->saved_toggle_position = system->mover.current_position;
        system->saved_toggle_position_present = true;
    } else {
        wanted.position = position ? *position
                          : system->saved_toggle_position_present
                              ? system->saved_toggle_position
                              : system->mover.current_position;
        wanted.zoom = system->limits.close_zoom;
        system->saved_toggle_position_present = false;
    }
    AbCameraLimitContext context = limit_context(system);
    if ((constrain_input &&
         !ab_camera_limits_input(&system->limits, &context, &wanted, &wanted)) ||
        !ab_camera_limits_position(&system->limits, &context, &wanted, false,
                                   &wanted)) {
        return ab_fail(error, "Cannot constrain camera zoom toggle");
    }
    AbCameraMove move = {.duration = 0.5f,
                         .movement_tween = AB_CAMERA_TWEEN_CUBIC_OUT,
                         .zoom_tween = AB_CAMERA_TWEEN_CUBIC_OUT,
                         .duration_present = true,
                         .movement_tween_present = true,
                         .zoom_tween_present = true};
    return ab_camera_system_goto_position(system, &wanted, &move, error);
}

bool ab_camera_system_input_enabled(AbCameraSystem *system, bool enabled) {
    if (!system) {
        return false;
    }
    system->input_enabled = enabled;
    return true;
}

bool ab_camera_system_start(AbCameraSystem *system, AbCameraSequence *sequence,
                            AbCameraSystemDone done, void *context, AbError *error) {
    if (!system || !sequence || sequence->count >= UINT32_C(0x1000000) ||
        (sequence->count && !sequence->stages)) {
        return ab_fail(error, "Invalid camera system sequence start");
    }
    system->done = done;
    system->done_context = context;
    return ab_camera_sequence_start(&system->sequence, sequence, done != NULL, error);
}

bool ab_camera_system_cancel_sequence(AbCameraSystem *system, AbError *error) {
    return system ? ab_camera_sequence_end(&system->sequence, error)
                  : ab_fail(error, "Missing camera system for sequence cancellation");
}

bool ab_camera_system_stop_movement(AbCameraSystem *system) {
    if (!system)
        return false;
    system->automatic_present = false;
    return ab_camera_mover_clear_targets(&system->mover);
}

static bool automatic_targets(AbCameraSystem *system, AbCameraTargets *output,
                              AbError *error) {
    CameraGoTo goal = system->automatic;
    AbCameraTargets targets = {.values = {goal.position, goal.zoom, 7},
                               .movement_duration = goal.duration,
                               .zoom_duration = goal.duration,
                               .movement_tween = goal.move.movement_tween,
                               .zoom_tween = goal.move.zoom_tween,
                               .movement_tween_present =
                                   goal.move.movement_tween_present,
                               .zoom_tween_present = goal.move.zoom_tween_present};
    AbCameraLimitContext context = limit_context(system);
    if (goal.validate &&
        !ab_camera_limits_position(&system->limits, &context, &targets.values, false,
                                   &targets.values)) {
        return ab_fail(error, "Cannot validate automatic camera target");
    }
    system->automatic_present = false;
    *output = targets;
    return true;
}

static bool apply_targets(AbCameraSystem *system, const AbCameraTargets *targets,
                          AbError *error) {
    uint32_t present = targets->values.present;
    if (((present & 3) == 3 &&
         !ab_camera_mover_position(&system->mover, targets->values.position,
                                   targets->movement_duration)) ||
        ((present & AB_CAMERA_VALUE_ZOOM) &&
         !ab_camera_mover_zoom(&system->mover, targets->values.zoom,
                               targets->zoom_duration))) {
        return ab_fail(error, "Cannot set camera mover targets");
    }
    if ((present & 3) == 3 || (present & AB_CAMERA_VALUE_ZOOM) ||
        targets->movement_tween_present || targets->zoom_tween_present) {
        AbCameraTween movement = targets->movement_tween_present
                                     ? targets->movement_tween
                                     : AB_CAMERA_TWEEN_CUBIC_IN_OUT;
        AbCameraTween zoom =
            targets->zoom_tween_present ? targets->zoom_tween : movement;
        if (!ab_camera_mover_tweens(&system->mover, movement, zoom)) {
            return ab_fail(error, "Unsupported camera target tween selection");
        }
        system->follow_curves = (AbCameraFollowTargets){0};
    }
    return true;
}

bool ab_camera_system_update(AbCameraSystem *system, float seconds,
                             const AbCameraSystemInput *input, AbError *error) {
    if (!system || !input) {
        return ab_fail(error, "Missing camera system or resolved input");
    }
    AbCameraTargets targets = input->targets;
    bool present = input->targets_present;
    bool followed = false;
    AbCameraLimitContext context = limit_context(system);
    if (present && !ab_camera_limits_input(&system->limits, &context, &targets.values,
                                           &targets.values)) {
        return ab_fail(error, "Cannot constrain camera input targets");
    }
    if (!ab_camera_sequence_update(&system->sequence, seconds, input->user_input,
                                   input->moving_objects, input->specialty_available,
                                   error)) {
        return false;
    }
    if (system->automatic_present) {
        if (system->follow_active) {
            if (!ab_camera_system_follow_targets(system, seconds, input->user_input,
                                                 &targets, &present, error)) {
                return false;
            }
            followed = true;
        } else {
            if (!automatic_targets(system, &targets, error)) {
                return false;
            }
            present = true;
        }
    }
    AbCameraFollowTargets curves = system->follow_curves;
    if (present) {
        if (!apply_targets(system, &targets, error)) {
            return false;
        }
        if (followed) {
            system->follow_curves = curves;
        }
    }
    if (!ab_camera_system_move(system, seconds, error)) {
        return ab_fail(error, "Cannot update camera movement");
    }
    system->screen_position = system->mover.current_position;
    AbCameraViewportInput viewport = {system->mover.current_position,
                                      system->pixel_size, input->shake,
                                      system->mover.current_zoom};
    if (!ab_camera_viewport(&viewport, &system->viewport)) {
        return ab_fail(error, "Cannot publish camera viewport");
    }
    system->viewport_present = true;
    return true;
}
