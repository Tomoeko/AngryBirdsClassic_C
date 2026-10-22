#ifndef AB_PHYSICS_JOINT_H
#define AB_PHYSICS_JOINT_H

#include "ab/base.h"
#include "ab/physics_geometry.h"

typedef enum {
    AB_JOINT_DISTANCE,
    AB_JOINT_WELD,
    AB_JOINT_REVOLUTE,
    AB_JOINT_PRISMATIC,
} AbJointKind;

typedef enum {
    AB_JOINT_LIMIT_INACTIVE,
    AB_JOINT_LIMIT_LOWER,
    AB_JOINT_LIMIT_UPPER,
    AB_JOINT_LIMIT_EQUAL,
} AbJointLimitState;

typedef struct {
    AbRigidTransform transform;
    float angle;
} AbJointBodyFrame;

typedef struct {
    AbJointKind kind;
    AbJointBodyFrame bodies[2];
    AbVec2 world_anchors[2];
    AbVec2 world_axis;
} AbJointGeometryInput;

typedef struct {
    AbVec2 local_anchors[2];
    AbVec2 local_axis;
    float reference_angle;
    float length;
} AbJointGeometry;

typedef struct {
    AbVec2 local_center;
    float angle;
    float inverse_mass;
    float inverse_inertia;
} AbJointSolverBody;

typedef struct {
    float seconds;
    float time_ratio;
    bool warm_start;
} AbJointStep;

/* Construct local anchors from explicit body frames and world-space inputs.
 * Distance uses both anchors and returns their separation length. Other kinds
 * share world_anchors[0] and return second-minus-first reference angle.
 * Prismatic also rotates world_axis into the first body's local frame, without
 * normalizing it. Unused output fields are positive zero; unused input fields
 * are not read. Required inputs/results must be finite. Caller storage is
 * disjoint; failure preserves output. No allocation, body mutation, solver,
 * game-coordinate policy, motor/limit defaults or world joint creation occurs. */
bool ab_joint_geometry(const AbJointGeometryInput *input, AbJointGeometry *output,
                       AbError *error);

#endif
