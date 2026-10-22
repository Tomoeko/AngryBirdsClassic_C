#include "broadphase_internal.h"

typedef struct {
    AbBroadPhase *phase;
    AbError *error;
    bool okay;
} PairCollector;

static bool collect_pair(void *context, int32_t proxy, uintptr_t payload) {
    (void)payload;
    PairCollector *collector = context;
    AbBroadPhase *phase = collector->phase;
    if (proxy == phase->query_proxy) {
        return true;
    }
    if (!ab_broadphase_reserve_pairs(phase, collector->error)) {
        collector->okay = false;
        return false;
    }
    AbBroadPair pair = proxy < phase->query_proxy
                           ? (AbBroadPair){proxy, phase->query_proxy}
                           : (AbBroadPair){phase->query_proxy, proxy};
    phase->pairs[phase->pair_count++] = pair;
    return true;
}

static bool less_pair(AbBroadPair first, AbBroadPair second) {
    return first.first < second.first ||
           (first.first == second.first && first.second < second.second);
}

static void sift_pairs(AbBroadPair *pairs, size_t count, size_t root) {
    size_t child = root * 2 + 1;
    while (child < count) {
        if (child + 1 < count && less_pair(pairs[child], pairs[child + 1])) {
            ++child;
        }
        if (!less_pair(pairs[root], pairs[child])) {
            return;
        }
        AbBroadPair saved = pairs[root];
        pairs[root] = pairs[child];
        pairs[child] = saved;
        root = child;
        child = root * 2 + 1;
    }
}

static void sort_pairs(AbBroadPair *pairs, size_t count) {
    for (size_t root = count / 2; root; --root) {
        sift_pairs(pairs, count, root - 1);
    }
    for (size_t remaining = count; remaining > 1; --remaining) {
        AbBroadPair saved = pairs[0];
        pairs[0] = pairs[remaining - 1];
        pairs[remaining - 1] = saved;
        sift_pairs(pairs, remaining - 1, 0);
    }
}

bool ab_broadphase_update(AbBroadPhase *phase, AbBroadCallback callback, void *context,
                          AbError *error) {
    if (!ab_broadphase_valid(phase) || !callback) {
        return ab_fail(error, "Invalid broadphase pair update");
    }
    phase->pair_count = 0;
    PairCollector collector = {phase, error, true};
    for (size_t index = 0; index < phase->move_count; ++index) {
        int32_t proxy = phase->moves[index];
        phase->query_proxy = proxy;
        if (proxy == -1) {
            continue;
        }
        if (!ab_tree_query(&phase->tree, phase->tree.nodes[proxy].bounds, collect_pair,
                           &collector, error) ||
            !collector.okay) {
            return false;
        }
    }
    phase->move_count = 0;
    sort_pairs(phase->pairs, phase->pair_count);
    size_t index = 0;
    while (index < phase->pair_count) {
        AbBroadPair pair = phase->pairs[index];
        callback(context, pair, phase->tree.nodes[pair.first].payload,
                 phase->tree.nodes[pair.second].payload);
        do {
            ++index;
        } while (index < phase->pair_count && phase->pairs[index].first == pair.first &&
                 phase->pairs[index].second == pair.second);
    }
    return true;
}
