#include "ab/ui_menu_manager.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

struct AbUiMenuManager {
    AbStringMap *menus, *queue, *links;
    AbSpan root_channel;
    const AbSpan *menu_order;
    size_t menu_count;
    const AbSpan *input_order;
    size_t input_count;
    AbVec2 screen_size;
    AbUiPointerValue allow_input, update_enabled, delegate_back_clicks;
};

static bool finite_point(AbVec2 point) {
    return isfinite(point.x) && isfinite(point.y);
}

static AbSpan channel_name(const AbUiMenuManager *manager, const AbSpan *channel) {
    return channel ? *channel : manager->root_channel;
}

static bool root_channel(AbSpan channel) {
    return channel.size == 4 && channel.data && memcmp(channel.data, "root", 4) == 0;
}

static bool node_get(const AbStringMap *map, AbSpan channel, AbUiMenuNode **output,
                     AbError *error) {
    uintptr_t value;
    bool present;
    if (!ab_string_map_get(map, channel, &value, &present, error)) {
        return false;
    }
    *output = present ? (AbUiMenuNode *)value : NULL;
    return true;
}

static bool method(AbUiMenuNode *node, AbUiMenuMethod operation, float first,
                   float second, AbError *error) {
    if (!node || !node->method) {
        return ab_fail(error, "Missing eligible menu method");
    }
    return node->method(node->context, node, operation, first, second, error);
}

bool ab_ui_menu_manager_open(size_t maximum_nodes, size_t allocation_limit,
                             const AbUiMenuBoundaries *boundaries,
                             AbUiMenuManager **output, AbError *error) {
    if (!output) {
        return ab_fail(error, "Missing menu manager output");
    }
    *output = NULL;
    if (allocation_limit < sizeof(AbUiMenuManager)) {
        return ab_fail(error, "Menu manager allocation limit is too small");
    }
    AbUiMenuManager *manager = calloc(1, sizeof(*manager));
    if (!manager) {
        return ab_fail(error, "Cannot allocate menu manager owner");
    }
    size_t map_limit = (allocation_limit - sizeof(*manager)) / 3;
    if (!ab_string_map_open(0, maximum_nodes, map_limit, &manager->menus, error) ||
        !ab_string_map_open(0, maximum_nodes, map_limit, &manager->queue, error) ||
        !ab_string_map_open(0, maximum_nodes, map_limit, &manager->links, error) ||
        !ab_ui_menu_manager_reset(manager, boundaries, error)) {
        ab_ui_menu_manager_close(manager);
        return false;
    }
    *output = manager;
    return true;
}

bool ab_ui_menu_manager_reset(AbUiMenuManager *manager,
                              const AbUiMenuBoundaries *boundaries, AbError *error) {
    if (!manager || !boundaries || !finite_point(boundaries->screen_size)) {
        return ab_fail(error, "Invalid menu manager reset input");
    }
    if (!ab_string_map_reset(manager->menus, 0, error) ||
        !ab_string_map_reset(manager->queue, 0, error) ||
        !ab_string_map_reset(manager->links, 0, error)) {
        return false;
    }
    manager->root_channel = (AbSpan){(const uint8_t *)"root", 4};
    manager->menu_order = &manager->root_channel;
    manager->menu_count = 1;
    manager->input_order = &manager->root_channel;
    manager->input_count = 1;
    manager->screen_size = boundaries->screen_size;
    manager->allow_input = manager->update_enabled = manager->delegate_back_clicks =
        (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN, .boolean = true};
    if (!boundaries->reset_filtered_input) {
        return ab_fail(error, "Missing menu input reset boundary");
    }
    return boundaries->reset_filtered_input(boundaries->context, manager, error);
}

void ab_ui_menu_manager_close(AbUiMenuManager *manager) {
    if (manager) {
        ab_string_map_close(manager->links);
        ab_string_map_close(manager->queue);
        ab_string_map_close(manager->menus);
        free(manager);
    }
}

bool ab_ui_menu_manager_view(const AbUiMenuManager *manager,
                             AbUiMenuManagerView *output) {
    if (!manager || !output) {
        return false;
    }
    *output = (AbUiMenuManagerView){
        manager->menus,       manager->queue,          manager->links,
        manager->menu_order,  manager->menu_count,     manager->screen_size,
        manager->allow_input, manager->update_enabled, manager->delegate_back_clicks,
        manager->input_order, manager->input_count};
    return true;
}

bool ab_ui_menu_manager_collect_nil_keys(AbUiMenuManager *manager) {
    return manager && ab_string_map_collect_nil_keys(manager->menus) &&
           ab_string_map_collect_nil_keys(manager->queue) &&
           ab_string_map_collect_nil_keys(manager->links);
}

