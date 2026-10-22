#include "ab/ui_menu_keys.h"

#include "ab/data_order.h"

#include <string.h>

typedef struct {
    const char *input, *output;
} KeyMapping;

static const KeyMapping desktop_keys[] = {{"BACKSPACE", "BACKSPACE"},
                                          {"SPACE", "SPACE"},
                                          {"SUBTRACT", "-"},
                                          {"DASH", "-"},
                                          {"LEFT", "LEFT"},
                                          {"RIGHT", "RIGHT"},
                                          {"UP", "UP"},
                                          {"DOWN", "DOWN"},
                                          {"CONTROL", "CONTROL"},
                                          {"SHIFT", "SHIFT"},
                                          {"DELETE", "DELETE"},
                                          {"PAGEUP", "PAGEUP"},
                                          {"PAGEDOWN", "PAGEDOWN"},
                                          {"TAB", "TAB"},
                                          {"F1", "F1"},
                                          {"F2", "F2"},
                                          {"F3", "F3"},
                                          {"F4", "F4"},
                                          {"F5", "F5"},
                                          {"F6", "F6"},
                                          {"F7", "F7"},
                                          {"F8", "F8"},
                                          {"F9", "F9"},
                                          {"F10", "F10"},
                                          {"F11", "F11"},
                                          {"F12", "F12"}};

static const KeyMapping roku_keys[] = {{"PAUSE", "PAUSE"},
                                       {"REPLAY", "RESTART"},
                                       {"UP", "UP"},
                                       {"DOWN", "DOWN"},
                                       {"LEFT", "LEFT"},
                                       {"RIGHT", "RIGHT"},
                                       {"FASTFORWARD", "FORWARD"},
                                       {"REWIND", "BACKWARD"},
                                       {"KEY_GAMING_A", "BUTTON_A"},
                                       {"KEY_GAMING_B", "BUTTON_B"},
                                       {"KEY_MENU", "KEY_MENU"}};

static AbSpan text(const char *value) {
    return (AbSpan){(const uint8_t *)value, strlen(value)};
}

static bool key_order(const KeyMapping *keys, size_t count, size_t capacity,
                      size_t *indices, AbError *error) {
    AbDataEntry entries[26] = {0};
    AbStringOrderNode nodes[32];
    for (size_t index = 0; index < count; ++index) {
        entries[index].key =
            (AbDataValue){.kind = AB_DATA_STRING, .string = text(keys[index].input)};
        entries[index].value = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1};
    }
    AbDataTable table = {.entries = entries, .count = count, .capacity = count};
    AbStringOrderWorkspace workspace = {indices, count, nodes, capacity};
    return ab_data_string_order(&table, capacity, &workspace, error);
}

bool ab_ui_menu_key_layout(AbUiMenuKeyLayout *output, AbError *error) {
    if (!output) {
        return ab_fail(error, "Missing menu key layout output");
    }
    AbUiMenuKeyLayout layout = {0};
    if (!key_order(desktop_keys, 26, 32, layout.desktop_order, error) ||
        !key_order(roku_keys, 11, 16, layout.roku_order, error)) {
        return false;
    }
    layout.ready = true;
    *output = layout;
    return true;
}

static bool current(const AbUiMenuManagerView *view, AbSpan channel,
                    AbUiMenuNode **output, AbError *error) {
    uintptr_t value;
    bool present;
    if (!ab_string_map_get(view->menus, channel, &value, &present, error)) {
        return false;
    }
    *output = present ? (AbUiMenuNode *)value : NULL;
    return true;
}

static bool down(const AbUiMenuKeyBoundaries *boundaries, AbUiKeyPhase phase,
                 const char *key, bool *output, AbError *error) {
    if (!boundaries->down) {
        return ab_fail(error, "Missing menu key input boundary");
    }
    return boundaries->down(boundaries->context, phase, text(key), output, error);
}

static bool event(const AbUiMenuKeyBoundaries *boundaries, AbUiMenuNode *menu,
                  AbUiKeyPhase phase, AbSpan key, AbUiPointerReply *reply,
                  AbError *error) {
    static const char *const phases[] = {"PRESS", "HOLD", "RELEASE"};
    if (!menu || !boundaries->event) {
        return ab_fail(error, "Missing eligible menu key method");
    }
    return boundaries->event(boundaries->context, menu, text(phases[phase]), key, reply,
                             error);
}

static bool routed_release(const AbUiMenuManagerView *view,
                           const AbUiMenuKeyBoundaries *boundaries, const char *key,
                           const char *label, AbError *error) {
    for (size_t index = 0; index < view->input_count; ++index) {
        AbUiMenuNode *menu = NULL;
        bool pressed;
        if (!current(view, view->input_order[index], &menu, error) ||
            !down(boundaries, AB_UI_KEY_RELEASE, key, &pressed, error)) {
            return false;
        }
        if (!pressed || !menu) {
            continue;
        }
        AbUiPointerReply reply = {0};
        if (!event(boundaries, menu, AB_UI_KEY_RELEASE, text(label), &reply, error)) {
            return false;
        }
        if (!ab_ui_pointer_value_valid(reply.values)) {
            return ab_fail(error, "Invalid menu key primary reply");
        }
        if (ab_ui_pointer_value_truthy(reply.values)) {
            break;
        }
    }
    return true;
}

