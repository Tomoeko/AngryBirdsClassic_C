#include "ab/camera_sequence.h"

#include <math.h>

static bool valid_source(const AbCameraSequence *sequence) {
    return sequence && sequence->count < UINT32_C(0x1000000) &&
           (!sequence->count || sequence->stages);
}

static AbCameraSequenceStage *current_stage(AbCameraSequenceManager *manager) {
    return valid_source(manager->current) && manager->stage_index &&
                   manager->stage_index <= manager->current->count
               ? manager->current->stages + manager->stage_index - 1
               : NULL;
}

bool ab_camera_sequence_reset(AbCameraSequenceManager *manager) {
    if (!manager) {
        return false;
    }
    manager->timer = 0;
    manager->current = NULL;
    manager->done_callback = false;
    manager->stage_index = 0;
    return true;
}

bool ab_camera_sequence_init(AbCameraSequenceManager *manager,
                             AbCameraSequenceHost host, void *context, AbError *error) {
    if (!manager || !host) {
        return ab_fail(error, "Missing camera sequence manager or action host");
    }
    *manager = (AbCameraSequenceManager){.host = host, .context = context};
    return ab_camera_sequence_reset(manager);
}

bool ab_camera_sequence_end(AbCameraSequenceManager *manager, AbError *error) {
    if (!manager || !manager->host) {
        return ab_fail(error, "Missing camera sequence manager or host");
    }
    if (manager->done_callback) {
        AbCameraSequenceCall call = {.operation = AB_CAMERA_SEQUENCE_DONE};
        if (!manager->host(manager->context, manager, &call, error)) {
            return false;
        }
    }
    return ab_camera_sequence_reset(manager);
}

static bool next_stage(AbCameraSequenceManager *manager, AbError *error) {
    if (!valid_source(manager->current) ||
        manager->stage_index > manager->current->count) {
        return ab_fail(error, "Invalid camera sequence transition");
    }
    ++manager->stage_index;
    if (manager->stage_index > manager->current->count) {
        return ab_camera_sequence_end(manager, error);
    }
    manager->stage_timer = 0;
    manager->stage_timer_present = true;
    AbCameraSequenceStage *stage = current_stage(manager);
    if ((unsigned)stage->condition > AB_CAMERA_CONDITION_VELOCITY) {
        return ab_fail(error, "Unsupported camera sequence condition");
    }
    if (stage->condition == AB_CAMERA_CONDITION_UNSET) {
        stage->condition = stage->duration_present ? AB_CAMERA_CONDITION_DURATION
                                                   : AB_CAMERA_CONDITION_ALWAYS;
    }
    AbCameraSequenceCall call = {.operation = AB_CAMERA_SEQUENCE_DELEGATE,
                                 .stage = stage};
    return manager->host(manager->context, manager, &call, error);
}

bool ab_camera_sequence_start(AbCameraSequenceManager *manager,
                              AbCameraSequence *sequence, bool done_callback,
                              AbError *error) {
    if (!manager || !manager->host || !valid_source(sequence)) {
        return ab_fail(error, "Invalid camera sequence start");
    }
    manager->current = sequence;
    manager->stage_index = 0;
    manager->done_callback = done_callback;
    return next_stage(manager, error);
}

static bool condition_result(AbCameraSequenceManager *manager,
                             AbCameraSequenceStage *stage, bool moving_objects,
                             bool specialty_available, bool *output, AbError *error) {
    switch (stage->condition) {
        case AB_CAMERA_CONDITION_ALWAYS:
            *output = true;
            return true;
        case AB_CAMERA_CONDITION_DURATION:
            if (!isfinite(stage->duration)) {
                return ab_fail(error, "Nonfinite camera sequence duration");
            }
            *output = manager->stage_timer > stage->duration;
            return true;
        case AB_CAMERA_CONDITION_VELOCITY:
            if (!isfinite(stage->velocity_duration)) {
                return ab_fail(error, "Nonfinite camera velocity duration");
            }
            *output = manager->stage_timer > stage->velocity_duration;
            return true;
        case AB_CAMERA_CONDITION_NO_MOVING:
            if (moving_objects) {
                manager->stage_timer = 0;
            }
            if (!isfinite(stage->minimum_duration)) {
                return ab_fail(error, "Nonfinite camera sequence minimum duration");
            }
            *output = manager->stage_timer > stage->minimum_duration;
            return true;
        case AB_CAMERA_CONDITION_SPECIALTY:
            *output = !specialty_available;
            return true;
        case AB_CAMERA_CONDITION_CUSTOM: {
            AbCameraSequenceCall call = {
                .operation = AB_CAMERA_SEQUENCE_CUSTOM_CONDITION, .stage = stage};
            if (!manager->host(manager->context, manager, &call, error)) {
                return false;
            }
            *output = call.condition_result;
            return true;
        }
        default:
            return ab_fail(error, "Uninitialized camera sequence condition");
    }
}

bool ab_camera_sequence_update(AbCameraSequenceManager *manager, float seconds,
                               bool user_input, bool moving_objects,
                               bool specialty_available, AbError *error) {
    if (!manager || !manager->host) {
        return ab_fail(error, "Missing camera sequence manager or host");
    }
    if (!manager->current) {
        return true;
    }
    AbCameraSequenceStage *stage = current_stage(manager);
    if (!stage) {
        return ab_fail(error, "Invalid active camera sequence stage");
    }
    if (user_input && stage->interrupt_on_input) {
        return ab_camera_sequence_end(manager, error);
    }
    float next_time = manager->stage_timer + seconds;
    if (!manager->stage_timer_present || !isfinite(manager->stage_timer) ||
        !isfinite(seconds) || !isfinite(next_time)) {
        return ab_fail(error, "Invalid camera sequence timer update");
    }
    manager->stage_timer = next_time;
    bool complete = false;
    if (!condition_result(manager, stage, moving_objects, specialty_available,
                          &complete, error)) {
        return false;
    }
    return !complete || next_stage(manager, error);
}
