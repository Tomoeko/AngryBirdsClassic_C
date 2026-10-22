#ifndef AB_BLOCK_EVENT_H
#define AB_BLOCK_EVENT_H

#include "ab/lua_data.h"

typedef struct AbBlockEventObject AbBlockEventObject;
typedef struct AbBlockEventScript AbBlockEventScript;

typedef bool (*AbBlockEventFunction)(AbBlockEventScript *script,
                                     AbBlockEventObject *object,
                                     const AbDataTable *definition,
                                     const AbDataValue *arguments, size_t count,
                                     AbError *error);

typedef struct {
    AbSpan name;
    AbBlockEventFunction function;
} AbBlockEventBinding;

struct AbBlockEventScript {
    AbBlockEventBinding *bindings;
    size_t count;
    void *context;
};

struct AbBlockEventObject {
    AbSpan name;
    AbSpan definition;
    AbBlockEventScript **scripts;
    size_t count;
    void *context;
    bool scripts_present;
    bool tracked;
    /* Caller-owned attachment storage available to block-script application. */
    size_t capacity;
};

typedef struct {
    void *context;
    /* Resolve the current raw block definition before every actual handler.
     * A NULL result represents the native nil definition and is permitted. */
    bool (*definition)(void *context, AbBlockEventObject *object,
                       const AbDataTable **output, AbError *error);
    /* Native global event traversal checks Lua truthiness, unlike force queues. */
    bool (*world_truthy)(void *context, AbSpan name, bool *present, AbError *error);
} AbBlockEventHost;

/* Original BlockScriptManager.triggerEvent over already attached C scripts.
 * Bindings have unique names and attached arrays are dense. Absent scripts
 * return immediately. For each script in attachment order, read
 * its current named handler, resolve the current raw definition, then invoke it
 * with self/object/definition and unchanged typed variadic arguments. NULL
 * functions represent absent/false handlers. Unused definitions are not read.
 * Arrays and membership remain stable during a pass; callbacks may replace
 * later handler values and definition bindings. Borrow all spans/data. No Lua
 * or allocation. A late failure retains prior callback changes. */
bool ab_block_event(AbBlockEventObject *object, AbSpan event,
                    const AbDataValue *arguments, size_t count, AbBlockEventHost host,
                    AbError *error);

/* Original triggerGlobalEvent over a supplied native active-object map order.
 * Re-read world truthiness for every tracked object. Nil/false removes it from
 * future passes; otherwise dispatch the object event above. Stable membership
 * and lifetime only: insertion/reinsertion and map-order recovery are separate.
 * Removal of the current object preserves the remaining supplied traversal.
 */
bool ab_block_global_event(AbBlockEventObject *const *objects, size_t object_count,
                           AbSpan event, const AbDataValue *arguments, size_t count,
                           AbBlockEventHost host, AbError *error);

#endif
