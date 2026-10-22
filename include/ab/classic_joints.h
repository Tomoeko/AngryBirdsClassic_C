#ifndef AB_CLASSIC_JOINTS_H
#define AB_CLASSIC_JOINTS_H

#include "ab/classic_joint.h"
#include "ab/game_object_remove.h"
#include "ab/physics_world.h"

typedef struct {
    AbClassicJoint definition;
    size_t world_joint;
    bool metadata_visible;
    AbJointParameters parameter_updates;
} AbClassicJointRecord;

typedef struct {
    AbClassicJointRecord *joints;
    size_t joint_capacity;
    AbClassicJointRecord *glue;
    size_t glue_capacity;
} AbClassicJointStorage;

typedef struct {
    AbPhysicsWorld *world;
    AbClassicJointStorage storage;
    size_t joint_count;
    size_t glue_count;
    bool failed;
} AbClassicJoints;

typedef struct {
    size_t first_body;
    size_t second_body;
} AbClassicGluePair;

typedef struct {
    AbClassicGluePair *pairs;
    size_t count, capacity;
} AbClassicGlueQueue;

typedef bool (*AbClassicGlueBodyLookup)(void *context, AbSpan name, size_t *body,
                                        AbError *error);

/* Consume the callback's resolved glueContactPair value after basicBeginContact
 * callback eligibility/dispatch. Non-tables do nothing. A table's numeric keys
 * 1 and 2 must contain strings; resolve both names in order and append one pair.
 * Do not deduplicate, reverse, filter by registered metadata or discard unknown
 * names. Lookup represents an absent native body by SIZE_MAX; later glue
 * consumption safely rejects that unsupported pair before world mutation.
 * Borrow caller-owned disjoint queue/table storage. No allocation or engine
 * mutation occurs. Lookup must not mutate/reenter this queue or its sources.
 * Typed string lookup only; Lua coercion/metatables are separate. Failure
 * preserves queue/count, although the first lookup may already have run.
 */
bool ab_classic_glue_queue_contact(AbClassicGlueQueue *queue, AbDataValue response,
                                   AbClassicGlueBodyLookup lookup, void *context,
                                   AbError *error);

/* Borrow an open world, disjoint record arrays, definitions and their strings.
 * Records retain creation order, and removal shifts later records. World joint
 * slots remain independent/stable. Closing clears this registry, with world
 * lifetime and joint storage still owned by the enclosing game.
 */
bool ab_classic_joints_open(AbClassicJoints *joints, AbPhysicsWorld *world,
                            AbClassicJointStorage storage, AbError *error);
void ab_classic_joints_close(AbClassicJoints *joints);

/* Register a definition read by ab_classic_joint_read. Immediate kinds create
 * an engine joint; glue records remain deferred. A locked immediate creation
 * retains a record with world_joint == SIZE_MAX, matching the native null
 * engine result. New registrations replace the visible metadata of the same
 * name across both record lists, while retaining older records.
 * Success writes the engine slot, or SIZE_MAX for deferred/locked creation.
 * Capacity/control errors reject before mutation.
 */
bool ab_classic_joints_create(AbClassicJoints *joints, const AbClassicJoint *definition,
                              size_t *world_joint, AbError *error);
const AbClassicJointRecord *ab_classic_joints_metadata(const AbClassicJoints *joints,
                                                       AbSpan name);

/* Remove the first registered record matching an engine slot. Unregistered
 * joints are left alone. Clear its named metadata before engine removal, then
 * erase the record. A locked world preserves the engine joint while removing
 * game metadata. A null record cannot be removed through an unlocked world.
 */
bool ab_classic_joints_remove(AbClassicJoints *joints, size_t world_joint,
                              AbError *error);

/* Remove the first immediate record whose name matches, otherwise the first
 * deferred record. Clear all visible metadata for that name even if a later
 * duplicate remains. Deferred removal has no engine effects. Missing names are
 * no-ops. Immediate locked/null-engine behavior is the same as slot removal,
 * with the selected record index retained even when null slots repeat. This is
 * the native operation below the script's destruction-event notification. */
bool ab_classic_joints_remove_named(AbClassicJoints *joints, AbSpan name,
                                    AbError *error);

