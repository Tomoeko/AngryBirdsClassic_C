#ifndef AB_ORACLE_UI_FRAME_TREE_CASES_H
#define AB_ORACLE_UI_FRAME_TREE_CASES_H

enum {
    UI_TREE_ADD,
    UI_TREE_REMOVE,
    UI_TREE_CLEAR,
    UI_TREE_POP,
    UI_TREE_REMOVE_SELF,
    UI_TREE_ENTER,
    UI_TREE_EXIT,
    UI_TREE_LAYOUT,
    UI_TREE_CHILD,
    UI_TREE_PARENT,
    UI_TREE_SET_ENTERED,
    UI_TREE_SET_COUNTER,
    UI_TREE_RESET,
};

static const char *const ui_tree_names[] = {"node-a", "node-b", "node-c", "node-d",
                                            "node-e", "node-f", "node-g", "node-h"};
static const char *const ui_tree_texts[] = {"", "truthy"};

#endif
