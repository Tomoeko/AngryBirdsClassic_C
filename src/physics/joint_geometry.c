#include "ab/physics_joint.h"

#include <math.h>

static bool finite_point(AbVec2 point) {
    return isfinite(point.x) && isfinite(point.y);
}

static bool finite_frame(const AbJointBodyFrame *frame) {
    return finite_point(frame->transform.position) && isfinite(frame->transform.sine) &&
           isfinite(frame->transform.cosine);
}

static AbVec2 local_vector(const AbRigidTransform *frame, AbVec2 vector) {
    float x = vector.y * frame->sine;
    x += vector.x * frame->cosine;
    float y = vector.y * frame->cosine;
    y -= frame->sine * vector.x;
    return (AbVec2){x, y};
}

static AbVec2 local_point(const AbRigidTransform *frame, AbVec2 point) {
    float y = point.y - frame->position.y;
    float x = point.x - frame->position.x;
    return local_vector(frame, (AbVec2){x, y});
}

bool ab_joint_geometry(const AbJointGeometryInput *input, AbJointGeometry *output,
                       AbError *error) {
    if (!input || !output || (unsigned)input->kind > (unsigned)AB_JOINT_PRISMATIC ||
        !finite_frame(input->bodies) || !finite_frame(input->bodies + 1) ||
        !finite_point(input->world_anchors[0])) {
        return ab_fail(error, "Invalid joint geometry input/storage");
    }
    bool distance = input->kind == AB_JOINT_DISTANCE;
    if ((distance && !finite_point(input->world_anchors[1])) ||
        (!distance &&
         (!isfinite(input->bodies[0].angle) || !isfinite(input->bodies[1].angle))) ||
        (input->kind == AB_JOINT_PRISMATIC && !finite_point(input->world_axis))) {
        return ab_fail(error, "Nonfinite required joint geometry input");
    }
    AbJointGeometry result = {0};
    result.local_anchors[0] =
        local_point(&input->bodies[0].transform, input->world_anchors[0]);
    result.local_anchors[1] = local_point(&input->bodies[1].transform,
                                          input->world_anchors[distance ? 1 : 0]);
    if (distance) {
        float y = input->world_anchors[1].y - input->world_anchors[0].y;
        float x = input->world_anchors[1].x - input->world_anchors[0].x;
        float squared = y * y;
        squared += x * x;
        result.length = sqrtf(squared);
    } else {
        result.reference_angle = input->bodies[1].angle - input->bodies[0].angle;
    }
    if (input->kind == AB_JOINT_PRISMATIC) {
        result.local_axis =
            local_vector(&input->bodies[0].transform, input->world_axis);
    }
    if (!finite_point(result.local_anchors[0]) ||
        !finite_point(result.local_anchors[1]) || !finite_point(result.local_axis) ||
        !isfinite(result.reference_angle) || !isfinite(result.length)) {
        return ab_fail(error, "Joint geometry arithmetic overflow");
    }
    *output = result;
    return true;
}