static bool root_key(const AbUiMenuManagerView *view,
                     const AbUiMenuKeyBoundaries *boundaries, AbUiKeyPhase phase,
                     const char *key, AbSpan label, bool *was_down, AbError *error) {
    if (!down(boundaries, phase, key, was_down, error)) {
        return false;
    }
    if (!*was_down) {
        return true;
    }
    AbUiMenuNode *menu = NULL;
    AbUiPointerReply reply = {0};
    return current(view, text("root"), &menu, error) &&
           event(boundaries, menu, phase, label, &reply, error);
}

static bool root_three_phases(const AbUiMenuManagerView *view,
                              const AbUiMenuKeyBoundaries *boundaries,
                              const KeyMapping *key, AbError *error) {
    bool active;
    for (unsigned phase = 0; phase < 3; ++phase) {
        if (!root_key(view, boundaries, (AbUiKeyPhase)phase, key->input,
                      text(key->output), &active, error)) {
            return false;
        }
    }
    return true;
}

static bool desktop(const AbUiMenuManagerView *view, const AbUiMenuKeyLayout *layout,
                    const AbUiMenuKeyBoundaries *boundaries, AbError *error) {
    bool control;
    if (!down(boundaries, AB_UI_KEY_HOLD, "CONTROL", &control, error)) {
        return false;
    }
    if (!control || boundaries->show_editor) {
        static const char *const characters[] = {
            "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L",
            "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X",
            "Y", "Z", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9"};
        for (size_t index = 0; index < 36; ++index) {
            KeyMapping key = {characters[index], characters[index]};
            if (!root_three_phases(view, boundaries, &key, error)) {
                return false;
            }
        }
    }
    for (size_t index = 0; index < 26; ++index) {
        if (layout->desktop_order[index] >= 26 ||
            !root_three_phases(view, boundaries,
                               desktop_keys + layout->desktop_order[index], error)) {
            return false;
        }
    }
    if (!down(boundaries, AB_UI_KEY_HOLD, "CONTROL", &control, error)) {
        return false;
    }
    if (control) {
        KeyMapping paste = {"V", "PASTE"};
        return root_three_phases(view, boundaries, &paste, error);
    }
    return true;
}

static bool roku(const AbUiMenuManagerView *view, const AbUiMenuKeyLayout *layout,
                 const AbUiMenuKeyBoundaries *boundaries, AbError *error) {
    bool active;
    if (!root_key(view, boundaries, AB_UI_KEY_HOLD, "UP", text("UP"), &active, error) ||
        !root_key(view, boundaries, AB_UI_KEY_HOLD, "DOWN", text("DOWN"), &active,
                  error)) {
        return false;
    }
    for (size_t index = 0; index < 11; ++index) {
        if (layout->roku_order[index] >= 11) {
            return ab_fail(error, "Invalid Roku key traversal index");
        }
        const KeyMapping *key = roku_keys + layout->roku_order[index];
        if (!root_key(view, boundaries, AB_UI_KEY_PRESS, key->input, text(key->output),
                      &active, error) ||
            (!active && !root_key(view, boundaries, AB_UI_KEY_RELEASE, key->input,
                                  text(key->output), &active, error))) {
            return false;
        }
    }
    return true;
}

bool ab_ui_menu_keys(AbUiMenuManager *manager, const AbUiMenuKeyLayout *layout,
                     const AbUiMenuKeyBoundaries *boundaries, AbError *error) {
    AbUiMenuManagerView view;
    if (!ab_ui_menu_manager_view(manager, &view)) {
        return ab_fail(error, "Missing menu key owner");
    }
    if (!ab_ui_pointer_value_truthy(&view.allow_input) ||
        !ab_ui_pointer_value_truthy(&view.delegate_back_clicks)) {
        return true;
    }
    AbUiMenuNode *root = NULL;
    if (!current(&view, text("root"), &root, error)) {
        return false;
    }
    if (!root) {
        return true;
    }
    if (!layout || !layout->ready || !boundaries ||
        boundaries->device > AB_UI_KEY_DEVICE_ROKU) {
        return ab_fail(error, "Invalid menu key layout or boundaries");
    }
    static const KeyMapping releases[] = {{"KEY_BACK", "BACK"},
                                          {"RETURN", "RETURN"},
                                          {"KEY_MENU", "KEY_MENU"},
                                          {"ESCAPE", "ESCAPE"}};
    for (size_t index = 0; index < 4; ++index) {
        if (!routed_release(&view, boundaries, releases[index].input,
                            releases[index].output, error)) {
            return false;
        }
    }
    if (boundaries->device == AB_UI_KEY_DEVICE_WINDOWS ||
        boundaries->device == AB_UI_KEY_DEVICE_OSX || boundaries->show_editor) {
        return desktop(&view, layout, boundaries, error);
    }
    return boundaries->device != AB_UI_KEY_DEVICE_ROKU ||
           roku(&view, layout, boundaries, error);
}
