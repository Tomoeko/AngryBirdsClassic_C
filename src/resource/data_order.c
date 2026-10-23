#include "ab/data_order.h"

#include <string.h>

static uint32_t string_hash(AbSpan string) {
    uint32_t hash = (uint32_t)string.size;
    size_t stride = (string.size >> 5) + 1;
    for (size_t offset = string.size; offset >= stride; offset -= stride) {
        hash ^= (hash << 5) + (hash >> 2) + string.data[offset - 1];
    }
    return hash;
}

static size_t main_slot(const AbDataTable *table, size_t entry, size_t capacity) {
    return string_hash(table->entries[entry].key.string) & (capacity - 1);
}

static bool same_string(AbSpan first, AbSpan second) {
    return first.size == second.size &&
           (!first.size || memcmp(first.data, second.data, first.size) == 0);
}

static bool insert_entry(const AbDataTable *table, size_t entry, size_t capacity,
                         AbStringOrderNode *nodes, size_t *last_free) {
    size_t slot = main_slot(table, entry, capacity);
    for (size_t current = slot; current != SIZE_MAX; current = nodes[current].next) {
        if (nodes[current].entry != SIZE_MAX &&
            same_string(table->entries[entry].key.string,
                        table->entries[nodes[current].entry].key.string)) {
            return false;
        }
    }
    if (nodes[slot].entry != SIZE_MAX) {
        size_t free_slot = SIZE_MAX;
        while (*last_free) {
            size_t candidate = --*last_free;
            if (nodes[candidate].entry == SIZE_MAX) {
                free_slot = candidate;
                break;
            }
        }
        if (free_slot == SIZE_MAX) {
            return false;
        }
        size_t origin = main_slot(table, nodes[slot].entry, capacity);
        if (origin != slot) {
            size_t previous = origin;
            while (nodes[previous].next != slot) {
                previous = nodes[previous].next;
                if (previous == SIZE_MAX) {
                    return false;
                }
            }
            nodes[previous].next = free_slot;
            nodes[free_slot] = nodes[slot];
            nodes[slot] = (AbStringOrderNode){SIZE_MAX, SIZE_MAX};
        } else {
            nodes[free_slot].next = nodes[slot].next;
            nodes[slot].next = free_slot;
            slot = free_slot;
        }
    }
    nodes[slot].entry = entry;
    return true;
}

static bool valid_storage(const AbDataTable *table, size_t hash_capacity,
                          const AbStringOrderWorkspace *workspace, bool allow_nil,
                          AbError *error) {
    if (!table || !workspace || table->count > table->capacity ||
        (table->count && (!table->entries || !workspace->indices)) ||
        table->count > workspace->index_capacity || table->count > hash_capacity ||
        hash_capacity > UINT32_MAX || (hash_capacity & (hash_capacity - 1)) ||
        hash_capacity > workspace->node_capacity ||
        hash_capacity > SIZE_MAX / sizeof(*workspace->nodes) ||
        (hash_capacity && !workspace->nodes)) {
        return ab_fail(error, "Invalid native string-order storage");
    }
    for (size_t index = 0; index < table->count; ++index) {
        const AbDataEntry *entry = table->entries + index;
        if (entry->key.kind != AB_DATA_STRING || entry->key.string.size > UINT32_MAX ||
            (entry->key.string.size && !entry->key.string.data) ||
            (!allow_nil && entry->value.kind == AB_DATA_NIL)) {
            return ab_fail(error,
                           "String ordering requires unique non-nil string keys");
        }
    }
    return true;
}

static void clear_nodes(AbStringOrderNode *nodes, size_t capacity) {
    for (size_t slot = 0; slot < capacity; ++slot) {
        nodes[slot] = (AbStringOrderNode){SIZE_MAX, SIZE_MAX};
    }
}

static void write_indices(const AbStringOrderNode *nodes, size_t capacity,
                          size_t *indices) {
    size_t count = 0;
    for (size_t slot = 0; slot < capacity; ++slot) {
        if (nodes[slot].entry != SIZE_MAX) {
            indices[count++] = nodes[slot].entry;
        }
    }
}

bool ab_data_string_order(const AbDataTable *table, size_t hash_capacity,
                          AbStringOrderWorkspace *workspace, AbError *error) {
    if (!valid_storage(table, hash_capacity, workspace, false, error)) {
        return false;
    }
    clear_nodes(workspace->nodes, hash_capacity);
    size_t last_free = hash_capacity;
    for (size_t entry = 0; entry < table->count; ++entry) {
        if (!insert_entry(table, entry, hash_capacity, workspace->nodes, &last_free)) {
            return ab_fail(error, "Duplicate key or unsupported native string layout");
        }
    }
    write_indices(workspace->nodes, hash_capacity, workspace->indices);
    return true;
}

bool ab_data_string_order_pruned(const AbDataTable *table, size_t hash_capacity,
                                 AbStringOrderWorkspace *workspace, size_t *count,
                                 AbError *error) {
    if (!count || !valid_storage(table, hash_capacity, workspace, true, error)) {
        return ab_fail(error, "Invalid pruned string-order storage");
    }
    clear_nodes(workspace->nodes, hash_capacity);
    size_t last_free = hash_capacity;
    for (size_t entry = 0; entry < table->count; ++entry) {
        if (!insert_entry(table, entry, hash_capacity, workspace->nodes, &last_free)) {
            return ab_fail(error, "Duplicate key or unsupported pruned string layout");
        }
    }
    size_t live = 0;
    for (size_t slot = 0; slot < hash_capacity; ++slot) {
        size_t entry = workspace->nodes[slot].entry;
        if (entry != SIZE_MAX && table->entries[entry].value.kind != AB_DATA_NIL) {
            workspace->indices[live++] = entry;
        }
    }
    *count = live;
    return true;
}

bool ab_data_string_order_growing(const AbDataTable *table,
                                  AbStringOrderWorkspace *workspace,
                                  size_t *final_capacity, AbError *error) {
    if (!table || !final_capacity || table->count > UINT32_C(0x80000000)) {
        return ab_fail(error, "Invalid growing native string table");
    }
    size_t maximum = table->count ? 1 : 0;
    while (maximum < table->count) {
        maximum *= 2;
    }
    if (!valid_storage(table, maximum, workspace, false, error)) {
        return false;
    }
    size_t capacity = 0, last_free = 0;
    for (size_t entry = 0; entry < table->count; ++entry) {
        if (entry == capacity) {
            size_t old_count = 0;
            for (size_t cursor = capacity; cursor > 0;) {
                size_t previous = workspace->nodes[--cursor].entry;
                if (previous != SIZE_MAX) {
                    workspace->indices[old_count++] = previous;
                }
            }
            capacity = capacity ? capacity * 2 : 1;
            clear_nodes(workspace->nodes, capacity);
            last_free = capacity;
            for (size_t cursor = 0; cursor < old_count; ++cursor) {
                if (!insert_entry(table, workspace->indices[cursor], capacity,
                                  workspace->nodes, &last_free)) {
                    return ab_fail(error, "Unsupported growing native string rehash");
                }
            }
        }
        if (!insert_entry(table, entry, capacity, workspace->nodes, &last_free)) {
            return ab_fail(error, "Duplicate key or unsupported growing string layout");
        }
    }
    write_indices(workspace->nodes, capacity, workspace->indices);
    *final_capacity = capacity;
    return true;
}
