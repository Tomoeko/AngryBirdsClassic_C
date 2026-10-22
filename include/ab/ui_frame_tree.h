#ifndef AB_UI_FRAME_TREE_H
#define AB_UI_FRAME_TREE_H

#include "ab/ui_init.h"

typedef enum {
    AB_UI_FRAME_ENTRY,
    AB_UI_FRAME_EXIT,
    AB_UI_FRAME_LAYOUT,
} AbUiFrameMethod;

typedef struct AbUiFrameNode AbUiFrameNode;

struct AbUiFrameNode {
    AbUiFrameState *state;
    AbUiFrameNode *parent;
    AbUiFrameNode **children;
    size_t capacity;
    AbUiPointerValue entered;
    void *context;
    bool (*method)(void *context, AbUiFrameNode *node, AbUiFrameMethod method,
                   AbError *error);
};

typedef struct {
    AbUiPointerValue entry_counts;
} AbUiFrameLifecycle;

/* Borrowed original Frame child/list/lookup value model. state child_count and
 * lookup_count describe the dense array; both must agree and fit capacity/2^24.
 * Stable text names are unique within each parent. Lookup indices are derived
 * from dense order; custom table keys/metatables and externally renamed attached
 * children are outside this contract. No allocation or ownership transfer.
 * All arrays/nodes/text remain caller-owned and stable during callbacks. A
 * callback may change entered fields and child subtrees, but not the parent
 * array currently being traversed. Invalid capacity/name/index/duplicate input
 * is rejected before mutation; original malformed-table/assert prefixes are
 * outside this model. Successful prefixes remain when a callback fails. */

/* Original addChild. An absent position appends; a present position is one-based
 * in [1,count+1]. Attach/update lookup order, assign child's parent, then when
 * parent entered is truthy call child entry followed by layout. Never remove
 * the child from an older parent's array. */
bool ab_ui_frame_add_child(AbUiFrameNode *parent, AbUiFrameNode *child, size_t position,
                           bool position_present, AbError *error);

/* Remove by exact name and identity, shift order, then call child exit only
 * when its entered value is truthy. Preserve the child's parent field. Clear
 * calls every child's exit only when the parent entered value is truthy, then
 * resets both logical counts; it also preserves every parent field. */
bool ab_ui_frame_remove_child(AbUiFrameNode *parent, AbUiFrameNode *child,
                              AbError *error);
bool ab_ui_frame_clear_children(AbUiFrameNode *parent, AbError *error);
bool ab_ui_frame_remove_self(AbUiFrameNode *node, AbError *error);
bool ab_ui_frame_pop_child(AbUiFrameNode *parent, AbUiFrameNode **output,
                           AbError *error);

/* Original getChild's bounded path through stable text names. A missing name
 * succeeds with NULL. At least one name is required; no recursion/allocation.
 * Failure preserves output. Direct node.parent retains original stale links. */
bool ab_ui_frame_find_child(AbUiFrameNode *parent, const AbSpan *names, size_t count,
                            AbUiFrameNode **output, AbError *error);

/* Base onEntry/onExit/layout delegate forwards without visibility/activity
 * gates. Entry rejects truthy entered, increments the shared entryCounts then
 * sets entered true before child calls. Exit always decrements and sets false,
 * including repeated exits. Absent/false counter defaults to zero; otherwise
 * require finite numeric input/result. Child methods are explicit boundaries,
 * allowing the caller to compose base methods or recovered subclass behavior. */
bool ab_ui_frame_enter(AbUiFrameNode *node, AbUiFrameLifecycle *lifecycle,
                       AbError *error);
bool ab_ui_frame_exit(AbUiFrameNode *node, AbUiFrameLifecycle *lifecycle,
                      AbError *error);
bool ab_ui_frame_layout_children(AbUiFrameNode *node, AbError *error);

#endif
