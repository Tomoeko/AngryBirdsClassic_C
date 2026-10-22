#include "tree_internal.h"

static float perimeter(AbAabb bounds) {
    float width = bounds.upper.x - bounds.lower.x;
    float height = bounds.upper.y - bounds.lower.y;
    float sum = width + height;
    return sum + sum;
}

static float branch_cost(const AbTreeNode *node, AbAabb leaf, float inherited) {
    float combined = perimeter(ab_tree_combine(leaf, node->bounds));
    if (node->children[0] == -1) {
        return inherited + combined;
    }
    return combined - perimeter(node->bounds) + inherited;
}

static int32_t choose_sibling(const AbDynamicTree *tree, AbAabb leaf) {
    int32_t index = tree->root;
    while (tree->nodes[index].children[0] != -1) {
        const AbTreeNode *node = tree->nodes + index;
        float combined = perimeter(ab_tree_combine(leaf, node->bounds));
        float inherited = combined - perimeter(node->bounds);
        inherited = inherited + inherited;
        float first = branch_cost(tree->nodes + node->children[0], leaf, inherited);
        float second = branch_cost(tree->nodes + node->children[1], leaf, inherited);
        float parent = combined + combined;
        if (parent < first && parent < second) {
            break;
        }
        index = node->children[first < second ? 0 : 1];
    }
    return index;
}

static void refresh_ancestors(AbDynamicTree *tree, int32_t index) {
    while (index != -1) {
        index = ab_tree_balance(tree, index);
        ab_tree_refresh(tree, index);
        index = tree->nodes[index].parent;
    }
}

void ab_tree_insert(AbDynamicTree *tree, int32_t leaf) {
    ++tree->insertion_count;
    if (tree->root == -1) {
        tree->root = leaf;
        tree->nodes[leaf].parent = -1;
        return;
    }
    int32_t sibling = choose_sibling(tree, tree->nodes[leaf].bounds);
    int32_t previous_parent = tree->nodes[sibling].parent;
    int32_t parent_index = ab_tree_allocate(tree);
    AbTreeNode *parent = tree->nodes + parent_index;
    parent->parent = previous_parent;
    parent->bounds =
        ab_tree_combine(tree->nodes[sibling].bounds, tree->nodes[leaf].bounds);
    parent->height = tree->nodes[sibling].height + 1;
    parent->children[0] = sibling;
    parent->children[1] = leaf;
    tree->nodes[sibling].parent = parent_index;
    tree->nodes[leaf].parent = parent_index;
    if (previous_parent == -1) {
        tree->root = parent_index;
    } else {
        AbTreeNode *previous = tree->nodes + previous_parent;
        previous->children[previous->children[0] == sibling ? 0 : 1] = parent_index;
    }
    refresh_ancestors(tree, parent_index);
}

void ab_tree_remove(AbDynamicTree *tree, int32_t leaf) {
    if (leaf == tree->root) {
        tree->root = -1;
        return;
    }
    int32_t parent_index = tree->nodes[leaf].parent;
    const AbTreeNode *parent = tree->nodes + parent_index;
    int32_t grandparent_index = parent->parent;
    int32_t sibling = parent->children[parent->children[0] == leaf ? 1 : 0];
    tree->nodes[sibling].parent = grandparent_index;
    if (grandparent_index == -1) {
        tree->root = sibling;
        ab_tree_release(tree, parent_index);
        return;
    }
    AbTreeNode *grandparent = tree->nodes + grandparent_index;
    grandparent->children[grandparent->children[0] == parent_index ? 0 : 1] = sibling;
    ab_tree_release(tree, parent_index);
    refresh_ancestors(tree, grandparent_index);
}
