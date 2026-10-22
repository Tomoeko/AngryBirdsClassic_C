#ifndef AB_PHYSICS_TREE_H
#define AB_PHYSICS_TREE_H

#include "ab/base.h"
#include "ab/physics_geometry.h"

typedef struct {
    AbAabb bounds;
    uintptr_t payload;
    int32_t parent;
    int32_t children[2];
    int32_t height;
} AbTreeNode;

typedef struct {
    AbTreeNode *nodes;
    int32_t root;
    int32_t count;
    int32_t capacity;
    int32_t free_head;
    uint32_t insertion_count;
    size_t maximum_capacity;
} AbDynamicTree;

typedef bool (*AbTreeQuery)(void *context, int32_t proxy, uintptr_t payload);

/* Return the next clipping fraction through fraction: zero stops successfully,
 * positive values replace the current limit and negative/NaN values ignore it.
 * The callback status is an explicit host failure, preserving prior effects. */
typedef bool (*AbTreeRaycast)(void *context, int32_t proxy, uintptr_t payload,
                              AbRay current, float *fraction, AbError *error);

/* Tree owns its pool, starting with 16 nodes and growing by doubling. The limit
 * bounds node capacity, including internal nodes. Treat fields as read-only.
 * Proxy IDs are reused after removal. Bounds and displacement are finite, with
 * finite intermediate arithmetic and ordered lower/upper coordinates.
 * Output pointers cannot alias the tree or its node storage.
 */
bool ab_tree_open(size_t maximum_capacity, AbDynamicTree *tree, AbError *error);
void ab_tree_close(AbDynamicTree *tree);
bool ab_tree_create(AbDynamicTree *tree, AbAabb bounds, uintptr_t payload,
                    int32_t *proxy, AbError *error);
bool ab_tree_destroy(AbDynamicTree *tree, int32_t proxy);
bool ab_tree_move(AbDynamicTree *tree, int32_t proxy, AbAabb bounds,
                  AbVec2 displacement, bool *moved);
bool ab_tree_shift(AbDynamicTree *tree, AbVec2 origin);
/* The callback may stop traversal by returning false; this is a successful
 * query. It must not mutate the tree. Querying performs no allocation.
 */
bool ab_tree_query(const AbDynamicTree *tree, AbAabb bounds, AbTreeQuery callback,
                   void *context, AbError *error);

/* Recovered native ray traversal with segment bounds, perpendicular separation
 * and child-one-before-child-zero stack order. Endpoints may contain any
 * binary32 value; preserve unordered comparisons and overflow rather than
 * substituting bounds. The initial fraction is finite and nonnegative. Degenerate
 * directions retain the native small-length normalization behavior. Callback
 * bindings/storage remain stable; never mutate the tree or reenter from a host.
 * The fixed stack holds 256 entries, matching the native inline allocation;
 * larger traversal depth rejects instead of growing it. No allocation. Optional
 * final_fraction commits only on success; host effects remain on failure. */
bool ab_tree_raycast(const AbDynamicTree *tree, AbRay ray, AbTreeRaycast callback,
                     void *context, float *final_fraction, AbError *error);

#endif
