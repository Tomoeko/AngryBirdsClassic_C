#include "ab/physics_tree.h"
#include "tree_internal.h"

#include <float.h>
#include <math.h>

static AbAabb segment_bounds(AbRay ray, AbVec2 direction) {
    AbVec2 end = {ray.first.x + ray.max_fraction * direction.x,
                  ray.first.y + ray.max_fraction * direction.y};
    return (AbAabb){{ray.first.x < end.x ? ray.first.x : end.x,
                     ray.first.y < end.y ? ray.first.y : end.y},
                    {ray.first.x > end.x ? ray.first.x : end.x,
                     ray.first.y > end.y ? ray.first.y : end.y}};
}

static bool overlaps(AbAabb first, AbAabb second) {
    return !(first.lower.x - second.upper.x > 0 || first.lower.y - second.upper.y > 0 ||
             second.lower.x - first.upper.x > 0 || second.lower.y - first.upper.y > 0);
}

static bool separated(AbAabb bounds, AbVec2 first, AbVec2 perpendicular,
                      AbVec2 absolute) {
    float dy = first.y - (bounds.upper.y + bounds.lower.y) * 0.5f;
    float dx = first.x - (bounds.upper.x + bounds.lower.x) * 0.5f;
    float projected = dy * perpendicular.y + perpendicular.x * dx;
    float half_y = (bounds.upper.y - bounds.lower.y) * 0.5f;
    float half_x = (bounds.upper.x - bounds.lower.x) * 0.5f;
    float radius = absolute.y * half_y + absolute.x * half_x;
    float distance = (projected > 0 ? projected : -projected) - radius;
    return distance > 0;
}

bool ab_tree_raycast(const AbDynamicTree *tree, AbRay ray, AbTreeRaycast callback,
                     void *context, float *final_fraction, AbError *error) {
    AbVec2 direction = {ray.second.x - ray.first.x, ray.second.y - ray.first.y};
    float squared = direction.y * direction.y + direction.x * direction.x;
    if (!tree || !tree->nodes || !isfinite(ray.max_fraction) || ray.max_fraction < 0) {
        return ab_fail(error, "Invalid dynamic-tree ray input");
    }
    float length = sqrtf(squared);
    AbVec2 normalized = direction;
    if (length >= FLT_EPSILON) {
        float inverse = 1.0f / length;
        normalized.x = direction.x * inverse;
        normalized.y = direction.y * inverse;
    }
    AbVec2 perpendicular = {-normalized.y, normalized.x};
    AbVec2 absolute = {perpendicular.x > 0 ? perpendicular.x : -perpendicular.x,
                       perpendicular.y > 0 ? perpendicular.y : -perpendicular.y};
    AbAabb segment = segment_bounds(ray, direction);
    int32_t stack[256] = {tree->root};
    size_t count = 1;
    while (count) {
        int32_t index = stack[--count];
        if (index == -1) {
            continue;
        }
        if (index < 0 || index >= tree->capacity || tree->nodes[index].height < 0) {
            return ab_fail(error, "Dynamic-tree ray reached an invalid node");
        }
        const AbTreeNode *node = tree->nodes + index;
        if (!overlaps(segment, node->bounds) ||
            separated(node->bounds, ray.first, perpendicular, absolute)) {
            continue;
        }
        if (node->children[0] != -1) {
            if (count > sizeof(stack) / sizeof(*stack) - 2) {
                return ab_fail(error, "Dynamic-tree ray exceeds stack bound");
            }
            stack[count++] = node->children[0];
            stack[count++] = node->children[1];
            continue;
        }
        if (!callback) {
            return ab_fail(error, "Missing dynamic-tree ray callback");
        }
        float next = ray.max_fraction;
        if (!callback(context, index, node->payload, ray, &next, error)) {
            return false;
        }
        if (next == 0) {
            ray.max_fraction = next;
            break;
        }
        if (next > 0) {
            ray.max_fraction = next;
            segment = segment_bounds(ray, direction);
        }
    }
    if (final_fraction) {
        *final_fraction = ray.max_fraction;
    }
    return true;
}
