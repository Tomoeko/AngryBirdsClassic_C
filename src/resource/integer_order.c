#include "ab/integer_order.h"

#include <string.h>

typedef struct {
    const int32_t *keys;
    AbIntegerOrderWorkspace *workspace;
    size_t array_capacity;
    size_t hash_capacity;
    size_t last_free;
} IntegerOrder;

static size_t hash_slot(int32_t key, size_t capacity) {
    if (!key) {
        return 0;
    }
    float number = (float)key;
    uint32_t word;
    memcpy(&word, &number, sizeof(word));
    /* Target float-key hashing divides its representation by an odd modulus;
     * it does not mask the integer ID. Zero has a separate first-slot path. */
    return word % ((capacity - 1) | 1);
}

static bool array_key(int32_t key, size_t capacity) {
    return key > 0 && (size_t)key <= capacity;
}

static bool insert_hash(IntegerOrder *state, size_t entry) {
    if (!state->hash_capacity) {
        return false;
    }
    AbIntegerOrderNode *nodes = state->workspace->nodes;
    size_t slot = hash_slot(state->keys[entry], state->hash_capacity);
    if (nodes[slot].entry != SIZE_MAX) {
        size_t free_slot = SIZE_MAX;
        while (state->last_free) {
            size_t candidate = --state->last_free;
            if (nodes[candidate].entry == SIZE_MAX) {
                free_slot = candidate;
                break;
            }
        }
        if (free_slot == SIZE_MAX) {
            return false;
        }
        size_t origin = hash_slot(state->keys[nodes[slot].entry], state->hash_capacity);
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
            nodes[slot] = (AbIntegerOrderNode){SIZE_MAX, SIZE_MAX};
        } else {
            nodes[free_slot].next = nodes[slot].next;
            nodes[slot].next = free_slot;
            slot = free_slot;
        }
    }
    nodes[slot].entry = entry;
    return true;
}

static size_t power_capacity(size_t count) {
    size_t capacity = count ? 1 : 0;
    while (capacity < count) {
        capacity *= 2;
    }
    return capacity;
}

static void array_selection(const int32_t *keys, size_t count, size_t *capacity,
                            size_t *members) {
    *capacity = *members = 0;
    size_t maximum = power_capacity(count);
    for (size_t size = 1; size <= maximum; size *= 2) {
        size_t occupied = 0;
        for (size_t entry = 0; entry < count; ++entry) {
            occupied += array_key(keys[entry], size);
        }
        if (occupied > size / 2) {
            *capacity = size;
            *members = occupied;
        }
    }
}

static bool resize(IntegerOrder *state, size_t count) {
    size_t capacity, members;
    array_selection(state->keys, count, &capacity, &members);
    size_t hash_capacity = power_capacity(count - members);
    if (capacity < state->array_capacity ||
        capacity > state->workspace->array_capacity ||
        hash_capacity > state->workspace->node_capacity) {
        return false;
    }
    size_t saved = 0;
    /* Original rehash visits old physical hash slots from last to first. */
    for (size_t cursor = state->hash_capacity; cursor > 0;) {
        size_t entry = state->workspace->nodes[--cursor].entry;
        if (entry != SIZE_MAX) {
            state->workspace->indices[saved++] = entry;
        }
    }
    for (size_t slot = state->array_capacity; slot < capacity; ++slot) {
        state->workspace->array_slots[slot] = SIZE_MAX;
    }
    for (size_t slot = 0; slot < hash_capacity; ++slot) {
        state->workspace->nodes[slot] = (AbIntegerOrderNode){SIZE_MAX, SIZE_MAX};
    }
    state->array_capacity = capacity;
    state->hash_capacity = hash_capacity;
    state->last_free = hash_capacity;
    for (size_t index = 0; index < saved; ++index) {
        size_t entry = state->workspace->indices[index];
        int32_t key = state->keys[entry];
        if (array_key(key, capacity)) {
            state->workspace->array_slots[(size_t)key - 1] = entry;
        } else if (!insert_hash(state, entry)) {
            return false;
        }
    }
    return true;
}

static bool valid_storage(const int32_t *keys, size_t count,
                          const AbIntegerOrderWorkspace *workspace,
                          const AbIntegerOrderLayout *layout) {
    if (!workspace || !layout || count > UINT32_C(0x1000000) ||
        count > workspace->index_capacity ||
        (count && (!keys || !workspace->indices))) {
        return false;
    }
    size_t maximum = power_capacity(count);
    if (maximum > workspace->array_capacity || maximum > workspace->node_capacity ||
        (maximum && (!workspace->array_slots || !workspace->nodes))) {
        return false;
    }
    for (size_t entry = 0; entry < count; ++entry) {
        if (keys[entry] < -16777216 || keys[entry] > 16777216) {
            return false;
        }
        for (size_t previous = 0; previous < entry; ++previous) {
            if (keys[previous] == keys[entry]) {
                return false;
            }
        }
    }
    return true;
}

bool ab_integer_order_growing(const int32_t *keys, size_t count,
                              AbIntegerOrderWorkspace *workspace,
                              AbIntegerOrderLayout *layout, AbError *error) {
    if (!valid_storage(keys, count, workspace, layout)) {
        return ab_fail(error, "Invalid fresh integer-order keys or workspace");
    }
    IntegerOrder state = {.keys = keys, .workspace = workspace};
    for (size_t entry = 0; entry < count; ++entry) {
        int32_t key = keys[entry];
        if (array_key(key, state.array_capacity)) {
            workspace->array_slots[(size_t)key - 1] = entry;
            continue;
        }
        if (insert_hash(&state, entry)) {
            continue;
        }
        if (!resize(&state, entry + 1)) {
            return ab_fail(error, "Cannot rebuild fresh integer-order storage");
        }
        if (array_key(key, state.array_capacity)) {
            workspace->array_slots[(size_t)key - 1] = entry;
        } else if (!insert_hash(&state, entry)) {
            return ab_fail(error, "Cannot insert fresh integer-order key");
        }
    }
    size_t written = 0;
    for (size_t slot = 0; slot < state.array_capacity; ++slot) {
        if (workspace->array_slots[slot] != SIZE_MAX) {
            workspace->indices[written++] = workspace->array_slots[slot];
        }
    }
    for (size_t slot = 0; slot < state.hash_capacity; ++slot) {
        if (workspace->nodes[slot].entry != SIZE_MAX) {
            workspace->indices[written++] = workspace->nodes[slot].entry;
        }
    }
    if (written != count) {
        return ab_fail(error, "Fresh integer-order member count changed");
    }
    *layout = (AbIntegerOrderLayout){state.array_capacity, state.hash_capacity};
    return true;
}
