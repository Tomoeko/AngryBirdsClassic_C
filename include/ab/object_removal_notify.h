#ifndef AB_OBJECT_REMOVAL_NOTIFY_H
#define AB_OBJECT_REMOVAL_NOTIFY_H

#include "ab/lua_data.h"
#include "ab/ui_pointer.h"

typedef struct {
    AbDataValue definition;
    void *context;
} AbObjectRemovalObject;

typedef struct {
    AbUiPointerValue on_remove;
} AbObjectRemovalLevel;

typedef enum {
    AB_OBJECT_REMOVAL_DEFINITION,
    AB_OBJECT_REMOVAL_LEVEL,
} AbObjectRemovalNotice;

typedef struct {
    void *context;
    AbDataValue block_table, level_name;
    /* Pure lookup in this function's current environment. Missing/falsy
     * bindings skip; truthy non-functions reject when called. */
    bool (*binding)(void *context, AbDataValue key, AbUiPointerValue *output,
                    AbError *error);
    /* Pure current g_level_scripts lookup: NULL denotes a missing/falsy entry.
     * Plain level tables only; repeated original lookups are retained. */
    bool (*level)(void *context, AbDataValue current_level,
                  AbObjectRemovalLevel **output, AbError *error);
    /* Both channels receive just the physical object, without a receiver.
     * Status represents host failure, never an original script return value. */
    bool (*call)(void *context, AbObjectRemovalNotice notice, AbUiPointerValue function,
                 AbObjectRemovalObject *object, AbError *error);
} AbObjectRemovalNotifyHost;

/* Gamelogic's private on-removed helper, original prototype 107147. Cache the raw
 * blockTable.blocks[object.definition], independently of merged definitions.
 * Its truthy onRemoved key conditionally calls the current truthy environment
 * binding. Then read current levelName and optional level onRemoveBlock, even if
 * the definition callback changed that name or replaced the script registry.
 * Both functions receive one object argument and ignore all script returns.
 *
 * Borrow well-formed plain tables/scalar definition keys, stable object and
 * callback storage. Callbacks may change later fields/bindings; retain the cached
 * raw table and do not reenter/free storage. There is no native removal, map
 * deletion, allocation, Lua or owned registry here. Errors preserve preceding
 * callback changes. Missing raw definitions are errors, not skipped defaults.
 */
bool ab_object_removal_notify(AbObjectRemovalObject *object,
                              AbObjectRemovalNotifyHost *host, AbError *error);

#endif
