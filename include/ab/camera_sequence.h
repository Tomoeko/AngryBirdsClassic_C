#ifndef AB_CAMERA_SEQUENCE_H
#define AB_CAMERA_SEQUENCE_H

#include "ab/lua_data.h"

typedef enum {
    AB_CAMERA_CONDITION_UNSET,
    AB_CAMERA_CONDITION_ALWAYS,
    AB_CAMERA_CONDITION_DURATION,
    AB_CAMERA_CONDITION_NO_MOVING,
    AB_CAMERA_CONDITION_SPECIALTY,
    AB_CAMERA_CONDITION_CUSTOM,
    AB_CAMERA_CONDITION_VELOCITY,
} AbCameraSequenceCondition;

typedef struct {
    /* Exact action identifier and immutable recovered property table, borrowed
     * for the action host. Conditions are mutable manager-owned stage metadata. */
    AbSpan action;
    const AbDataTable *properties;
    AbCameraSequenceCondition condition;
    float duration;
    float minimum_duration;
    /* Velocity conditions capture a duration independently of the stage's
     * subsequently mutable duration property. */
    float velocity_duration;
    bool duration_present;
    bool interrupt_on_input;
} AbCameraSequenceStage;

typedef struct {
    AbCameraSequenceStage *stages;
    size_t count;
} AbCameraSequence;

/* Prepare a dense, integer-keyed static sequence table. Owns the mutable stage
 * array; action strings and property tables borrow input until free. Numeric
 * duration/minDuration and the two named builtin conditions are supported;
 * arbitrary function conditions require explicit host bindings instead. Stage
 * construction validates supported data before playback, without executing Lua.
 * allocation_limit bounds the stage array. Output starts empty on failure.
 * Free only sources produced by read; manager/source uses must end first. */
bool ab_camera_sequence_read(const AbDataTable *table, size_t allocation_limit,
                             AbCameraSequence *output, AbError *error);
void ab_camera_sequence_free(AbCameraSequence *sequence);

typedef struct AbCameraSequenceManager AbCameraSequenceManager;

typedef enum {
    AB_CAMERA_SEQUENCE_DELEGATE,
    AB_CAMERA_SEQUENCE_CUSTOM_CONDITION,
    AB_CAMERA_SEQUENCE_DONE,
} AbCameraSequenceOperation;

typedef struct {
    AbCameraSequenceOperation operation;
    AbCameraSequenceStage *stage;
    bool condition_result;
} AbCameraSequenceCall;

typedef bool (*AbCameraSequenceHost)(void *context, AbCameraSequenceManager *manager,
                                     AbCameraSequenceCall *call, AbError *error);

struct AbCameraSequenceManager {
    AbCameraSequence *current;
    /* Native sequenceStageIndex is one based and zero when reset. */
    size_t stage_index;
    float timer;
    float stage_timer;
    bool stage_timer_present;
    bool done_callback;
    AbCameraSequenceHost host;
    void *context;
};

/* Numeric/state portion of CameraSequenceManager. The source and mutable stages
 * remain borrowed, stable and owned by a single manager during execution.
 * The host implements action effects, custom conditions and an optional done
 * callback; these are controlled boundaries, not recovered camera actions here.
 * Delegate runs after condition selection. Done runs before reset and sees the
 * still-current sequence/index/timer. Callbacks may change ordinary state, but
 * must not move/free storage or replace the host during a call. No allocation.
 * Validation failures preserve initial state; later host/numeric errors retain
 * preceding state changes, as at the corresponding native boundaries. */
bool ab_camera_sequence_init(AbCameraSequenceManager *manager,
                             AbCameraSequenceHost host, void *context, AbError *error);
/* Reset timer/current/done/index; preserve the existing stageTimer field. */
bool ab_camera_sequence_reset(AbCameraSequenceManager *manager);
bool ab_camera_sequence_end(AbCameraSequenceManager *manager, AbError *error);
/* Prepared stages already contain any caller-applied parameter overrides.
 * Start immediately enters stage one, including for an empty sequence, whose
 * done callback precedes reset. Subsequent stages enter at most once per update.
 * Source count is limited so native binary32 stage indices stay exact. */
bool ab_camera_sequence_start(AbCameraSequenceManager *manager,
                              AbCameraSequence *sequence, bool done_callback,
                              AbError *error);
/* Input interruption precedes timer addition. Duration and no-moving conditions
 * use strict greater-than; moving objects reset the no-moving timer to zero.
 * Specialty completes when availability becomes falsy. Conditions/custom host
 * calls can mutate the manager before its next-stage transition. Inactive or
 * interrupted updates do not use seconds. Used arithmetic must remain finite. */
bool ab_camera_sequence_update(AbCameraSequenceManager *manager, float seconds,
                               bool user_input, bool moving_objects,
                               bool specialty_available, AbError *error);

#endif