static bool valid_order(const AbSpan *order, size_t count, AbError *error) {
    if (count > UINT32_C(0x1000000) || (count && !order)) {
        return ab_fail(error, "Invalid menu traversal order");
    }
    for (size_t index = 0; index < count; ++index) {
        if (order[index].size > UINT32_MAX ||
            (order[index].size && !order[index].data)) {
            return ab_fail(error, "Invalid menu order channel");
        }
    }
    return true;
}

bool ab_ui_menu_manager_order(AbUiMenuManager *manager, const AbSpan *order,
                              size_t count, AbError *error) {
    if (!manager || !valid_order(order, count, error)) {
        return false;
    }
    manager->menu_order = order;
    manager->menu_count = count;
    return true;
}

bool ab_ui_menu_manager_input_order(AbUiMenuManager *manager, const AbSpan *order,
                                    size_t count, AbError *error) {
    if (!manager || !valid_order(order, count, error)) {
        return false;
    }
    manager->input_order = order;
    manager->input_count = count;
    return true;
}

bool ab_ui_menu_pointer(AbUiMenuManager *manager, AbSpan event, AbVec2 point,
                        AbUiMenuPointerCallback callback, void *context, bool *accepted,
                        AbError *error) {
    if (!manager || !accepted || (event.size && !event.data) || !finite_point(point)) {
        return ab_fail(error, "Invalid menu pointer input");
    }
    for (size_t index = 0; index < manager->input_count; ++index) {
        AbUiMenuNode *menu;
        if (!node_get(manager->menus, manager->input_order[index], &menu, error)) {
            return false;
        }
        if (!menu) {
            continue;
        }
        AbUiPointerReply reply = {0};
        if (!callback) {
            return ab_fail(error, "Missing menu pointer method");
        }
        if (!callback(context, menu, event, point, &reply, error)) {
            return false;
        }
        if (!ab_ui_pointer_value_valid(reply.values)) {
            return ab_fail(error, "Invalid menu pointer primary reply");
        }
        if (ab_ui_pointer_value_truthy(reply.values)) {
            *accepted = false;
            return true;
        }
    }
    *accepted = true;
    return true;
}

bool ab_ui_menu_manager_allow_input(AbUiMenuManager *manager, AbUiPointerValue value) {
    if (!manager || !ab_ui_pointer_value_valid(&value)) {
        return false;
    }
    manager->allow_input = value;
    return true;
}

bool ab_ui_menu_manager_update_enabled(AbUiMenuManager *manager,
                                       AbUiPointerValue value) {
    if (!manager || !ab_ui_pointer_value_valid(&value)) {
        return false;
    }
    manager->update_enabled = value;
    return true;
}

bool ab_ui_menu_manager_delegate_back(AbUiMenuManager *manager,
                                      AbUiPointerValue value) {
    if (!manager || !ab_ui_pointer_value_valid(&value)) {
        return false;
    }
    manager->delegate_back_clicks = value;
    return true;
}

bool ab_ui_menu_add_link(AbUiMenuManager *manager, AbSpan name, AbUiMenuNode *target,
                         AbError *error) {
    if (!manager) {
        return ab_fail(error, "Missing menu manager");
    }
    return ab_string_map_set(manager->links, name, (uintptr_t)target, true, error);
}

bool ab_ui_menu_get_link(const AbUiMenuManager *manager, AbSpan name,
                         AbUiMenuNode **output, AbError *error) {
    if (!manager || !output) {
        return ab_fail(error, "Invalid menu link query");
    }
    return node_get(manager->links, name, output, error);
}

bool ab_ui_menu_handle_result(AbUiMenuManager *manager, AbSpan result,
                              const AbSpan *channel,
                              const AbUiMenuBoundaries *boundaries, AbError *error) {
    if (!manager) {
        return ab_fail(error, "Missing menu manager");
    }
    uintptr_t target;
    bool present;
    if (!ab_string_map_get(manager->links, result, &target, &present, error)) {
        return false;
    }
    if (!present) {
        return ab_fail(error, "Missing menu result link");
    }
    return ab_ui_menu_change_root(manager, (AbUiMenuNode *)target, channel, boundaries,
                                  error);
}

static bool dynamic_assets(AbUiMenuNode *menu, bool release,
                           const AbUiMenuBoundaries *boundaries, AbError *error) {
    if (!boundaries->use_dynamic_assets) {
        return true;
    }
    if (!release && !method(menu, AB_UI_MENU_PREPARE_DYNAMIC, 0, 0, error)) {
        return false;
    }
    if (!boundaries->requirements ||
        (release ? !boundaries->delay_release : !boundaries->load)) {
        return ab_fail(error, "Missing menu dynamic asset boundary");
    }
    AbUiPointerValue requirements;
    if (!boundaries->requirements(boundaries->context, menu, &requirements, error)) {
        return false;
    }
    if (!ab_ui_pointer_value_valid(&requirements)) {
        return ab_fail(error, "Invalid menu dynamic asset requirements");
    }
    return release ? boundaries->delay_release(boundaries->context, requirements, error)
                   : boundaries->load(boundaries->context, requirements, error);
}

