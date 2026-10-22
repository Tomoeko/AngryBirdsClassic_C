#ifndef AB_TEST_ROOT_QUERY_MENU_H
#define AB_TEST_ROOT_QUERY_MENU_H

#include "ab/ui_frame_tree.h"
#include "ab/ui_menu_manager.h"
#include "../tools/oracle/reference_root_queries.h"

#include <string.h>

typedef struct {
    AbUiMenuManager *manager;
    AbUiFrameState frames[4];
    AbUiFrameNode nodes[4], *children[3];
    AbUiFrameLifecycle lifecycle;
    AbUiMenuNode menu;
} QueryMenu;

static AbUiPointerValue query_ui_flag(unsigned kind) {
    if (!kind)
        return (AbUiPointerValue){0};
    if (kind < 3)
        return (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN, .boolean = kind == 2};
    return (AbUiPointerValue){.kind = AB_UI_POINTER_NUMBER};
}

static bool query_frame_method(void *context, AbUiFrameNode *node,
                               AbUiFrameMethod method, AbError *error) {
    QueryMenu *menu = context;
    if (method == AB_UI_FRAME_ENTRY)
        return ab_ui_frame_enter(node, &menu->lifecycle, error);
    if (method == AB_UI_FRAME_EXIT)
        return ab_ui_frame_exit(node, &menu->lifecycle, error);
    return ab_ui_frame_layout_children(node, error);
}

static bool query_menu_method(void *context, AbUiMenuNode *node, AbUiMenuMethod method,
                              float first, float second, AbError *error) {
    (void)first;
    (void)second;
    if (method < AB_UI_MENU_ENTRY || method > AB_UI_MENU_LAYOUT)
        return ab_fail(error, "Unexpected query fixture menu lifecycle method");
    return query_frame_method(context, node->tree,
                              (AbUiFrameMethod)(method - AB_UI_MENU_ENTRY), error);
}

static bool query_menu_changed(void *context, AbUiMenuNode *menu, AbSpan channel,
                               AbError *error) {
    (void)context;
    (void)menu;
    (void)channel;
    (void)error;
    /* Only the root-query result is compared; setup notification is observed
     * at this controlled boundary. Base Frame entry/layout execute above. */
    return true;
}

static bool query_input_reset(void *context, AbUiMenuManager *manager, AbError *error) {
    (void)context;
    (void)manager;
    (void)error;
    /* Query fixtures have no input recipients. The independent input owner
     * corpus covers this controlled constructor boundary. */
    return true;
}

static bool query_menu_prepare_named(ReferenceRootQuery input,
                                     const char *const *child_names, QueryMenu *output,
                                     AbError *error) {
    *output = (QueryMenu){0};
    if (!input.root)
        return true;
    AbUiMenuBoundaries boundaries = {.screen_size = {480, 320},
                                     .root_changed = query_menu_changed,
                                     .reset_filtered_input = query_input_reset};
    if (!ab_ui_menu_manager_open(8, 32768, &boundaries, &output->manager, error))
        return false;
    if (input.root == 1)
        return true;
    static const char *names[] = {
        "",        "", "mainMenu", "mainMenuRoot", "ForceUpdatePopup",
        "gameHud", "", "",         "other"};
    AbUiPointerValue name =
        (input.root < 6 || input.root == 8)
            ? (AbUiPointerValue){.kind = AB_UI_POINTER_TEXT,
                                 .text = {(const uint8_t *)names[input.root],
                                          strlen(names[input.root])}}
            : query_ui_flag(input.root == 6 ? 3u : 1u);
    output->frames[0].fields.name = name;
    output->nodes[0] = (AbUiFrameNode){.state = output->frames,
                                       .children = output->children,
                                       .capacity = 3,
                                       .context = output,
                                       .method = query_frame_method};
    static const char *default_names[] = {"c0", "c1", "c2"};
    if (!child_names)
        child_names = default_names;
    for (unsigned child = 0; child < input.children; ++child) {
        AbUiFrameState *state = output->frames + child + 1;
        state->fields.name = (AbUiPointerValue){
            .kind = AB_UI_POINTER_TEXT,
            .text = {(const uint8_t *)child_names[child], strlen(child_names[child])}};
        state->fields.require_input = query_ui_flag(child ? 1u : input.require);
        state->fields.visible = state->fields.active = query_ui_flag(1);
        AbUiFrameNode *node = output->nodes + child + 1;
        *node = (AbUiFrameNode){
            .state = state, .context = output, .method = query_frame_method};
        if (!ab_ui_frame_add_child(output->nodes, node, 0, false, error))
            return false;
    }
    output->menu = (AbUiMenuNode){.frame = output->frames,
                                  .identity = 1,
                                  .context = output,
                                  .method = query_menu_method,
                                  .tree = output->nodes};
    return ab_ui_menu_set_root(output->manager, &output->menu, NULL, &boundaries,
                               error);
}

static inline bool query_menu_prepare(ReferenceRootQuery input, QueryMenu *output,
                                      AbError *error) {
    return query_menu_prepare_named(input, NULL, output, error);
}

#endif
