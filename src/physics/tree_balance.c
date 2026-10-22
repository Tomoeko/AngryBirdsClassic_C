#include "tree_internal.h"

AbAabb ab_tree_combine(AbAabb first, AbAabb second) {
    return (AbAabb){
        {first.lower.x < second.lower.x ? first.lower.x : second.lower.x,
         first.lower.y < second.lower.y ? first.lower.y : second.lower.y},
        {first.upper.x > second.upper.x ? first.upper.x : second.upper.x,
         first.upper.y > second.upper.y ? first.upper.y : second.upper.y},
    };
}

static int32_t greater_height(int32_t first, int32_t second) {
    return first > second ? first : second;
}

void ab_tree_refresh(AbDynamicTree *tree, int32_t index) {
    AbTreeNode *node = tree->nodes + index;
    const AbTreeNode *first = tree->nodes + node->children[0];
    const AbTreeNode *second = tree->nodes + node->children[1];
    node->bounds = ab_tree_combine(first->bounds, second->bounds);
    node->height = 1 + greater_height(first->height, second->height);
}

static int32_t promote_branch(AbDynamicTree *tree, int32_t index, unsigned side) {
    AbTreeNode *node = tree->nodes + index;
    int32_t promoted_index = node->children[side];
    AbTreeNode *promoted = tree->nodes + promoted_index;
    int32_t old_parent = node->parent;
    int32_t other_index = node->children[1 - side];
    int32_t first_grandchild = promoted->children[0];
    int32_t second_grandchild = promoted->children[1];
    int32_t retained = second_grandchild;
    int32_t transferred = first_grandchild;
    if (tree->nodes[first_grandchild].height > tree->nodes[second_grandchild].height) {
        retained = first_grandchild;
        transferred = second_grandchild;
    }
    promoted->parent = old_parent;
    promoted->children[0] = index;
    promoted->children[1] = retained;
    node->parent = promoted_index;
    node->children[side] = transferred;
    tree->nodes[transferred].parent = index;
    if (old_parent == -1) {
        tree->root = promoted_index;
    } else {
        AbTreeNode *parent = tree->nodes + old_parent;
        parent->children[parent->children[0] == index ? 0 : 1] = promoted_index;
    }
    const AbTreeNode *other = tree->nodes + other_index;
    const AbTreeNode *moved = tree->nodes + transferred;
    /* Both native rotations combine the unaffected branch first, even when
     * it becomes the second child. Keep comparison ties in this order. */
    node->bounds = ab_tree_combine(other->bounds, moved->bounds);
    node->height = 1 + greater_height(other->height, moved->height);
    promoted->bounds = ab_tree_combine(node->bounds, tree->nodes[retained].bounds);
    promoted->height = 1 + greater_height(node->height, tree->nodes[retained].height);
    return promoted_index;
}

int32_t ab_tree_balance(AbDynamicTree *tree, int32_t index) {
    const AbTreeNode *node = tree->nodes + index;
    if (node->children[0] == -1 || node->height <= 1) {
        return index;
    }
    int32_t difference =
        tree->nodes[node->children[1]].height - tree->nodes[node->children[0]].height;
    if (difference > 1) {
        return promote_branch(tree, index, 1);
    }
    if (difference < -1) {
        return promote_branch(tree, index, 0);
    }
    return index;
}
