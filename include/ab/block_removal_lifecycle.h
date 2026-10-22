#ifndef AB_BLOCK_REMOVAL_LIFECYCLE_H
#define AB_BLOCK_REMOVAL_LIFECYCLE_H

#include "ab/block_removal_cursor.h"

typedef struct {
    AbDataValue spawn, death_spawn, x, y;
} AbBlockRemovalDetached;

typedef struct {
    void *context;
    AbDataValue block_table;
    bool (*remove)(void *context, AbSpan captured_key, AbError *error);
} AbBlockRemovalDetachHost;

typedef struct {
    AbBlockRemovalState *removal;
    AbStringMap *level_goals;
    AbDataValue level_name;
} AbBlockRemovalCleanupState;

typedef struct {
    void *context;
    bool (*remove_pig)(void *context, AbSpan captured_key, AbError *error);
    bool (*collect_gift)(void *context, AbDataValue current_level_name,
                         AbDataValue current_x, AbDataValue current_y, AbError *error);
} AbBlockRemovalCleanupHost;

/* Ordinary removeBlocks instructions 274..288. Cache raw-catalog spawn and
 * deathSpawn independently of the merged scoring definition, then raw object
 * x/y. Require removeObject(captured dead-map key), even if object.name changed.
 * The callback owns actual native object/body removal; cached data must survive
 * it. Output commits only on success; earlier callback effects survive failure.
 * No spawn behavior, cleanup, allocation or Lua is performed here. */
bool ab_block_removal_detach(const AbBlockRemovalEntry *entry,
                             AbBlockRemovalDetachHost *host,
                             AbBlockRemovalDetached *output, AbError *error);

/* After spawn continuation, original instructions 451..475: clear levelGoals,
 * current objects.world, current objects.dynamic, then current deadBlocks by
 * the captured key, in that order. Failing a later write retains earlier ones.
 * Truthy current cached-object levelGoal requests removePig(key). Then truthy
 * current isGiftBox requests manager collectGiftBox(current levelName,x,y), with
 * its receiver bound by the host. Read gift fields after removePig mutations.
 *
 * Borrow plain mutable world/dynamic tables with bounded storage, the measured
 * mutable string maps and pinned key wrappers as in the cursor. Callback changes
 * remain live. No metatables, hash iteration for world/dynamic, body removal,
 * spawning or implicit registry ownership. Required hosts fail when reached.
 * Do not reenter or release the cached object during these calls. */
bool ab_block_removal_cleanup(const AbBlockRemovalEntry *entry,
                              AbBlockRemovalCleanupState *state,
                              AbBlockRemovalCleanupHost *host, AbError *error);

#endif
