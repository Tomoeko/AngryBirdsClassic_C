#ifndef AB_Z_ORDER_H
#define AB_Z_ORDER_H

#include "ab/base.h"

typedef struct {
    float z;
    uintptr_t identity;
} AbZOrderEntry;

/* Reproduce the pinned numeric ascending table.sort permutation, including its
 * equal-key swaps. Entries own no resources. Immutable finite binary32 keys and
 * dense arrays only; arbitrary comparator callbacks/alias mutations are outside
 * this boundary. Validate before mutation, use bounded stack workspace and make
 * no allocations. Identity words are preserved without interpretation. */
bool ab_z_order_sort(AbZOrderEntry *entries, size_t count, AbError *error);

#endif
