#include "ab/ui_frame_tree.h"

#include <math.h>
#include <string.h>

static bool valid(const AbUiFrameNode *node) {
    return node && node->state &&
           node->state->child_count == node->state->lookup_count &&
           node->state->child_count <= node->capacity &&
           node->state->child_count <= UINT32_C(0x1000000) &&
           (!node->capacity || node->children);
}

static bool name(const AbUiFrameNode *node, AbSpan *output) {
    if (!node || !node->state || node->state->fields.name.kind != AB_UI_POINTER_TEXT ||
        !ab_ui_pointer_value_valid(&node->state->fields.name)) {
        return false;
    }
    *output = node->state->fields.name.text;
    return true;
}

static bool find_index(const AbUiFrameNode *node, AbSpan requested, size_t *output,
                       AbError *error) {
    if (!valid(node) || (requested.size && !requested.data)) {
        return ab_fail(error, "Invalid UI frame child lookup input");
    }
    *output = SIZE_MAX;
    for (size_t index = 0; index < node->state->child_count; ++index) {
        AbSpan candidate;
        if (!name(node->children[index], &candidate)) {
            return ab_fail(error, "Invalid attached UI child name");
        }
        if (candidate.size == requested.size &&
            (!candidate.size ||
             memcmp(candidate.data, requested.data, candidate.size) == 0)) {
            *output = index;
            break;
        }
    }
    return true;
}

static bool call(AbUiFrameNode *node, AbUiFrameMethod method, AbError *error) {
    if (!node || !node->method) {
        return ab_fail(error, "Missing eligible UI frame child method");
    }
    return node->method(node->context, node, method, error);
}

static bool visit(AbUiFrameNode *node, AbUiFrameMethod method, AbError *error) {
    for (size_t index = 0; index < node->state->child_count; ++index) {
        if (!call(node->children[index], method, error)) {
            return false;
        }
    }
    return true;
}

bool ab_ui_frame_add_child(AbUiFrameNode *parent, AbUiFrameNode *child, size_t position,
                           bool position_present, AbError *error) {
    AbSpan child_name;
    if (!valid(parent) || !name(child, &child_name) ||
        !ab_ui_pointer_value_valid(&parent->entered) ||
        parent->state->child_count == parent->capacity ||
        parent->state->child_count == UINT32_C(0x1000000)) {
        return ab_fail(error, "Invalid UI frame child insertion input");
    }
    size_t count = parent->state->child_count;
    size_t at = position_present ? position : count + 1;
    if (!at || at > count + 1) {
        return ab_fail(error, "UI frame child insertion index exceeds dense bounds");
    }
    size_t duplicate = SIZE_MAX;
    if (!find_index(parent, child_name, &duplicate, error)) {
        return false;
    }
    if (duplicate != SIZE_MAX) {
        return ab_fail(error, "UI frame child name is already attached");
    }
    --at;
    memmove(parent->children + at + 1, parent->children + at,
            (count - at) * sizeof(*parent->children));
    parent->children[at] = child;
    parent->state->child_count = parent->state->lookup_count = count + 1;
    child->parent = parent;
    return !ab_ui_pointer_value_truthy(&parent->entered) ||
           (call(child, AB_UI_FRAME_ENTRY, error) &&
            call(child, AB_UI_FRAME_LAYOUT, error));
}

bool ab_ui_frame_remove_child(AbUiFrameNode *parent, AbUiFrameNode *child,
                              AbError *error) {
    AbSpan child_name;
    if (!name(child, &child_name)) {
        return ab_fail(error, "Invalid UI frame child removal name");
    }
    size_t at = SIZE_MAX;
    if (!find_index(parent, child_name, &at, error)) {
        return false;
    }
    if (at == SIZE_MAX || parent->children[at] != child) {
        return ab_fail(error, "UI frame child removal requires matching identity");
    }
    size_t count = parent->state->child_count;
    memmove(parent->children + at, parent->children + at + 1,
            (count - at - 1) * sizeof(*parent->children));
    parent->children[count - 1] = NULL;
    parent->state->child_count = parent->state->lookup_count = count - 1;
    if (!ab_ui_pointer_value_valid(&child->entered)) {
        return ab_fail(error, "Invalid removed UI child entered value");
    }
    return !ab_ui_pointer_value_truthy(&child->entered) ||
           call(child, AB_UI_FRAME_EXIT, error);
}

