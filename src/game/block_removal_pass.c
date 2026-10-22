#include "ab/block_removal_pass.h"

static bool ordinary(const AbBlockRemovalEntry *entry, AbBlockRemovalState *state,
                     AbBlockRemovalPassHost *host, AbError *error) {
    if (!host || !host->scoring) {
        return ab_fail(error, "Missing removal scoring phase");
    }
    AbBlockRemovalScored scored = {0};
    if (!ab_block_removal_score(entry, host->scoring, &scored, error)) {
        return false;
    }
    if (!host->effects || host->effects->state != state) {
        return ab_fail(error, "Removal effects need the current shared state");
    }
    AbBlockRemovalEffects effects = {0};
    if (!ab_block_removal_effects(entry, scored.definition, host->effects, &effects,
                                  error)) {
        return false;
    }
    if (!host->particles) {
        return ab_fail(error, "Missing removal notification/particle phase");
    }
    if (!ab_block_removal_particles(entry, &effects, host->particles, error)) {
        return false;
    }
    if (!host->detach) {
        return ab_fail(error, "Missing native removal boundary phase");
    }
    AbBlockRemovalDetached detached = {0};
    if (!ab_block_removal_detach(entry, host->detach, &detached, error)) {
        return false;
    }
    if (host->spawns) {
        if (!ab_block_removal_spawn(entry, &detached, host->spawns, error)) {
            return false;
        }
    } else if (detached.spawn.kind != AB_DATA_NIL ||
               (detached.death_spawn.kind != AB_DATA_NIL &&
                (detached.death_spawn.kind != AB_DATA_BOOLEAN ||
                 detached.death_spawn.bits))) {
        return ab_fail(error, "Missing required removal spawn phase");
    }
    if (!host->cleanup_state || host->cleanup_state->removal != state ||
        !host->cleanup) {
        return ab_fail(error, "Removal cleanup needs the current shared state");
    }
    return ab_block_removal_cleanup(entry, host->cleanup_state, host->cleanup, error);
}

bool ab_block_removal_pass(AbBlockRemovalState *state, AbBlockRemovalPassHost *host,
                           AbError *error) {
    AbBlockRemovalCursor cursor = {0};
    if (!ab_block_removal_open(state, &cursor, error)) {
        return false;
    }
    for (;;) {
        AbBlockRemovalEntry entry = {0};
        bool found = false;
        if (!ab_block_removal_next(state, &cursor, host ? host->traversal : NULL,
                                   &entry, &found, error)) {
            return false;
        }
        if (!found) {
            return true;
        }
        if (!ordinary(&entry, state, host, error)) {
            return false;
        }
    }
}