/* GameLua.removeObject's joint-record suffix, after physical object removal.
 * Visit deferred records before immediate records, erasing every record whose
 * first or second endpoint equals object. Preserve the order of survivors and
 * clear all visible metadata of each removed name. Do not touch engine joints:
 * body destruction already did that when unlocked; locked bodies/joints survive.
 * This suffix must also run when the physical object key was absent.
 *
 * Only a matching FIRST endpoint marks the current physical SECOND endpoint
 * delayed_destroy, then assigns destroy_timer from the latest named metadata.
 * A matching second endpoint does not mark the first. If the second object is
 * absent, skip both assignments. Missing named metadata fails after marking the
 * second object, retaining that record and earlier effects. Typed timers retain
 * raw binary32 values; numeric-string coercion is outside this boundary.
 * Registry/world identity, stable object handles/names and no reentry are caller
 * invariants. This neither owns objects nor implements teleporter cleanup.
 */
bool ab_classic_joints_forget_object(AbClassicJoints *joints,
                                     AbGameObjectCoreRegistry *objects, AbSpan object,
                                     AbError *error);

/* Visit queued pairs in order. Request game removal for each joint connected
 * to the second body, then create an unregistered rigid weld at body origins'
 * midpoint. Locked worlds retain metadata effects but create no engine weld.
 * Clear count only after consuming the entire queue. Structural validation
 * rejects before mutation; later failures are sticky after possible partial
 * work. Queue production/contact/timer policy is separate.
 */
bool ab_classic_joints_glue(AbClassicJoints *joints, const AbClassicGluePair *pairs,
                            size_t *count, AbError *error);

/* Check every immediate record with this name. Enabled revolute/prismatic
 * motors reverse at inclusive upper/lower bounds in their travel direction.
 * Saved metadata and cached solver state remain unchanged. back_and_forth is
 * caller policy; this explicit operation does not consult that metadata flag.
 * Locked worlds are supported. Invalid record/math state can fail after earlier
 * matching records have changed and makes the registry failed.
 */
bool ab_classic_joints_check_limits(AbClassicJoints *joints, AbSpan name,
                                    AbError *error);

/* Update only the first immediate record with this name, retaining creation
 * order. Accept literal Boolean motor/limit and numeric speed/maxTorque/bounds
 * for motors, or numeric frequency/dampingRatio for distance joints. Other
 * fields/types are ignored. Save accepted assignments in the currently visible
 * named metadata, which may belong to a later duplicate of another joint kind.
 * Welds and missing names are no-ops. No allocation or metadata insertion occurs.
 * Numeric strings/coercions are outside this typed boundary. Invalid state or
 * finite-domain violations reject before mutation. Locked worlds are supported.
 */
bool ab_classic_joints_set_parameters(AbClassicJoints *joints, AbSpan name,
                                      const AbDataTable *parameters, AbError *error);

typedef bool (*AbClassicJointBreakRequest)(void *context, AbSpan joint_name,
                                           AbError *error);

/* Visit immediate records in creation order whose end1 or end2 matches object.
 * Use the latest visible named metadata, including a later deferred record.
 * Literal true breakable and force strictly greater than break_force request
 * deferred destruction once per matching record; duplicate names may repeat.
 * AbClassicJoint's typed metadata treats absent/non-numeric thresholds as zero;
 * numeric-string coercion is outside this boundary. No engine lookup, removal,
 * allocation or queue ownership occurs, and locked/null-engine records work.
 * Names remain borrowed. The required host must enqueue requests without
 * mutating/reentering this registry/world. Invalid arguments reject before work;
 * bad record state or host failure is sticky after possible earlier requests.
 */
bool ab_classic_joints_break(AbClassicJoints *joints, AbSpan object, float force,
                             AbClassicJointBreakRequest request, void *context,
                             AbError *error);

/* Return a saved parameter (one AB_JOINT_SET_* bit), including typed defaults
 * and later updates. Unsupported/absent fields return nil. The record remains
 * borrowed; this does not query the current engine definition.
 */
AbDataValue ab_classic_joint_parameter(const AbClassicJointRecord *record,
                                       uint32_t parameter);

#endif
