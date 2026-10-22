#ifndef AB_PHYSICS_BROADPHASE_H
#define AB_PHYSICS_BROADPHASE_H

#include "ab/physics_tree.h"

typedef struct {
    int32_t first;
    int32_t second;
} AbBroadPair;

typedef struct {
    size_t nodes;
    size_t moves;
    size_t pairs;
} AbBroadLimits;

typedef struct {
    AbDynamicTree tree;
    int32_t *moves;
    AbBroadPair *pairs;
    size_t proxy_count;
    size_t move_capacity;
    size_t move_count;
    size_t pair_capacity;
    size_t pair_count;
    int32_t query_proxy;
    AbBroadLimits limits;
} AbBroadPhase;

typedef void (*AbBroadCallback)(void *context, AbBroadPair pair,
                                uintptr_t first_payload, uintptr_t second_payload);

/* Owns the tree and both buffers. Capacities begin at 16 and double within
 * explicit limits. Treat fields as read-only. Tree numeric contracts apply.
 * Failed operations preserve existing proxies; auxiliary capacities may grow.
 * Output pointers cannot alias the phase or its owned buffers.
 */
bool ab_broadphase_open(AbBroadLimits limits, AbBroadPhase *phase, AbError *error);
void ab_broadphase_close(AbBroadPhase *phase);
bool ab_broadphase_create(AbBroadPhase *phase, AbAabb bounds, uintptr_t payload,
                          int32_t *proxy, AbError *error);
bool ab_broadphase_move(AbBroadPhase *phase, int32_t proxy, AbAabb bounds,
                        AbVec2 displacement, AbError *error);
bool ab_broadphase_touch(AbBroadPhase *phase, int32_t proxy, AbError *error);
bool ab_broadphase_destroy(AbBroadPhase *phase, int32_t proxy);
bool ab_broadphase_shift(AbBroadPhase *phase, AbVec2 origin);
/* Preserve duplicate move entries and removal tombstones. Collect and sort
 * canonical proxy pairs, then emit each unique pair once in proxy order.
 * Callbacks must not mutate the phase. On collection failure no callbacks run;
 * moves are retained and partial pairs are discarded on the next update.
 */
bool ab_broadphase_update(AbBroadPhase *phase, AbBroadCallback callback, void *context,
                          AbError *error);

#endif
