#include "ab/z_order.h"

#include <math.h>

typedef struct {
    size_t first, last;
} Range;

static void exchange(AbZOrderEntry *entries, size_t first, size_t second) {
    AbZOrderEntry saved = entries[first];
    entries[first] = entries[second];
    entries[second] = saved;
}

static size_t partition(AbZOrderEntry *entries, Range range, size_t middle) {
    size_t pivot_slot = range.last - 1;
    exchange(entries, middle, pivot_slot);
    float pivot = entries[pivot_slot].z;
    size_t lower = range.first, upper = pivot_slot;
    for (;;) {
        do {
            ++lower;
        } while (entries[lower].z < pivot);
        do {
            --upper;
        } while (pivot < entries[upper].z);
        if (lower > upper)
            break;
        exchange(entries, lower, upper);
    }
    exchange(entries, lower, pivot_slot);
    return lower;
}

bool ab_z_order_sort(AbZOrderEntry *entries, size_t count, AbError *error) {
    if (count > UINT32_C(16777216) || (count && !entries))
        return ab_fail(error, "Invalid dense z-order storage");
    for (size_t index = 0; index < count; ++index)
        if (!isfinite(entries[index].z))
            return ab_fail(error, "Nonfinite numeric z-order key");
    if (count < 2)
        return true;
    /* Partition the smaller side immediately. Each deferred range therefore
     * at least halves the current work; the admitted length needs 24 levels. */
    Range pending[25], current = {0, count - 1};
    size_t waiting = 0;
    for (;;) {
        if (current.first < current.last) {
            if (entries[current.last].z < entries[current.first].z)
                exchange(entries, current.first, current.last);
            size_t distance = current.last - current.first;
            if (distance > 1) {
                size_t middle = current.first + distance / 2;
                if (entries[middle].z < entries[current.first].z)
                    exchange(entries, middle, current.first);
                else if (entries[current.last].z < entries[middle].z)
                    exchange(entries, middle, current.last);
                if (distance > 2) {
                    size_t pivot = partition(entries, current, middle);
                    Range left = {current.first, pivot - 1};
                    Range right = {pivot + 1, current.last};
                    bool left_first = pivot - current.first < current.last - pivot;
                    pending[waiting++] = left_first ? right : left;
                    current = left_first ? left : right;
                    continue;
                }
            }
        }
        if (!waiting)
            return true;
        current = pending[--waiting];
    }
}