bool ab_ui_frame_clear_children(AbUiFrameNode *parent, AbError *error) {
    if (!valid(parent) || !ab_ui_pointer_value_valid(&parent->entered)) {
        return ab_fail(error, "Invalid UI frame child reset input");
    }
    if (ab_ui_pointer_value_truthy(&parent->entered) &&
        !visit(parent, AB_UI_FRAME_EXIT, error)) {
        return false;
    }
    parent->state->child_count = parent->state->lookup_count = 0;
    return true;
}

bool ab_ui_frame_remove_self(AbUiFrameNode *node, AbError *error) {
    if (!node || !node->parent) {
        return ab_fail(error, "UI frame removal requires a parent");
    }
    return ab_ui_frame_remove_child(node->parent, node, error);
}

bool ab_ui_frame_pop_child(AbUiFrameNode *parent, AbUiFrameNode **output,
                           AbError *error) {
    if (!valid(parent) || !output || !parent->state->child_count) {
        return ab_fail(error, "UI frame pop requires a child");
    }
    AbUiFrameNode *child = parent->children[parent->state->child_count - 1];
    if (!ab_ui_frame_remove_child(parent, child, error)) {
        return false;
    }
    *output = child;
    return true;
}

bool ab_ui_frame_find_child(AbUiFrameNode *parent, const AbSpan *names, size_t count,
                            AbUiFrameNode **output, AbError *error) {
    if (!output || !names || !count || count > UINT32_C(0x1000000)) {
        return ab_fail(error, "Invalid UI frame child path input");
    }
    AbUiFrameNode *node = parent;
    for (size_t index = 0; node && index < count; ++index) {
        size_t at = SIZE_MAX;
        if (!find_index(node, names[index], &at, error)) {
            return false;
        }
        node = at == SIZE_MAX ? NULL : node->children[at];
    }
    *output = node;
    return true;
}

static bool change_counter(AbUiFrameLifecycle *lifecycle, float delta, AbError *error) {
    if (!lifecycle || !ab_ui_pointer_value_valid(&lifecycle->entry_counts)) {
        return ab_fail(error, "Invalid UI frame lifecycle counter");
    }
    AbUiPointerValue *value = &lifecycle->entry_counts;
    float count = 0;
    if (ab_ui_pointer_value_truthy(value)) {
        if (value->kind != AB_UI_POINTER_NUMBER) {
            return ab_fail(error, "UI frame lifecycle counter requires a number");
        }
        memcpy(&count, &value->number_bits, sizeof(count));
    }
    float next = count + delta;
    if (!isfinite(count) || !isfinite(next)) {
        return ab_fail(error, "UI frame lifecycle counter exceeds finite range");
    }
    value->kind = AB_UI_POINTER_NUMBER;
    memcpy(&value->number_bits, &next, sizeof(next));
    return true;
}

bool ab_ui_frame_enter(AbUiFrameNode *node, AbUiFrameLifecycle *lifecycle,
                       AbError *error) {
    if (!valid(node) || !ab_ui_pointer_value_valid(&node->entered) ||
        ab_ui_pointer_value_truthy(&node->entered)) {
        return ab_fail(error, "UI frame entry requires an unentered frame");
    }
    if (!change_counter(lifecycle, 1, error)) {
        return false;
    }
    node->entered = (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN, .boolean = true};
    return visit(node, AB_UI_FRAME_ENTRY, error);
}

bool ab_ui_frame_exit(AbUiFrameNode *node, AbUiFrameLifecycle *lifecycle,
                      AbError *error) {
    if (!valid(node)) {
        return ab_fail(error, "Invalid UI frame exit input");
    }
    if (!change_counter(lifecycle, -1, error)) {
        return false;
    }
    node->entered = (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN};
    return visit(node, AB_UI_FRAME_EXIT, error);
}

bool ab_ui_frame_layout_children(AbUiFrameNode *node, AbError *error) {
    if (!valid(node)) {
        return ab_fail(error, "Invalid UI frame child layout input");
    }
    return visit(node, AB_UI_FRAME_LAYOUT, error);
}
