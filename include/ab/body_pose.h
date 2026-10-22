#ifndef AB_BODY_POSE_H
#define AB_BODY_POSE_H

#include "ab/physics_step.h"

typedef struct {
    AbVec2 position;
    float angle;
} AbBodyPose;

/* Native GameLua pose-history pass after a physics step. A sleeping body leaves
 * its destination sample unchanged. An awake body supplies transform position
 * and the signed atan2f of its rotation components. Bird collider 3 with nonzero
 * mass becomes a continuous-collision bullet in this same pass. Caller selects
 * the history slot and executes this only at the recovered accumulator gate.
 * Used fields/results must be finite; failure preserves body and sample.
 */
bool ab_body_pose_capture(AbBodyMotion *body, uint32_t collider, AbBodyPose *sample);

/* Interpolate the older history slot toward current, with target binary32
 * multiply/add order and shortest-angle adjustment when the difference is at
 * least pi. The resulting angle is not normalized. No fraction clamp is added.
 * All used values/results must be finite; current is 0 or 1. Failure preserves
 * output. Caller owns awake/pause gating and resetting poses after transforms.
 */
bool ab_body_pose_interpolate(const AbBodyPose history[2], uint32_t current,
                              float fraction, AbBodyPose *output);

#endif
