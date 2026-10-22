#ifndef AB_BLOCK_REMOVAL_SPAWN_H
#define AB_BLOCK_REMOVAL_SPAWN_H

#include "ab/block_removal_lifecycle.h"

typedef struct {
    void *context;
    AbDataValue block_table, scale_factor;
    float math_pi;
    uint8_t *name_storage;
    size_t name_capacity, maximum_instances;
    /* Original math.random boundary: zero or two arguments. Return data rather
     * than prematurely validating values that are only used after callbacks. */
    bool (*random)(void *context, unsigned count, float lower, float upper,
                   AbDataValue *output, AbError *error);
    bool (*create)(void *context, AbDataValue current_block_table,
                   AbDataValue definition, AbSpan generated_name, float x, float y,
                   AbDataValue *output, AbError *error);
    bool (*rotate)(void *context, AbDataValue created_object, float angle,
                   AbError *error);
} AbBlockRemovalSpawnHost;

/* Ordinary removeBlocks spawn continuation, instructions 289..450. Non-nil
 * spawn selects the grid path even when false; deathSpawn uses truthiness.
 * Cache grid dimensions/cell offsets and each loop bound, but read current
 * object angle/name/position, spawn definition and block-table binding for each
 * creation. Use the recovered binary32 operation order and promoted-double
 * script trigonometry. Create callbacks precede the grid's rotation lookup.
 *
 * Death spawns choose from the captured current types table, before optional
 * callback replacement, using random(1,#types). Their names use the captured
 * dead-map key, positions use detached x/y times current scaleFactor, and
 * rotation caches its host before random() then multiplies by two and current
 * math_pi. Script results are ignored except the created-object/random values.
 *
 * Borrow stable plain tables and disjoint name storage for callback duration.
 * Callbacks may change later fields/bindings; do not free cached data or reenter.
 * Used source numbers/loop limits must be finite, counts <= 2^24-1 and pi in
 * (0,512]. Types use dense numeric arrays (other nonnumeric keys are allowed).
 * Name text uses C numeric locale/nearest rounding. Storage/instance bounds
 * reject on exhaustion, retaining earlier effects; they never omit spawns.
 * No native creation/rotation implementation, Lua, cleanup or allocation here.
 * Native validation of this continuation is tracked separately in the roadmap.
 */
bool ab_block_removal_spawn(const AbBlockRemovalEntry *entry,
                            const AbBlockRemovalDetached *detached,
                            AbBlockRemovalSpawnHost *host, AbError *error);

#endif
