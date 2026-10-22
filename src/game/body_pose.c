#include "ab/body_pose.h"
#include "ab/rotation.h"

#include <math.h>

static bool finite_pose(AbBodyPose pose) {
    return isfinite(pose.position.x) && isfinite(pose.position.y) &&
           isfinite(pose.angle);
}

bool ab_body_pose_capture(AbBodyMotion *body, uint32_t collider, AbBodyPose *sample) {
    if (!body || !sample || (unsigned)body->mass.type > AB_BODY_DYNAMIC) {
        return false;
    }
    if (!body->awake) {
        return true;
    }
    const AbRigidTransform *transform = &body->mass.transform;
    AbBodyPose result = {.position = transform->position};
    if (!isfinite(body->mass.mass) ||
        !ab_angle_from_components(transform->sine, transform->cosine, &result.angle) ||
        !finite_pose(result)) {
        return false;
    }
    if (collider == 3 && body->mass.mass != 0) {
        body->bullet = true;
    }
    *sample = result;
    return true;
}

bool ab_body_pose_interpolate(const AbBodyPose history[2], uint32_t current,
                              float fraction, AbBodyPose *output) {
    if (!history || !output || current > 1 || !isfinite(fraction) ||
        !finite_pose(history[0]) || !finite_pose(history[1])) {
        return false;
    }
    const AbBodyPose *previous = history + (1 - current);
    const AbBodyPose *next = history + current;
    float inverse = 1.0f - fraction;
    float difference = next->angle - previous->angle;
    float magnitude = difference < 0 ? -difference : difference;
    float old_angle = previous->angle;
    const float pi = 0x1.921fb6p+1f;
    if (pi <= magnitude) {
        float period = pi + pi;
        old_angle = next->angle > old_angle ? old_angle + period : old_angle - period;
    }
    AbBodyPose result = {
        .position = {inverse * previous->position.x + fraction * next->position.x,
                     inverse * previous->position.y + fraction * next->position.y},
        .angle = inverse * old_angle + fraction * next->angle};
    if (!isfinite(inverse) || !isfinite(difference) || !finite_pose(result)) {
        return false;
    }
    *output = result;
    return true;
}
