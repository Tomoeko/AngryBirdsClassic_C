#ifndef AB_TOUCH_SNAPSHOT_H
#define AB_TOUCH_SNAPSHOT_H

#include "ab/camera_touch.h"
#include "ab/integer_order.h"

typedef struct {
    int32_t *keys;
    size_t *first_copy_indices;
    size_t capacity;
    AbIntegerOrderWorkspace order;
} AbTouchSnapshotWorkspace;

/* Numeric snapshot corresponding to MenuManager's raw-touch copy followed by
 * getFilteredTouches' second fresh deep copy. Input is in the raw native
 * table's traversal order; IDs obey the fresh integer-order contract. Copy the
 * x/y payload unchanged into the second copy's traversal order. With copy_touches
 * false or empty input, write count zero without requiring arrays/workspace.
 * Otherwise input, output and all borrowed scratch arrays must be disjoint.
 * No allocation. On failure, output/count are unchanged; scratch may change.
 * Source-table creation, other payload fields, menu callback mutations and
 * platform input delivery remain outside this numeric snapshot contract. */
bool ab_touch_snapshot(const AbCameraTouch *input, size_t count, bool copy_touches,
                       AbTouchSnapshotWorkspace *workspace, AbCameraTouch *output,
                       size_t output_capacity, size_t *output_count, AbError *error);

#endif
