#include "ab/classic_root_queries.h"

#include <string.h>

static bool ui_named(AbUiPointerValue value, const char *name) {
    size_t length = strlen(name);
    return value.kind == AB_UI_POINTER_TEXT && value.text.size == length &&
           (!length || (value.text.data && !memcmp(value.text.data, name, length)));
}

bool ab_classic_safe_to_quit(const AbUiMenuManager *menus, bool *output,
                             AbError *error) {
    if (!output)
        return ab_fail(error, "Missing safe-to-quit output");
    AbUiMenuNode *root = NULL;
    if (menus && !ab_ui_menu_get_root(menus, NULL, &root, error))
        return false;
    if (!root) {
        *output = true;
        return true;
    }
    if (!root->frame)
        return ab_fail(error, "Missing reached menu root Frame state");
    AbUiPointerValue name = root->frame->fields.name;
    if (ui_named(name, "mainMenu") || ui_named(name, "mainMenuRoot")) {
        AbUiFrameNode *tree = root->tree;
        if ((!tree && root->frame->child_count) ||
            (tree &&
             (tree->state != root->frame || root->frame->child_count > tree->capacity ||
              (root->frame->child_count && !tree->children))))
            return ab_fail(error, "Invalid reached safe-to-quit Frame tree");
        if (tree) {
            for (size_t index = 0; index < root->frame->child_count; ++index) {
                AbUiFrameNode *child = tree->children[index];
                if (!child || !child->state)
                    return ab_fail(error, "Missing reached safe-to-quit child");
                if (ab_ui_pointer_value_truthy(&child->state->fields.require_input)) {
                    *output = false;
                    return true;
                }
            }
        }
        *output = true;
    } else
        *output = ui_named(name, "ForceUpdatePopup");
    return true;
}

static AbDataValue text(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)value, strlen(value)}};
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL &&
           (value.kind != AB_DATA_BOOLEAN || value.bits != 0);
}

static bool valid_table(const AbDataTable *table) {
    return table && table->count <= table->capacity &&
           (!table->count || table->entries);
}

static AbDataValue field(const AbDataTable *table, const char *name) {
    const AbDataValue *value = ab_data_get(table, text(name));
    return value ? *value : (AbDataValue){0};
}

bool ab_classic_free_eagle(const AbClassicEagleQuery *query, bool *output,
                           AbError *error) {
    if (!query || !output || !valid_table(query->unlock_status) ||
        !query->current_episode || !query->clicked_from)
        return ab_fail(error, "Invalid free-eagle query binding");
    for (size_t index = 0; index < query->unlock_status->count; ++index) {
        AbDataValue record = query->unlock_status->entries[index].value;
        if (record.kind == AB_DATA_NIL)
            continue;
        if (record.kind != AB_DATA_TABLE || !valid_table(record.table))
            return ab_fail(error, "Invalid reached free-eagle unlock record");
        if (truthy(field(record.table, "enabled"))) {
            *output = true;
            return true;
        }
    }
    bool enabled = false;
    if (truthy(*query->current_episode)) {
        if (!valid_table(query->episodes))
            return ab_fail(error, "Missing reached free-eagle episodes table");
        const AbDataValue *episode =
            ab_data_get(query->episodes, *query->current_episode);
        if (!episode || episode->kind != AB_DATA_TABLE || !valid_table(episode->table))
            return ab_fail(error, "Missing reached free-eagle episode metadata");
        AbDataValue power = field(episode->table, "mightyPower");
        if (truthy(power)) {
            if (power.kind != AB_DATA_TABLE || !valid_table(power.table))
                return ab_fail(error, "Invalid reached free-eagle mightyPower record");
            AbDataValue origin = *query->clicked_from;
            const char *main_menu = "MAIN_MENU";
            bool main = origin.kind == AB_DATA_STRING &&
                        origin.string.size == strlen(main_menu) && origin.string.data &&
                        !memcmp(origin.string.data, main_menu, origin.string.size);
            enabled = truthy(field(power.table, "free")) && !main;
        }
    }
    *output = enabled;
    return true;
}

static bool valid_tree(const AbUiFrameNode *tree) {
    return tree && tree->state &&
           tree->state->child_count == tree->state->lookup_count &&
           tree->state->child_count <= tree->capacity &&
           (!tree->state->child_count || tree->children);
}

