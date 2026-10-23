#ifndef AB_LEVEL_EGG_H
#define AB_LEVEL_EGG_H

#include "ab/lua_data.h"

typedef struct {
    const char *level, *object, *egg;
    bool keep_object;
} AbLevelEggScript;

typedef struct {
    void *context;
    bool full_version;
    const AbDataValue *eagle_bait_launched;
    bool (*unlocked)(void *context, AbSpan egg, AbDataValue *output, AbError *error);
    bool (*achieve)(void *context, AbSpan egg, AbError *error);
} AbLevelEggHost;

/* The common factory registrations in level_scripts.lua. Borrow the static
 * descriptor. Special level scripts with additional handlers are not matched. */
const AbLevelEggScript *ab_level_egg_script(AbSpan level);

/* Original loaded-object removal helper 3189. Mutate fresh decoded level
 * tables before creating bodies or joints. Keep original entries and their
 * constructor layout; write nil without compacting. The named world entry
 * must already exist. No later insertion, metatable or callback is supported.
 * Failure retains earlier nil writes, as the source does. */
bool ab_level_remove_loaded_object(const AbLuaData *level, AbSpan name, AbError *error);

/* Factory3681 and helpers3395/3533. full_version is the captured module flag.
 * Preparation queries unlock state only for full-version removable objects.
 * Removal reads the current physical name and current eagle flag. The host
 * owns settings, achievement effects and all borrowed inputs. No allocation. */
bool ab_level_egg_prepare(const AbLevelEggScript *script, const AbLuaData *level,
                          AbLevelEggHost host, AbError *error);
bool ab_level_egg_removed(const AbLevelEggScript *script, const AbDataTable *object,
                          AbLevelEggHost host, AbError *error);

#endif
