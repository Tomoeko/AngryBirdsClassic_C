#ifndef AB_INTEGER_ORDER_H
#define AB_INTEGER_ORDER_H

#include "ab/base.h"

typedef struct {
    size_t entry;
    size_t next;
} AbIntegerOrderNode;

typedef struct {
    size_t *indices;
    size_t index_capacity;
    size_t *array_slots;
    size_t array_capacity;
    AbIntegerOrderNode *nodes;
    size_t node_capacity;
} AbIntegerOrderWorkspace;

typedef struct {
    size_t array_capacity;
    size_t hash_capacity;
} AbIntegerOrderLayout;

/* Iterate a fresh, initially unhinted native numeric-key table after unique
 * non-nil insertions in supplied order. Keys are signed integer IDs exactly
 * representable in binary32, within [-16777216,16777216]. Return input indices
 * in array-then-physical-hash order and both resulting capacities. No original
 * interpreter, allocation, deletions, nil writes, or mixed/string key support.
 * All workspace arrays are caller-owned, disjoint and may change on failure.
 * indices covers count; array_slots/nodes each cover the next power of two of
 * count. Empty input accepts zero storage. Layout changes only on success.
 * Original fresh deep copies can apply this helper again to the previous order;
 * source-table construction, arbitrary IDs and surrounding input remain separate. */
bool ab_integer_order_growing(const int32_t *keys, size_t count,
                              AbIntegerOrderWorkspace *workspace,
                              AbIntegerOrderLayout *layout, AbError *error);

#endif
