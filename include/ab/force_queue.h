#ifndef AB_FORCE_QUEUE_H
#define AB_FORCE_QUEUE_H

#include "ab/base.h"
#include "ab/physics_geometry.h"

typedef struct {
    AbSpan object;
    AbVec2 force;
    AbVec2 point;
} AbQueuedForce;

typedef struct {
    AbQueuedForce *entries;
    size_t count;
    size_t capacity;
} AbForceQueue;

typedef struct {
    void *context;
    /* Native closure checks for nil, not truthiness: a false-valued object
     * entry is present. Re-read membership for each queued request. */
    bool (*exists)(void *context, AbSpan object, bool *present, AbError *error);
    bool (*apply)(void *context, const AbQueuedForce *force, AbError *error);
} AbForceQueueHost;

/* Append the five captured applyForce arguments into caller-owned bounded
 * storage. Names remain borrowed until clear. Used numbers must be finite.
 * Failure preserves the queue and entries. No world changes or allocation. */
bool ab_force_queue_append(AbForceQueue *queue, AbQueuedForce force, AbError *error);

/* Revisit captured requests in insertion order for one physics step. Missing
 * objects skip; present objects reach apply with the original argument bits.
 * Applying preserves the queue for later steps. Queue/storage membership must
 * remain stable during this pass; host changes to world membership are visible
 * to later requests. Invalid top-level input rejects before effects; a later
 * failure retains preceding host changes. No Lua or frame allocations. */
bool ab_force_queue_apply(const AbForceQueue *queue, AbForceQueueHost host,
                          AbError *error);

/* Frame-end clear, including an enabled frame with zero physics steps. The
 * caller must not clear while apply is active. Retain storage, drop requests. */
bool ab_force_queue_clear(AbForceQueue *queue, AbError *error);

#endif
