#ifndef AB_BLOCK_REMOVAL_PARTICLES_H
#define AB_BLOCK_REMOVAL_PARTICLES_H

#include "ab/block_removal_effects.h"
#include "ab/ui_pointer.h"

typedef struct {
    AbUiPointerValue *items;
    size_t capacity;
} AbBlockRemovalFunctions;

typedef struct {
    void *context;
    bool (*notify)(void *context, AbDataValue cached_object, AbError *error);
    /* Pure current removeFunctions field view. NONE is nil; OBJECT carries a
     * borrowed AbBlockRemovalFunctions pointer. Other kinds fail at ipairs. */
    bool (*functions)(void *context, AbDataValue cached_object,
                      AbUiPointerValue *output, AbError *error);
    bool (*call)(void *context, AbUiPointerValue function, AbDataValue cached_object,
                 AbError *error);
    bool (*property)(void *context, AbDataValue cached_object, AbSpan key,
                     AbDataValue *output, AbError *error);
    bool (*new_particles)(void *context, AbSpan captured_key,
                          AbDataValue removal_descriptor, bool flag1, bool flag2,
                          AbError *error);
    bool (*old_particles)(void *context, AbSpan captured_key,
                          AbDataValue cached_particles, AbDataValue cached_amount,
                          bool flag1, bool flag2, AbError *error);
} AbBlockRemovalParticlesHost;

/* Ordinary removeBlocks instructions 231..273. Notify the cached object first,
 * then capture its current optional removeFunctions table and traverse ascending
 * slots until nil. Every non-nil item must be callable, including false/zero.
 * Each receives only the cached object. Rebinding the object's list does not
 * change the captured iterator; later item writes within capacity remain live.
 *
 * Then current truthy killSilently skips particles. Otherwise query current
 * script:get(useNewParticleTable). Falsy uses the earlier effects' cached
 * particles/amount. Truthy caches the new-particle function before a fresh
 * script:get(particles), indexes its removal descriptor, then calls with the
 * captured dead-map key and two false flags. An absent/non-table particle value
 * fails at indexing even if the captured new-particle host is missing.
 *
 * Borrow plain data, function/list views, keys and fixed-capacity storage.
 * Callbacks may change later fields, bindings or list items; do not resize/free
 * cached lists or reenter. No allocation, Lua, native removal or spawning.
 * Script results are ignored; status denotes failure and retains earlier
 * callbacks. Concrete notification, removal functions and particle effects are
 * host boundaries. Custom ipairs/metatables and mixed-key lists are external.
 */
bool ab_block_removal_particles(const AbBlockRemovalEntry *entry,
                                const AbBlockRemovalEffects *cached_effects,
                                AbBlockRemovalParticlesHost *host, AbError *error);

#endif
