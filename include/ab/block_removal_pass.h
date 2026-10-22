#ifndef AB_BLOCK_REMOVAL_PASS_H
#define AB_BLOCK_REMOVAL_PASS_H

#include "ab/block_removal_particles.h"
#include "ab/block_removal_score.h"
#include "ab/block_removal_spawn.h"

typedef struct {
    AbBlockRemovalHost *traversal;
    AbBlockRemovalScoreHost *scoring;
    AbBlockRemovalEffectsHost *effects;
    AbBlockRemovalParticlesHost *particles;
    AbBlockRemovalDetachHost *detach;
    AbBlockRemovalSpawnHost *spawns;
    AbBlockRemovalCleanupState *cleanup_state;
    AbBlockRemovalCleanupHost *cleanup;
} AbBlockRemovalPassHost;

/* Compose removeBlocks over the measured retained string-map cursor. For each
 * ordinary selection: destroy/score, prepare effects, notify/removal functions/
 * particles, detach, spawn, then clear maps and remove goals/collect gifts.
 * Rocket handling remains in traversal and does not enter ordinary phases.
 *
 * Borrow phase hosts rather than copying them. Callback changes to later host
 * pointers, bindings, data or maps remain live. The effects and cleanup views
 * must reference this same removal state. A missing phase view fails when its
 * phase is reached; spawns may be NULL when both spawn paths are absent/falsy.
 * Initially nil world or an empty captured dead table needs no phase hosts.
 *
 * Retain cached metadata and pinned keys until the pass ends, even after native
 * removal releases a physical body/object. Cursor topology is stable except
 * deletion/value replacement; creation must not insert/rehash its captured dead
 * map. No allocation, Lua, native effect implementation, owned registry or error
 * recovery. Failure preserves all earlier phases/entries; discard on failure.
 * Full composed native replay and owned-session binding are separate evidence
 * gates from the individual measured phases.
 */
bool ab_block_removal_pass(AbBlockRemovalState *state, AbBlockRemovalPassHost *host,
                           AbError *error);

#endif
