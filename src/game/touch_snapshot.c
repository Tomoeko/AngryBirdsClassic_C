#include "ab/touch_snapshot.h"

bool ab_touch_snapshot(const AbCameraTouch *input, size_t count, bool copy_touches,
                       AbTouchSnapshotWorkspace *workspace, AbCameraTouch *output,
                       size_t output_capacity, size_t *output_count, AbError *error) {
    if (!output_count) {
        return ab_fail(error, "Missing filtered touch count output");
    }
    if (!copy_touches || !count) {
        *output_count = 0;
        return true;
    }
    if (!input || !output || !workspace || count > output_capacity ||
        count > workspace->capacity || !workspace->keys ||
        !workspace->first_copy_indices) {
        return ab_fail(error, "Invalid filtered touch snapshot storage");
    }
    for (size_t index = 0; index < count; ++index) {
        workspace->keys[index] = input[index].id;
    }
    AbIntegerOrderLayout layout;
    if (!ab_integer_order_growing(workspace->keys, count, &workspace->order, &layout,
                                  error)) {
        return false;
    }
    for (size_t index = 0; index < count; ++index) {
        size_t original = workspace->order.indices[index];
        workspace->first_copy_indices[index] = original;
        workspace->keys[index] = input[original].id;
    }
    if (!ab_integer_order_growing(workspace->keys, count, &workspace->order, &layout,
                                  error)) {
        return false;
    }
    for (size_t index = 0; index < count; ++index) {
        size_t first_copy = workspace->order.indices[index];
        output[index] = input[workspace->first_copy_indices[first_copy]];
    }
    *output_count = count;
    return true;
}
