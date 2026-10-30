#ifndef AB_BLOCK_REMOVAL_CURSOR_H
#define AB_BLOCK_REMOVAL_CURSOR_H

#include "ab/lua_data.h"
#include "ab/string_map.h"

typedef struct {
    AbDataValue objects;
    /* Each present string-map handle points to a stable borrowed AbDataValue,
     * representing that deadBlocks value independently of the world entry. */
    AbStringMap *dead_blocks;
} AbBlockRemovalState;

typedef struct {
    const AbStringMap *iteration;
    size_t cursor;
    bool finished;
    AbSpan resume_key;
    bool resume_pending;
} AbBlockRemovalCursor;

typedef struct {
    AbSpan key;
    AbDataValue object;
} AbBlockRemovalEntry;

typedef struct {
    void *context;
    bool (*rocket)(void *context, AbDataValue current_name, AbError *error);
} AbBlockRemovalHost;

/* Enter removeBlocks' original plain string-keyed pairs traversal. An initially
 * nil objects.world returns immediately, without reading deadBlocks. Otherwise
 * retain that iteration table independently of later global binding changes.
 * A non-table/falsy-but-non-nil world only errors when an entry is indexed.
 * Output commits on success. No allocation, Lua or gameplay effects. */
bool ab_block_removal_open(const AbBlockRemovalState *state,
                           AbBlockRemovalCursor *output, AbError *error);

/* Continue its instructions 10..42 until the next ordinary-removal entry or end.
 * Remove a key from the CURRENT deadBlocks binding when its current world value
 * is falsy. For remaining cached dead values, nil strength skips; nonnumeric
 * strength fails before rocket fields are read. Strength <= 0, non-nil rocket
 * and nil deathTimerFull call shootRocket(current_name), then delete the key from
 * the current dead-map binding. False rocket values are non-nil. Positive/NaN
 * strengths and false/non-nil timers can still select ordinary removal.
 *
 * found=true returns the cached key/value just before gameObjectScript:onDestroy.
 * The enclosing owner runs the remaining removal pipeline before calling next
 * again; this component does not invoke it or remove a selected entry. Normal
 * exhaustion sets found=false and preserves entry output. Plain tables, numeric
 * strength bits, stable values/keys/maps and original physical hash order only.
 * Hosts may delete/replace values, insert/rehash and switch map/world bindings,
 * while the captured iteration table stays alive. Resume native next after the
 * last retained key in its current hash slot; do not continue from a stale slot,
 * restart, deduplicate visits or replace the captured table. A discarded resume
 * key fails as native next does. Nil writes to a different current binding use
 * its bounded map storage.
 * No metatables, custom pairs or GC scheduling. Abandon the cursor after failure:
 * preceding map writes and callback changes survive. Do not reenter or release borrowed
 * data.
 */
bool ab_block_removal_next(AbBlockRemovalState *state, AbBlockRemovalCursor *cursor,
                           const AbBlockRemovalHost *host, AbBlockRemovalEntry *output,
                           bool *found, AbError *error);

#endif
