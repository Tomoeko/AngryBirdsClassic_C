#ifndef AB_CLASSIC_FORCES_H
#define AB_CLASSIC_FORCES_H

#include "ab/classic_level.h"
#include "ab/force_queue.h"
#include "ab/game_physics_prepare.h"

typedef struct AbClassicForces AbClassicForces;

typedef struct {
    size_t storage_bytes;
    size_t sound_choices;
    /* Zero reserves the initial count; otherwise bound later direct circles. */
    size_t object_capacity;
} AbClassicForceLimits;

typedef bool (*AbClassicForceHost)(void *context, size_t object,
                                   const AbObjectForceCall *call, AbError *error);

typedef struct {
    AbGamePhysicsObject *objects;
    const size_t *source_indices;
    size_t count;
} AbClassicForceView;

/* Own initial resolved force/friction/interval/sound state in native C++ object
 * map order: length-aware lexicographic name bytes. Source indices address the
 * level's creation-order object array. Preserve cached creation mass separately
 * from current engine mass, and borrow the canonical per-object gravity flag.
 * Resolve definition/material fields with the observed six-field truthiness
 * gate; numeric zero enables preparation. Ground bypasses ObjectCreation fields.
 * Numeric intervals require the level's prepared initial sprite snapshot. Sound
 * tables require contiguous numeric string choices, independent of entry order.
 * Unsupported field coercions reject rather than guessing native behavior.
 *
 * All source tables, strings, bodies and gravity flags remain borrowed: close
 * this owner before its level, and do not move/reuse its object/body slots.
 * Level-forgotten rows skip later preparation and named queued forces. Later
 * direct circles register separately below. The caller supplies
 * the shared random state and actual sprite/
 * audio host. A missing RNG/host rejects only when a required effect uses it.
 * The host receives the source object index. No effects, RNG draws, world changes,
 * Lua execution or frame allocations occur here. storage_bytes bounds all owned
 * allocations, including the owner and choice spans. Output is NULL on failure.
 */
bool ab_classic_forces_open(AbClassicLevel *level, AbClassicForceLimits limits,
                            AbRandom48 *random, AbClassicForceHost host, void *context,
                            AbClassicForces **output, AbError *error);
void ab_classic_forces_close(AbClassicForces *forces);
size_t ab_classic_forces_bytes(const AbClassicForces *forces);

/* Borrow mutable rows in current native name order. Array storage and owned
 * interval/sound/context addresses stay stable, but explicit registration can
 * shift row positions. Release individual row references and refresh this view
 * after registering, before the next physics pass. NULL returns an empty view. */
AbClassicForceView ab_classic_forces_view(AbClassicForces *forces);

/* Register the next canonical direct circle between passes, using the native
 * circle constructor's disabled preparation gate and actual gravity flag.
 * Never resolve/apply its later copied definition's ObjectCreation force fields.
 * Insert in native name order without moving retained interval/sound contexts.
 * Capacity is pre-reserved. No allocation, RNG draw, resource effect or body
 * change. Reject gaps/duplicates/removed or non-circle identities. */
bool ab_classic_forces_register_circle(AbClassicForces *forces, size_t object,
                                       AbError *error);

/* Replay a queue against this owner's registered object map and live body
 * slots. Unknown/removed names skip, static bodies ignore force. Numeric point
 * application uses the original GameLua force boundary. The borrowed world must
 * be healthy and unlocked. Queue retention/clear timing is caller-owned. */
bool ab_classic_forces_apply_queue(AbClassicForces *forces, const AbForceQueue *queue,
                                   AbError *error);

#endif
