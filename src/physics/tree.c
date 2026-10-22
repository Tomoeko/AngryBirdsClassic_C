#include "ab/physics_tree.h"

#include "tree_internal.h"

#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

bool ab_tree_valid_bounds(AbAabb bounds) {
    return isfinite(bounds.lower.x) && isfinite(bounds.lower.y) &&
           isfinite(bounds.upper.x) && isfinite(bounds.upper.y) &&
           bounds.lower.x <= bounds.upper.x && bounds.lower.y <= bounds.upper.y;
}

bool ab_tree_valid_proxy(const AbDynamicTree *tree, int32_t proxy) {
    return tree && tree->nodes && proxy >= 0 && proxy < tree->capacity &&
           tree->nodes[proxy].height == 0 && tree->nodes[proxy].children[0] == -1;
}

static void initialize_free_nodes(AbTreeNode *nodes, int32_t start, int32_t end) {
    for (int32_t index = start; index < end; ++index) {
        nodes[index] = (AbTreeNode){.parent = index + 1, .height = -1};
    }
    nodes[end - 1].parent = -1;
}

void ab_tree_close(AbDynamicTree *tree) {
    if (tree) {
        free(tree->nodes);
        *tree = (AbDynamicTree){0};
    }
}

bool ab_tree_open(size_t maximum_capacity, AbDynamicTree *tree, AbError *error) {
    if (!tree) {
        return ab_fail(error, "Missing dynamic-tree output");
    }
    *tree = (AbDynamicTree){0};
    if (maximum_capacity < 16 || maximum_capacity > INT32_MAX ||
        maximum_capacity > SIZE_MAX / sizeof(AbTreeNode)) {
        return ab_fail(error, "Invalid dynamic-tree capacity limit");
    }
    AbTreeNode *nodes = calloc(16, sizeof(*nodes));
    if (!nodes) {
        return ab_fail(error, "Cannot allocate dynamic-tree pool");
    }
    initialize_free_nodes(nodes, 0, 16);
    *tree = (AbDynamicTree){.nodes = nodes,
                            .root = -1,
                            .capacity = 16,
                            .free_head = 0,
                            .maximum_capacity = maximum_capacity};
    return true;
}

static bool reserve_nodes(AbDynamicTree *tree, int32_t required, AbError *error) {
    if (tree->capacity - tree->count >= required) {
        return true;
    }
    size_t capacity = (size_t)tree->capacity * 2;
    if (capacity > tree->maximum_capacity) {
        return ab_fail(error, "Dynamic-tree capacity limit reached");
    }
    AbTreeNode *nodes = realloc(tree->nodes, capacity * sizeof(*nodes));
    if (!nodes) {
        return ab_fail(error, "Cannot grow dynamic-tree pool");
    }
    initialize_free_nodes(nodes, tree->capacity, (int32_t)capacity);
    /* Reserve can run while one free node remains; keep its allocation order. */
    if (tree->free_head == -1) {
        tree->free_head = tree->capacity;
    } else {
        int32_t tail = tree->free_head;
        while (nodes[tail].parent != -1) {
            tail = nodes[tail].parent;
        }
        nodes[tail].parent = tree->capacity;
    }
    tree->nodes = nodes;
    tree->capacity = (int32_t)capacity;
    return true;
}

int32_t ab_tree_allocate(AbDynamicTree *tree) {
    int32_t index = tree->free_head;
    AbTreeNode *node = tree->nodes + index;
    tree->free_head = node->parent;
    node->parent = -1;
    node->children[0] = -1;
    node->children[1] = -1;
    node->height = 0;
    node->payload = 0;
    ++tree->count;
    return index;
}

void ab_tree_release(AbDynamicTree *tree, int32_t index) {
    tree->nodes[index].parent = tree->free_head;
    tree->nodes[index].height = -1;
    tree->free_head = index;
    --tree->count;
}

bool ab_tree_create(AbDynamicTree *tree, AbAabb bounds, uintptr_t payload,
                    int32_t *proxy, AbError *error) {
    if (!tree || !tree->nodes || !proxy || !ab_tree_valid_bounds(bounds)) {
        return ab_fail(error, "Invalid dynamic-tree proxy input");
    }
    if (!reserve_nodes(tree, tree->root == -1 ? 1 : 2, error)) {
        return false;
    }
    int32_t index = ab_tree_allocate(tree);
    tree->nodes[index].bounds =
        (AbAabb){{bounds.lower.x - 0.1f, bounds.lower.y - 0.1f},
                 {bounds.upper.x + 0.1f, bounds.upper.y + 0.1f}};
    tree->nodes[index].payload = payload;
    ab_tree_insert(tree, index);
    *proxy = index;
    return true;
}