bool ab_classic_notifications_pausing(const AbUiFrameNode *notifications, bool *output,
                                      AbError *error) {
    if (!output || !valid_tree(notifications))
        return ab_fail(error, "Invalid notifications pause query binding");
    for (size_t index = 0; index < notifications->state->child_count; ++index) {
        const AbUiFrameNode *child = notifications->children[index];
        if (!child || !child->state)
            return ab_fail(error, "Missing reached notifications child");
        const AbUiFrameFields *fields = &child->state->fields;
        if (ab_ui_pointer_value_truthy(&fields->visible) &&
            ab_ui_pointer_value_truthy(&fields->notifications_frame_pause)) {
            *output = true;
            return true;
        }
    }
    *output = false;
    return true;
}

bool ab_classic_level_start_ingame(const AbUiMenuManager *menus,
                                   const AbUiFrameNode *notifications,
                                   AbDataValue *output, AbError *error) {
    if (!menus || !output)
        return ab_fail(error, "Missing level-start menu query binding");
    AbUiMenuNode *root = NULL;
    if (!ab_ui_menu_get_root(menus, NULL, &root, error))
        return false;
    if (root && !root->frame)
        return ab_fail(error, "Missing reached level-start root Frame");
    if (!root) {
        *output = (AbDataValue){0};
        return true;
    }
    if (!ui_named(root->frame->fields.name, "gameHud")) {
        *output = (AbDataValue){.kind = AB_DATA_BOOLEAN};
        return true;
    }
    AbUiFrameNode *tree = root->tree;
    if (!tree && !root->frame->child_count) {
        *output = (AbDataValue){0};
        return true;
    }
    if (!valid_tree(tree) || tree->state != root->frame)
        return ab_fail(error, "Invalid reached level-start Frame tree");
    AbUiFrameNode *pause = NULL;
    AbSpan name = {(const uint8_t *)"pausePage", 9};
    if (!ab_ui_frame_find_child(tree, &name, 1, &pause, error))
        return false;
    if (!pause) {
        *output = (AbDataValue){0};
        return true;
    }
    if (ab_ui_pointer_value_truthy(&pause->state->fields.visible)) {
        *output = (AbDataValue){.kind = AB_DATA_BOOLEAN};
        return true;
    }
    bool paused;
    if (!ab_classic_notifications_pausing(notifications, &paused, error))
        return false;
    *output = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = !paused};
    return true;
}

bool ab_classic_pause_menu_visible(const AbUiMenuManager *menus, bool *output,
                                   AbError *error) {
    if (!output)
        return ab_fail(error, "Missing pause-menu query output");
    AbUiMenuNode *root = NULL;
    if (menus && !ab_ui_menu_get_root(menus, NULL, &root, error))
        return false;
    if (!root) {
        *output = false;
        return true;
    }
    if (!root->frame)
        return ab_fail(error, "Missing reached pause-menu root Frame");
    if (!root->tree && !root->frame->child_count) {
        *output = false;
        return true;
    }
    if (!valid_tree(root->tree) || root->tree->state != root->frame)
        return ab_fail(error, "Invalid reached pause-menu Frame tree");
    AbUiFrameNode *pause = NULL;
    AbSpan name = {(const uint8_t *)"pausePage", 9};
    if (!ab_ui_frame_find_child(root->tree, &name, 1, &pause, error))
        return false;
    AbUiPointerValue show =
        pause ? pause->state->fields.pause_page_show : (AbUiPointerValue){0};
    *output = show.kind == AB_UI_POINTER_BOOLEAN && show.boolean;
    return true;
}

bool ab_classic_museum_in_level(const AbUiMenuManager *menus, AbDataValue *output,
                                AbError *error) {
    if (!menus || !output)
        return ab_fail(error, "Missing Museum level menu query binding");
    AbUiMenuNode *root = NULL;
    if (!ab_ui_menu_get_root(menus, NULL, &root, error))
        return false;
    if (!root) {
        *output = (AbDataValue){0};
        return true;
    }
    if (!root->frame)
        return ab_fail(error, "Missing reached Museum root Frame");
    bool paused = true;
    if (ui_named(root->frame->fields.name, "gameHud") &&
        !ab_classic_pause_menu_visible(menus, &paused, error))
        return false;
    *output = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = !paused};
    return true;
}
