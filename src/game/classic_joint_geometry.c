#include "classic_joint_internal.h"

#include <math.h>

static AbVec2 world_point(AbRigidTransform frame, AbVec2 point) {
    float x = point.x * frame.cosine;
    x -= point.y * frame.sine;
    float y = point.y * frame.cosine;
    y += point.x * frame.sine;
    return (AbVec2){x + frame.position.x, y + frame.position.y};
}

static AbVec2 distance_origin(AbRigidTransform frame) {
    float cosine = frame.cosine * 0.0f;
    float sine = frame.sine * 0.0f;
    return (AbVec2){(cosine - sine) + frame.position.x,
                    (cosine + sine) + frame.position.y};
}

static bool distance_geometry(const AbClassicJointSource *source, AbClassicJoint *joint,
                              AbError *error) {
    AbDistanceDefinition *definition = &joint->joint.constraint.definition.distance;
    AbVec2 points[2];
    for (size_t index = 0; index < 2; ++index) {
        AbRigidTransform frame = source->bodies[index].transform;
        if (joint->coordinate_type == 0) {
            points[index] = distance_origin(frame);
            joint->coordinates[index] = points[index];
        } else if (joint->coordinate_type == 1) {
            points[index] = source->legacy_world_anchors[index];
            AbVec2 origin = distance_origin(frame);
            definition->local_anchors[index] =
                (AbVec2){points[index].x - origin.x, points[index].y - origin.y};
            joint->coordinates[index] = points[index];
        } else {
            definition->local_anchors[index] = joint->coordinates[index];
            points[index] = world_point(frame, joint->coordinates[index]);
        }
    }
    float y = points[1].y - points[0].y;
    float x = points[1].x - points[0].x;
    float square = y * y;
    square += x * x;
    definition->length = sqrtf(square);
    if (!isfinite(definition->length)) {
        return ab_fail(error, "Classic distance joint geometry overflow");
    }
    return true;
}

bool ab_classic_joint_geometry(const AbClassicJointSource *source,
                               AbClassicJoint *joint, AbError *error) {
    if (joint->joint.constraint.kind == AB_JOINT_DISTANCE) {
        return distance_geometry(source, joint, error);
    }
    AbVec2 anchor = world_point(source->bodies[0].transform, joint->coordinates[0]);
    if (joint->joint.constraint.kind == AB_JOINT_WELD) {
        AbVec2 second = world_point(source->bodies[1].transform, joint->coordinates[1]);
        anchor.x += (second.x - anchor.x) * 0.5f;
        anchor.y += (second.y - anchor.y) * 0.5f;
    }
    AbJointGeometryInput input = {.kind = joint->joint.constraint.kind,
                                  .bodies = {source->bodies[0], source->bodies[1]},
                                  .world_anchors = {anchor}};
    if (input.kind == AB_JOINT_PRISMATIC) {
        input.world_axis = joint->joint.constraint.definition.prismatic.local.axis;
    }
    AbJointGeometry geometry;
    if (!ab_joint_geometry(&input, &geometry, error)) {
        return false;
    }
    switch (input.kind) {
        case AB_JOINT_WELD:
            joint->joint.constraint.definition.weld.local_anchors[0] =
                geometry.local_anchors[0];
            joint->joint.constraint.definition.weld.local_anchors[1] =
                geometry.local_anchors[1];
            joint->joint.constraint.definition.weld.reference_angle =
                geometry.reference_angle;
            break;
        case AB_JOINT_REVOLUTE:
            joint->joint.constraint.definition.revolute.local_anchors[0] =
                geometry.local_anchors[0];
            joint->joint.constraint.definition.revolute.local_anchors[1] =
                geometry.local_anchors[1];
            joint->joint.constraint.definition.revolute.reference_angle =
                geometry.reference_angle;
            break;
        case AB_JOINT_PRISMATIC:
            joint->joint.constraint.definition.prismatic.local_anchors[0] =
                geometry.local_anchors[0];
            joint->joint.constraint.definition.prismatic.local_anchors[1] =
                geometry.local_anchors[1];
            joint->joint.constraint.definition.prismatic.reference_angle =
                geometry.reference_angle;
            return ab_prismatic_axes_prepare(
                geometry.local_axis,
                &joint->joint.constraint.definition.prismatic.local, error);
        default:
            return false;
    }
    return true;
}

bool ab_classic_glue_definition(const AbJointBodyFrame bodies[2],
                                const size_t body_indices[2], AbWorldSolveJoint *output,
                                AbError *error) {
    if (!bodies || !body_indices || !output || body_indices[0] == body_indices[1]) {
        return ab_fail(error, "Invalid classic glue pair/storage");
    }
    AbVec2 anchor = distance_origin(bodies[0].transform);
    AbVec2 second = distance_origin(bodies[1].transform);
    anchor.x += (second.x - anchor.x) * 0.5f;
    anchor.y += (second.y - anchor.y) * 0.5f;
    AbJointGeometryInput input = {.kind = AB_JOINT_WELD,
                                  .bodies = {bodies[0], bodies[1]},
                                  .world_anchors = {anchor}};
    AbJointGeometry geometry;
    if (!ab_joint_geometry(&input, &geometry, error)) {
        return false;
    }
    *output = (AbWorldSolveJoint){
        .first_body = body_indices[0],
        .second_body = body_indices[1],
        .constraint = {
            .kind = AB_JOINT_WELD,
            .definition.weld = {
                .local_anchors = {geometry.local_anchors[0], geometry.local_anchors[1]},
                .reference_angle = geometry.reference_angle}}};
    return true;
}