bool ab_tree_destroy(AbDynamicTree *tree, int32_t proxy) {
    if (!ab_tree_valid_proxy(tree, proxy)) {
        return false;
    }
    ab_tree_remove(tree, proxy);
    ab_tree_release(tree, proxy);
    return true;
}

bool ab_tree_move(AbDynamicTree *tree, int32_t proxy, AbAabb bounds,
                  AbVec2 displacement, bool *moved) {
    if (!moved || !ab_tree_valid_proxy(tree, proxy) || !ab_tree_valid_bounds(bounds) ||
        !isfinite(displacement.x) || !isfinite(displacement.y)) {
        return false;
    }
    AbAabb previous = tree->nodes[proxy].bounds;
    *moved = false;
    if (previous.lower.x <= bounds.lower.x && previous.lower.y <= bounds.lower.y &&
        bounds.upper.x <= previous.upper.x && bounds.upper.y <= previous.upper.y) {
        return true;
    }
    ab_tree_remove(tree, proxy);
    AbAabb expanded = {{bounds.lower.x - 0.1f, bounds.lower.y - 0.1f},
                       {bounds.upper.x + 0.1f, bounds.upper.y + 0.1f}};
    displacement.x = displacement.x + displacement.x;
    displacement.y = displacement.y + displacement.y;
    if (displacement.x < 0) {
        expanded.lower.x = expanded.lower.x + displacement.x;
    } else {
        expanded.upper.x = expanded.upper.x + displacement.x;
    }
    if (displacement.y < 0) {
        expanded.lower.y = expanded.lower.y + displacement.y;
    } else {
        expanded.upper.y = expanded.upper.y + displacement.y;
    }
    tree->nodes[proxy].bounds = expanded;
    ab_tree_insert(tree, proxy);
    *moved = true;
    return true;
}

bool ab_tree_shift(AbDynamicTree *tree, AbVec2 origin) {
    if (!tree || !tree->nodes || !isfinite(origin.x) || !isfinite(origin.y)) {
        return false;
    }
    for (int32_t index = 0; index < tree->capacity; ++index) {
        AbAabb *bounds = &tree->nodes[index].bounds;
        bounds->lower.x = bounds->lower.x - origin.x;
        bounds->lower.y = bounds->lower.y - origin.y;
        bounds->upper.x = bounds->upper.x - origin.x;
        bounds->upper.y = bounds->upper.y - origin.y;
    }
    return true;
}

static bool overlaps(AbAabb first, AbAabb second) {
    return !(first.lower.x - second.upper.x > 0 || first.lower.y - second.upper.y > 0 ||
             second.lower.x - first.upper.x > 0 || second.lower.y - first.upper.y > 0);
}

bool ab_tree_query(const AbDynamicTree *tree, AbAabb bounds, AbTreeQuery callback,
                   void *context, AbError *error) {
    if (!tree || !tree->nodes || !callback || !ab_tree_valid_bounds(bounds)) {
        return ab_fail(error, "Invalid dynamic-tree query");
    }
    int32_t stack[256] = {tree->root};
    size_t count = 1;
    while (count) {
        int32_t index = stack[--count];
        if (index == -1) {
            continue;
        }
        if (index < 0 || index >= tree->capacity || tree->nodes[index].height < 0) {
            return ab_fail(error, "Dynamic-tree query reached an invalid node");
        }
        const AbTreeNode *node = tree->nodes + index;
        if (!overlaps(bounds, node->bounds)) {
            continue;
        }
        if (node->children[0] == -1) {
            if (!callback(context, index, node->payload)) {
                return true;
            }
        } else {
            if (count > sizeof(stack) / sizeof(stack[0]) - 2) {
                return ab_fail(error, "Dynamic-tree traversal exceeds stack bound");
            }
            stack[count++] = node->children[0];
            stack[count++] = node->children[1];
        }
    }
    return true;
}
