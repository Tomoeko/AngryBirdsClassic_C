#ifndef AB_BLUES_TRAJECTORY_BEHAVIOR_H
#define AB_BLUES_TRAJECTORY_BEHAVIOR_H

#include "ab/lua_data.h"

typedef enum {
    AB_BLUES_TRAJECTORY_ADD = 1,
    AB_BLUES_TRAJECTORY_REMOVE,
    AB_BLUES_TRAJECTORY_KEEP,
    AB_BLUES_TRAJECTORY_GET_OBJECT,
} AbBluesTrajectoryOperation;

typedef struct {
    AbBluesTrajectoryOperation operation;
    AbDataValue object, name, result;
    /* Original constructor slots, including nil holes. A KEEP call borrows this
     * fixed projection only during dispatch; it does not expose table identity. */
    AbDataValue keep[4];
    size_t keep_slots;
} AbBluesTrajectoryCall;

typedef struct {
    void *context;
    AbDataValue flying_bird, other_birds;
    bool (*call)(void *context, AbBluesTrajectoryCall *call, AbError *error);
    /* Optional canonical fields at stable borrowed addresses. When bound, read
     * each at its source boundary instead of the corresponding prepared value;
     * service callbacks can replace these fields between the two child lookups. */
    const AbDataValue *live_flying_bird, *live_other_birds;
} AbBluesTrajectoryHost;

typedef enum {
    AB_BLUES_TRAJECTORY_INIT,
    AB_BLUES_TRAJECTORY_LAUNCH,
    AB_BLUES_TRAJECTORY_SPECIALTY,
    AB_BLUES_TRAJECTORY_COLLISION,
    AB_BLUES_TRAJECTORY_DESTROY,
} AbBluesTrajectoryEvent;

/* Original MuseumBluesTrajectoryRenderer's five methods over a stable mutable
 * instance table. Preserve cluster versus split-child branches, field writes
 * before/after callbacks, raw collision fields and fresh otherBirds reads.
 * GET_OBJECT resolves each typed name at its native boundary; its result may
 * be nil. Cluster KEEP has one slot; a selected child's KEEP has four slots,
 * with originalCluster/self frozen before the two lookups. No allocation,
 * renderer disposal or body creation. Plain tables/values only; metatables,
 * coercion and multiple return values remain separate. Hosts may change later
 * fields/bindings but must retain borrowed storage and never reenter this event.
 * A late failure retains earlier effects and stops at that boundary. */
bool ab_blues_trajectory_event(AbDataTable *instance, AbBluesTrajectoryEvent event,
                               AbDataValue other, AbBluesTrajectoryHost *host,
                               AbError *error);

#endif
