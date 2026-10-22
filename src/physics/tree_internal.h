#ifndef AB_PHYSICS_TREE_INTERNAL_H
#define AB_PHYSICS_TREE_INTERNAL_H

#include "ab/physics_tree.h"

bool ab_tree_valid_bounds(AbAabb bounds);
bool ab_tree_valid_proxy(const AbDynamicTree *tree, int32_t proxy);

AbAabb ab_tree_combine(AbAabb first, AbAabb second);
void ab_tree_refresh(AbDynamicTree *tree, int32_t node);
int32_t ab_tree_balance(AbDynamicTree *tree, int32_t node);
int32_t ab_tree_allocate(AbDynamicTree *tree);
void ab_tree_release(AbDynamicTree *tree, int32_t node);
void ab_tree_insert(AbDynamicTree *tree, int32_t leaf);
void ab_tree_remove(AbDynamicTree *tree, int32_t leaf);

#endif
