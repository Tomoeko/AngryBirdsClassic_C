#include "ab/physics_broadphase.h"

#include "broadphase_internal.h"
#include "tree_internal.h"

#include <limits.h>
#include <math.h>
#include <stdlib.h>

bool ab_broadphase_valid(const AbBroadPhase *phase) {
    return phase && phase->tree.nodes && phase->moves && phase->pairs;
}

void ab_broadphase_close(AbBroadPhase *phase) {
    if (phase) {
        ab_tree_close(&phase->tree);
        free(phase->moves);
        free(phase->pairs);
        *phase = (AbBroadPhase){0};
    }
}

bool ab_broadphase_open(AbBroadLimits limits, AbBroadPhase *phase, AbError *error) {
    if (!phase) {
        return ab_fail(error, "Missing broadphase output");
    }
    *phase = (AbBroadPhase){0};
    if (limits.moves < 16 || limits.pairs < 16 || limits.moves > INT32_MAX ||
        limits.pairs > INT32_MAX || limits.moves > SIZE_MAX / sizeof(*phase->moves) ||
        limits.pairs > SIZE_MAX / sizeof(*phase->pairs)) {
        return ab_fail(error, "Invalid broadphase buffer limits");
    }
    if (!ab_tree_open(limits.nodes, &phase->tree, error)) {
        return false;
    }
    phase->moves = malloc(16 * sizeof(*phase->moves));
    phase->pairs = malloc(16 * sizeof(*phase->pairs));
    if (!phase->moves || !phase->pairs) {
        ab_broadphase_close(phase);
        return ab_fail(error, "Cannot allocate broadphase buffers");
    }
    phase->move_capacity = 16;
    phase->pair_capacity = 16;
    phase->limits = limits;
    return true;
}

static void *grow_buffer(void *buffer, size_t *capacity, size_t count, size_t limit,
                         size_t element_size, AbError *error) {
    if (count < *capacity) {
        return buffer;
    }
    if (*capacity > limit / 2) {
        ab_fail(error, "Broadphase buffer capacity limit reached");
        return NULL;
    }
    size_t grown_capacity = *capacity * 2;
    void *grown = realloc(buffer, grown_capacity * element_size);
    if (!grown) {
        ab_fail(error, "Cannot grow broadphase buffer");
        return NULL;
    }
    *capacity = grown_capacity;
    return grown;
}

static bool reserve_move(AbBroadPhase *phase, AbError *error) {
    void *grown = grow_buffer(phase->moves, &phase->move_capacity, phase->move_count,
                              phase->limits.moves, sizeof(*phase->moves), error);
    if (!grown) {
        return false;
    }
    phase->moves = grown;
    return true;
}

bool ab_broadphase_reserve_pairs(AbBroadPhase *phase, AbError *error) {
    void *grown = grow_buffer(phase->pairs, &phase->pair_capacity, phase->pair_count,
                              phase->limits.pairs, sizeof(*phase->pairs), error);
    if (!grown) {
        return false;
    }
    phase->pairs = grown;
    return true;
}

bool ab_broadphase_create(AbBroadPhase *phase, AbAabb bounds, uintptr_t payload,
                          int32_t *proxy, AbError *error) {
    if (!ab_broadphase_valid(phase) || !proxy || !ab_tree_valid_bounds(bounds)) {
        return ab_fail(error, "Invalid broadphase proxy input");
    }
    if (!reserve_move(phase, error) ||
        !ab_tree_create(&phase->tree, bounds, payload, proxy, error)) {
        return false;
    }
    ++phase->proxy_count;
    phase->moves[phase->move_count++] = *proxy;
    return true;
}

bool ab_broadphase_move(AbBroadPhase *phase, int32_t proxy, AbAabb bounds,
                        AbVec2 displacement, AbError *error) {
    if (!ab_broadphase_valid(phase) || !ab_tree_valid_proxy(&phase->tree, proxy) ||
        !ab_tree_valid_bounds(bounds) || !isfinite(displacement.x) ||
        !isfinite(displacement.y)) {
        return ab_fail(error, "Invalid broadphase move input");
    }
    AbAabb previous = phase->tree.nodes[proxy].bounds;
    if (previous.lower.x <= bounds.lower.x && previous.lower.y <= bounds.lower.y &&
        bounds.upper.x <= previous.upper.x && bounds.upper.y <= previous.upper.y) {
        return true;
    }
    if (!reserve_move(phase, error)) {
        return false;
    }
    bool moved;
    if (!ab_tree_move(&phase->tree, proxy, bounds, displacement, &moved)) {
        return ab_fail(error, "Cannot move broadphase proxy");
    }
    if (moved) {
        phase->moves[phase->move_count++] = proxy;
    }
    return true;
}

bool ab_broadphase_touch(AbBroadPhase *phase, int32_t proxy, AbError *error) {
    if (!ab_broadphase_valid(phase) || !ab_tree_valid_proxy(&phase->tree, proxy)) {
        return ab_fail(error, "Invalid broadphase touch input");
    }
    if (!reserve_move(phase, error)) {
        return false;
    }
    phase->moves[phase->move_count++] = proxy;
    return true;
}

bool ab_broadphase_destroy(AbBroadPhase *phase, int32_t proxy) {
    if (!ab_broadphase_valid(phase) || !ab_tree_valid_proxy(&phase->tree, proxy)) {
        return false;
    }
    for (size_t index = 0; index < phase->move_count; ++index) {
        if (phase->moves[index] == proxy) {
            phase->moves[index] = -1;
        }
    }
    if (!ab_tree_destroy(&phase->tree, proxy)) {
        return false;
    }
    --phase->proxy_count;
    return true;
}

bool ab_broadphase_shift(AbBroadPhase *phase, AbVec2 origin) {
    return ab_broadphase_valid(phase) && ab_tree_shift(&phase->tree, origin);
}
