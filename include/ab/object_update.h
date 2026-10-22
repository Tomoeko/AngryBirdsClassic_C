#ifndef AB_OBJECT_UPDATE_H
#define AB_OBJECT_UPDATE_H

#include "ab/base.h"

typedef struct {
    void *context;
    bool (*invoke)(void *context, void *object, float seconds, AbError *error);
} AbObjectUpdateFunction;

typedef struct {
    const AbObjectUpdateFunction **items;
    size_t count, capacity;
} AbObjectUpdateList;

typedef enum {
    AB_OBJECT_UPDATE_NONE,
    AB_OBJECT_UPDATE_FUNCTION,
    AB_OBJECT_UPDATE_LIST,
} AbObjectUpdateKind;

typedef struct {
    AbObjectUpdateKind kind;
    union {
        const AbObjectUpdateFunction *function;
        AbObjectUpdateList *list;
    };
} AbObjectUpdateValue;

typedef struct {
    AbObjectUpdateValue object;
    AbObjectUpdateValue registered;
} AbObjectUpdates;

/* Resolved addObjectUpdateFunction for one object/name binding. Function object
 * addresses are stable identities; callbacks and all list storage are borrowed.
 * A first addition stays a single function. Adding to a single function creates
 * a callable two-entry list, even for the same identity. A later duplicate list
 * addition fails as the original assertion does. Promotion needs a distinct empty list
 * with room for both entries; unused promotion storage is ignored. Refresh the
 * registered value after every successful add. No allocation or Lua.
 * Only callable function identities and dense lists are supported. Storage is
 * disjoint and remains alive while object/registered/captured call values refer
 * to it; do not reuse an old list during its invocation. Failure preserves state.
 */
bool ab_object_update_add(AbObjectUpdates *state,
                          const AbObjectUpdateFunction *function,
                          AbObjectUpdateList *promotion, AbError *error);

/* Remove the first matching list entry. Collapse a one-entry object property
 * back to its function, while preserving the registered list alias. Removing
 * the current single function clears both values. A different/absent single
 * function fails as the original assertion does; an absent list member is a
 * no-op. Clear drops both bindings without changing retained list contents.
 */
bool ab_object_update_remove(AbObjectUpdates *state,
                             const AbObjectUpdateFunction *function, AbError *error);
bool ab_object_update_clear(AbObjectUpdates *state, AbError *error);
bool ab_object_update_has(const AbObjectUpdates *state,
                          const AbObjectUpdateFunction *function, bool *output,
                          AbError *error);

/* Invoke a captured callable value with object and seconds only. A list reads
 * its next dense slot after each callback: bounded append/removal is observable.
 * Changing the object's property/registry does not replace this captured value.
 * Callback failure retains changes and stops dispatch. Numeric bits are passed
 * unchanged. NONE rejects; world/name traversal and pruning are separate.
 */
bool ab_object_update_call(AbObjectUpdateValue value, void *object, float seconds,
                           AbError *error);

#endif