bool ab_ui_menu_set_root(AbUiMenuManager *manager, AbUiMenuNode *menu,
                         const AbSpan *channel, const AbUiMenuBoundaries *boundaries,
                         AbError *error) {
    if (!manager) {
        return ab_fail(error, "Missing menu manager");
    }
    AbSpan selected = channel_name(manager, channel);
    if (!ab_string_map_set(manager->menus, selected, (uintptr_t)menu, menu != NULL,
                           error)) {
        return false;
    }
    if (!menu) {
        return true;
    }
    if (!boundaries) {
        return ab_fail(error, "Missing menu root boundaries");
    }
    if (!dynamic_assets(menu, false, boundaries, error) ||
        !method(menu, AB_UI_MENU_ENTRY, 0, 0, error)) {
        return false;
    }
    if (root_channel(selected) && menu->background_present) {
        float normalized[4];
        for (size_t channel_index = 0; channel_index < 4; ++channel_index) {
            if (!isfinite(menu->background[channel_index])) {
                return ab_fail(error, "Nonfinite menu background color");
            }
            normalized[channel_index] = menu->background[channel_index] / 255;
        }
        if (!boundaries->fullscreen_rectangle) {
            return ab_fail(error, "Missing menu background rectangle boundary");
        }
        if (!boundaries->fullscreen_rectangle(boundaries->context, normalized, error)) {
            return false;
        }
    }
    if (!method(menu, AB_UI_MENU_LAYOUT, 0, 0, error)) {
        return false;
    }
    if (!boundaries->root_changed) {
        return ab_fail(error, "Missing menu root notification boundary");
    }
    return boundaries->root_changed(boundaries->context, menu, selected, error);
}

bool ab_ui_menu_exit_root(AbUiMenuManager *manager, const AbSpan *channel,
                          const AbUiMenuBoundaries *boundaries, AbError *error) {
    if (!manager) {
        return ab_fail(error, "Missing menu manager");
    }
    AbSpan selected = channel_name(manager, channel);
    AbUiMenuNode *menu;
    if (!node_get(manager->menus, selected, &menu, error)) {
        return false;
    }
    if (!menu) {
        return true;
    }
    if (!boundaries) {
        return ab_fail(error, "Missing menu exit boundaries");
    }
    if (!method(menu, AB_UI_MENU_EXIT, 0, 0, error) ||
        !dynamic_assets(menu, true, boundaries, error)) {
        return false;
    }
    return ab_string_map_set(manager->menus, selected, 0, false, error);
}

bool ab_ui_menu_change_root(AbUiMenuManager *manager, AbUiMenuNode *menu,
                            const AbSpan *channel, const AbUiMenuBoundaries *boundaries,
                            AbError *error) {
    if (!manager) {
        return ab_fail(error, "Missing menu manager");
    }
    AbSpan selected = channel_name(manager, channel);
    if (!menu && !ab_ui_menu_exit_root(manager, &selected, boundaries, error)) {
        return false;
    }
    return ab_string_map_set(manager->queue, selected, (uintptr_t)menu, menu != NULL,
                             error);
}

bool ab_ui_menu_get_root(const AbUiMenuManager *manager, const AbSpan *channel,
                         AbUiMenuNode **output, AbError *error) {
    if (!manager || !output) {
        return ab_fail(error, "Invalid menu root query");
    }
    AbSpan selected = channel_name(manager, channel);
    AbUiMenuNode *menu;
    if (!node_get(manager->queue, selected, &menu, error) ||
        (!menu && !node_get(manager->menus, selected, &menu, error))) {
        return false;
    }
    *output = menu;
    return true;
}

bool ab_ui_menu_is_active(const AbUiMenuManager *manager, const AbSpan *channel,
                          bool *output, AbError *error) {
    if (!manager || !output) {
        return ab_fail(error, "Invalid active menu query");
    }
    AbUiMenuNode *menu;
    if (!node_get(manager->menus, channel_name(manager, channel), &menu, error)) {
        return false;
    }
    *output = menu != NULL;
    return true;
}

bool ab_ui_menu_set_root_visible(AbUiMenuManager *manager, const AbSpan *channel,
                                 AbUiPointerValue requested, AbError *error) {
    (void)requested;
    AbUiMenuNode *menu = NULL;
    if (!ab_ui_menu_get_root(manager, channel, &menu, error)) {
        return false;
    }
    if (!menu || !menu->frame) {
        return ab_fail(error, "No root to set visible");
    }
    menu->frame->fields.visible =
        (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN, .boolean = true};
    return true;
}

