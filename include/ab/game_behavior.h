#ifndef AB_GAME_BEHAVIOR_H
#define AB_GAME_BEHAVIOR_H

#include "ab/string_map.h"
#include "ab/ui_pointer.h"
#include "ab/block_collision.h"
#include "ab/lua_data.h"

typedef struct {
    AbSpan event;
    bool truthy;
    void *context;
    /* Receiver is bound in context; return status is not a script return value.
     * A truthy binding without a callable rejects when reached. */
    bool (*call)(void *context, const AbUiPointerValue *arguments, size_t count,
                 AbError *error);
} AbGameBehaviorMethod;

typedef struct {
    AbGameBehaviorMethod *methods;
    size_t count;
} AbGameBehavior;

typedef struct {
    AbGameBehavior **items;
    size_t capacity;
} AbGameBehaviorGroup;

typedef struct {
    AbStringMap *groups;
    AbDataValue receiver;
    AbGameBehaviorGroup *new_groups;
    size_t group_count, group_capacity;
    void *context;
    bool (*create)(void *context, AbSpan name, AbDataValue receiver,
                   AbDataValue properties, AbGameBehavior **output, AbError *error);
} AbGameBehaviorRegistration;

/* GameObject.addBehavior with a resolved factory returning one owned behavior
 * instance and ordinary dense groups. A missing/falsy group consumes the next
 * caller-prepared empty group and publishes it before invoking the factory.
 * Cache the current group before creation, then append after creation using its
 * current dense length. Factory effects and a published empty group survive
 * later failure. A present zero map handle represents a falsy group.
 *
 * All storage, key bytes, receiver/property data and returned instances stay
 * borrowed and stable. The factory may replace the group's map binding or
 * registration.groups, or append within a cached group's capacity; appending
 * still uses the original cached group. Keep that group alive. New group rows
 * have unique, disjoint, NULL-filled item arrays. No allocation or VM. Nil,
 * scalar/multiple factory returns, sparse/mixed-key groups, metatables and
 * dynamic class discovery are outside this typed construction boundary.
 * Retained nil-key collection is an explicit caller-controlled map phase. */
bool ab_game_object_add_behavior(AbGameBehaviorRegistration *registration, AbSpan name,
                                 AbDataValue properties, AbError *error);

/* GameObject.delegateFunctionCall over a plain string-keyed behaviors table.
 * Group map values are borrowed AbGameBehaviorGroup pointers. Traverse native
 * string-map order, then each cached group in ascending slot order, stopping at
 * its first NULL item. A missing/falsy event binding skips that behavior;
 * otherwise invoke with that behavior as receiver and all arguments, including
 * interior/trailing nil slots. Script return values are ignored.
 *
 * Storage, names and arguments remain borrowed. Bindings have unique event keys.
 * Callbacks may replace later bindings/items, append within group capacity, or
 * replace/remove existing group-map values. A replaced current group does not
 * replace the cached inner traversal. Do not insert/reinsert map keys, resize
 * storage, release cached groups/behaviors, or reenter. Mixed-key groups,
 * metatables, custom pairs/ipairs, group-key insertion and GC remain external.
 * No allocation, Lua or gameplay effects. Failure retains preceding callbacks.
 */
bool ab_game_object_delegate(const AbStringMap *groups, AbSpan event,
                             const AbUiPointerValue *arguments, size_t count,
                             AbError *error);

/* Bind these directly as bird/block notification methods; context is the
 * borrowed group map. Object arguments use the actual AbBlockCollisionObject
 * pointer as an opaque token. Both vector presence flags preserve nil slots.
 * Destroy delegates with no arguments. These compose the dispatcher, without
 * interpreting or applying any particular behavior's effects. */
bool ab_game_object_bird_collision(void *context, const AbBlockCollisionNotice *notice,
                                   AbError *error);
bool ab_game_object_block_collision(void *context, const AbBlockCollisionNotice *notice,
                                    AbError *error);
bool ab_game_object_destroy(void *context, AbError *error);

#endif
