#ifndef AB_CLASSIC_JOINT_H
#define AB_CLASSIC_JOINT_H

#include "ab/lua_data.h"
#include "ab/physics_world_solve.h"

typedef enum {
    AB_CLASSIC_JOINT_DISTANCE = 1,
    AB_CLASSIC_JOINT_WELD,
    AB_CLASSIC_JOINT_REVOLUTE,
    AB_CLASSIC_JOINT_PRISMATIC,
    AB_CLASSIC_JOINT_GLUE,
} AbClassicJointType;

typedef struct {
    const AbDataTable *table;
    AbJointBodyFrame bodies[2];
    size_t body_indices[2];
    AbVec2 legacy_world_anchors[2];
} AbClassicJointSource;

typedef struct {
    AbClassicJointType type;
    AbWorldSolveJoint joint;
    AbSpan name;
    AbSpan endpoints[2];
    AbVec2 coordinates[2];
    AbVec2 world_axis;
    int coordinate_type;
    bool back_and_forth;
    AbDataValue breakable;
    AbDataValue break_force;
    float destroy_timer;
} AbClassicJoint;

/* Read finite typed level-constructor data for distance/weld/revolute/prismatic
 * joints (resource types 1..4) and deferred glue (type 5), resolve immediate
 * local geometry from supplied body frames,
 * and retain the game metadata/defaults. Indices are supplied by the enclosing
 * named-object lookup; this function does not mutate/create world objects.
 * Distance coordinate type 1 uses the explicit legacy positional arguments;
 * other modes do not read those arguments. Strings remain borrowed from table.
 * No allocation occurs. Invalid/unsupported input preserves output. Deferred
 * glue (type 5) retains metadata and destroy_timer without reading body frames
 * or indices; its joint field is not a world definition. Parameter changes,
 * queue production, breakage and level order are separate.
 */
bool ab_classic_joint_read(const AbClassicJointSource *source, AbClassicJoint *output,
                           AbError *error);

/* Define a queued glue pair's rigid weld at the midpoint of body origins.
 * This only constructs a definition. The owning game first applies its joint
 * removal policy to each joint connected to the second body, including the
 * associated named metadata for registered joints.
 * No world mutation occurs; invalid input preserves output.
 */
bool ab_classic_glue_definition(const AbJointBodyFrame bodies[2],
                                const size_t body_indices[2], AbWorldSolveJoint *output,
                                AbError *error);

#endif
