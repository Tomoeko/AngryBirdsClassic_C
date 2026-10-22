#include "ab/ui_menu_navigation.h"

static AbUiMenuFrame *binding(AbUiMenuNavigation *navigation, AbUiMenuNode *menu) {
    if (!navigation || (navigation->frame_count && !navigation->frames) || !menu) {
        return NULL;
    }
    for (size_t index = 0; index < navigation->frame_count; ++index) {
        AbUiMenuFrame *frame = navigation->frames + index;
        if (frame->menu == menu && frame->tree && frame->tree->state == menu->frame) {
            return frame;
        }
    }
    return NULL;
}

static bool resolve_target(AbUiMenuNavigation *navigation, AbUiMenuTarget target,
                           AbUiMenuNode **output, AbError *error) {
    if (target.kind == AB_UI_MENU_TARGET_ABSENT) {
        *output = NULL;
        return true;
    }
    if (target.kind == AB_UI_MENU_TARGET_FRAME) {
        *output = target.frame;
        return true;
    }
    if (target.kind != AB_UI_MENU_TARGET_LINK) {
        return ab_fail(error, "Unsupported menu event target type");
    }
    AbUiMenuManagerView view;
    uintptr_t value;
    bool present;
    if (!ab_ui_menu_manager_view(navigation->manager, &view) ||
        !ab_string_map_get(view.links, target.link, &value, &present, error)) {
        return false;
    }
    if (!present) {
        return ab_fail(error, "Missing pushed-frame link record");
    }
    *output = (AbUiMenuNode *)value;
    return true;
}

bool ab_ui_menu_pop_frame(AbUiMenuNavigation *navigation, const AbSpan *channel,
                          AbUiFrameNode **output, AbError *error) {
    if (!navigation || !output) {
        return ab_fail(error, "Invalid menu frame pop input");
    }
    AbUiMenuNode *root;
    if (!ab_ui_menu_get_root(navigation->manager, channel, &root, error)) {
        return false;
    }
    AbUiMenuFrame *frame = binding(navigation, root);
    if (!frame) {
        return ab_fail(error, "Menu frame pop requires a bound root");
    }
    return ab_ui_frame_pop_child(frame->tree, output, error);
}

static bool push_frame(AbUiMenuNavigation *navigation, AbUiMenuTarget target,
                       const AbSpan *channel, AbError *error) {
    AbUiMenuNode *root = NULL, *child = NULL;
    if (!ab_ui_menu_get_root(navigation->manager, channel, &root, error) ||
        !resolve_target(navigation, target, &child, error)) {
        return false;
    }
    AbSpan selected = channel ? *channel : (AbSpan){(const uint8_t *)"root", 4};
    if (!navigation->frame_pushed) {
        return ab_fail(error, "Missing menu frame push notification boundary");
    }
    if (!navigation->frame_pushed(navigation->context, child, selected, error)) {
        return false;
    }
    AbUiMenuFrame *child_frame = binding(navigation, child);
    if (!child_frame) {
        return ab_fail(error, "Pushed frame requires a canonical binding");
    }
    child_frame->remove_from_frame = true;
    AbUiMenuFrame *root_frame = binding(navigation, root);
    if (!root_frame) {
        return ab_fail(error, "Pushed menu frame requires a bound root");
    }
    if (!ab_ui_frame_add_child(root_frame->tree, child_frame->tree, 0, false, error)) {
        return false;
    }
    if (ab_ui_pointer_value_truthy(&root_frame->tree->entered)) {
        if (!root->method) {
            return ab_fail(error, "Missing menu root layout method");
        }
        return root->method(root->context, root, AB_UI_MENU_LAYOUT, 0, 0, error);
    }
    return true;
}

bool ab_ui_menu_event(AbUiMenuNavigation *navigation, AbUiMenuEventKind event,
                      AbUiMenuTarget target, const AbSpan *channel,
                      const AbUiMenuBoundaries *boundaries, AbError *error) {
    if (!navigation || !navigation->manager) {
        return ab_fail(error, "Missing menu navigation owner");
    }
    if (event == AB_UI_MENU_EVENT_CHANGE_SCENE) {
        if (target.kind == AB_UI_MENU_TARGET_ABSENT ||
            (target.kind == AB_UI_MENU_TARGET_FRAME && !target.frame)) {
            return ab_fail(error, "Missing scene change target");
        }
        AbUiPointerValue enabled = {.kind = AB_UI_POINTER_BOOLEAN, .boolean = true};
        if (!ab_ui_menu_manager_delegate_back(navigation->manager, enabled)) {
            return ab_fail(error, "Cannot enable scene back delegation");
        }
        if (target.kind == AB_UI_MENU_TARGET_LINK) {
            return ab_ui_menu_handle_result(navigation->manager, target.link, channel,
                                            boundaries, error);
        }
        if (target.kind != AB_UI_MENU_TARGET_FRAME) {
            return ab_fail(error, "Unsupported scene change target type");
        }
        return ab_ui_menu_change_root(navigation->manager, target.frame, channel,
                                      boundaries, error);
    }
    if (event == AB_UI_MENU_EVENT_PUSH_FRAME) {
        return push_frame(navigation, target, channel, error);
    }
    if (event == AB_UI_MENU_EVENT_POP_FRAME) {
        AbUiFrameNode *popped;
        return ab_ui_menu_pop_frame(navigation, channel, &popped, error);
    }
    if (event != AB_UI_MENU_EVENT_OTHER) {
        return ab_fail(error, "Unsupported menu event kind");
    }
    return true;
}

bool ab_ui_menu_frame_remove(const AbUiMenuNavigation *navigation,
                             const AbUiMenuFrame *frame, AbError *error) {
    if (!navigation || !frame || !frame->remove_from_frame ||
        !navigation->pop_requested) {
        return ab_fail(error, "Missing installed frame removal callback");
    }
    return navigation->pop_requested(navigation->context, error);
}
