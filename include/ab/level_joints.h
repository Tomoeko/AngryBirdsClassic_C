#ifndef AB_LEVEL_JOINTS_H
#define AB_LEVEL_JOINTS_H

#include "ab/classic_joints.h"
#include "ab/level_bodies.h"

typedef struct {
    size_t immediate;
    size_t deferred;
    size_t hash_capacity;
} AbLevelJointRequirements;

/* Inventory the static level's joint table without creating anything. Nonempty
 * tables require their recovered constructor layout, string keys and supported
 * type values. Nil/empty tables need no workspace. Invalid input preserves
 * output. This does not inventory later event-created joints or glue pairs.
 */
bool ab_level_joint_requirements(const AbLuaData *level,
                                 AbLevelJointRequirements *output, AbError *error);

/* Create initial game joints in the original static string-table iteration
 * order, after ordinary bodies/ground exist and before queued bird transforms.
 * Resolve immediate endpoints by their exact object names and use current
 * engine frames. Deferred glue retains its names without requiring live bodies.
 * Borrow source data/object names until registry close. Legacy coordinate mode
 * has zero positional arguments here; none of the pinned level tables uses it.
 * The caller owns fixed registry/world/order storage. No extra allocation,
 * script VM, creation notification, contact/timer queue or gameplay hooks run.
 * Failure can follow partial creation; discard the enclosing level on failure.
 */
bool ab_level_joints_create(AbClassicJoints *joints, const AbLuaData *level,
                            const AbLevelBodies *bodies, AbStringOrderWorkspace *order,
                            AbError *error);

#endif
