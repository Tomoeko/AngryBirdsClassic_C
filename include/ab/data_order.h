#ifndef AB_DATA_ORDER_H
#define AB_DATA_ORDER_H

#include "ab/lua_data.h"

typedef struct {
    size_t entry;
    size_t next;
} AbStringOrderNode;

typedef struct {
    size_t *indices;
    size_t index_capacity;
    AbStringOrderNode *nodes;
    size_t node_capacity;
} AbStringOrderWorkspace;

/* Native string-table iteration order for unique, non-nil entries inserted
 * once in the supplied sequence into an already sized hash part. Capacity is
 * the measured constructor's power-of-two node count, not AbDataTable.capacity.
 * No array keys, growth, deletion, or reinsertion are modeled. Caller storage
 * is borrowed and disjoint from the input and other arrays. No allocation
 * occurs. Failure preserves indices; scratch nodes may change. An empty table
 * accepts zero capacity and empty workspace arrays.
 */
bool ab_data_string_order(const AbDataTable *table, size_t hash_capacity,
                          AbStringOrderWorkspace *workspace, AbError *error);

/* The same constructor layout after nil writes to existing entries. Rebuild
 * every original key, including deleted ones, then emit only live values.
 * The caller retains the complete original insertion sequence and capacity;
 * no later insertion, growth, collection or reinsertion is modeled. count and
 * indices change only on success. This models level onLoad pruning without
 * compacting the hash layout or changing surviving iteration order. */
bool ab_data_string_order_pruned(const AbDataTable *table, size_t hash_capacity,
                                 AbStringOrderWorkspace *workspace, size_t *count,
                                 AbError *error);

/* The same unique string insertions starting from an empty hash part, growing
 * at native power-of-two boundaries. Rehash old physical slots in reverse
 * order. Scratch nodes cover the final power-of-two capacity and indices cover
 * table.count. Indices are temporary rehash storage and may change on failure;
 * final_capacity changes only on success and must be disjoint from scratch.
 * No arrays, deletions or allocation.
 */
bool ab_data_string_order_growing(const AbDataTable *table,
                                  AbStringOrderWorkspace *workspace,
                                  size_t *final_capacity, AbError *error);

#endif