static bool visit_current(AbUiMenuManager *manager, AbUiMenuMethod operation,
                          AbError *error) {
    if (!manager) {
        return ab_fail(error, "Missing menu manager");
    }
    size_t cursor = 0;
    AbStringMapEntry entry;
    for (;;) {
        if (!ab_string_map_next(manager->menus, &cursor, &entry, error)) {
            return false;
        }
        if (!entry.present) {
            return true;
        }
        if (!method((AbUiMenuNode *)entry.value, operation, 0, 0, error)) {
            return false;
        }
    }
}

bool ab_ui_menu_resume(AbUiMenuManager *manager, AbError *error) {
    return visit_current(manager, AB_UI_MENU_LAYOUT, error);
}

bool ab_ui_menu_pause(AbUiMenuManager *manager, AbError *error) {
    return visit_current(manager, AB_UI_MENU_CLEAR_POINTER_FLAGS, error);
}

bool ab_ui_menu_update(AbUiMenuManager *manager, float seconds, float game_time,
                       const AbUiMenuBoundaries *boundaries, AbError *error) {
    if (!manager) {
        return ab_fail(error, "Missing menu manager");
    }
    if (!ab_ui_pointer_value_truthy(&manager->update_enabled)) {
        return true;
    }
    if (!boundaries || !finite_point(boundaries->screen_size) || !isfinite(seconds) ||
        !isfinite(game_time)) {
        return ab_fail(error, "Invalid menu update geometry/time");
    }
    bool changed = false;
    size_t cursor = 0;
    AbStringMapEntry entry;
    for (;;) {
        if (!ab_string_map_next(manager->queue, &cursor, &entry, error)) {
            return false;
        }
        if (!entry.present) {
            break;
        }
        if (!ab_ui_menu_exit_root(manager, &entry.key, boundaries, error) ||
            !ab_ui_menu_set_root(manager, (AbUiMenuNode *)entry.value, &entry.key,
                                 boundaries, error) ||
            !ab_string_map_set(manager->queue, entry.key, 0, false, error)) {
            return false;
        }
        changed = true;
    }
    bool resized = manager->screen_size.x != boundaries->screen_size.x ||
                   manager->screen_size.y != boundaries->screen_size.y;
    if (resized) {
        manager->screen_size = boundaries->screen_size;
    }
    for (size_t index = 0; index < manager->menu_count; ++index) {
        AbUiMenuNode *menu;
        if (!node_get(manager->menus, manager->menu_order[index], &menu, error)) {
            return false;
        }
        if (menu && ((resized && !method(menu, AB_UI_MENU_LAYOUT, 0, 0, error)) ||
                     !method(menu, AB_UI_MENU_UPDATE, seconds, game_time, error))) {
            return false;
        }
    }
    if (changed) {
        return true;
    }
    if (!boundaries->reset_filtered_input || !boundaries->delegate_clicks ||
        !boundaries->delegate_keys) {
        return ab_fail(error, "Missing menu update input boundaries");
    }
    return boundaries->reset_filtered_input(boundaries->context, manager, error) &&
           boundaries->delegate_clicks(boundaries->context, error) &&
           boundaries->delegate_keys(boundaries->context, error);
}

bool ab_ui_menu_draw(AbUiMenuManager *manager, const AbUiMenuBoundaries *boundaries,
                     AbError *error) {
    if (!manager || !boundaries || !finite_point(boundaries->screen_size) ||
        !boundaries->screen_clip) {
        return ab_fail(error, "Missing menu draw clip boundary");
    }
    if (!boundaries->screen_clip(boundaries->context, (AbVec2){0, 0},
                                 boundaries->screen_size, error)) {
        return false;
    }
    AbUiMenuNode *root;
    if (!node_get(manager->menus, manager->root_channel, &root, error)) {
        return false;
    }
    if (root && root->background_present) {
        for (size_t channel = 0; channel < 3; ++channel) {
            if (!isfinite(root->background[channel])) {
                return ab_fail(error, "Nonfinite menu draw background");
            }
        }
        if (!boundaries->background_color) {
            return ab_fail(error, "Missing menu background color boundary");
        }
        if (!boundaries->background_color(boundaries->context, root->background,
                                          error)) {
            return false;
        }
    }
    for (size_t index = 0; index < manager->menu_count; ++index) {
        AbUiMenuNode *menu;
        if (!node_get(manager->menus, manager->menu_order[index], &menu, error) ||
            (menu && !method(menu, AB_UI_MENU_DRAW, 0, 0, error))) {
            return false;
        }
    }
    return true;
}
