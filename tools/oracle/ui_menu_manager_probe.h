#ifndef AB_ORACLE_UI_MENU_MANAGER_H
#define AB_ORACLE_UI_MENU_MANAGER_H

#include "ui_value_probe.h"
#if defined(AB_ORACLE_UI_MENU_KEYS)
#include "ui_menu_key_cases.h"
#endif

#if defined(AB_ORACLE_UI_MENU_INPUT)
enum { UI_MENU_STATE_WORDS = 151, UI_MENU_CALLBACK_WORDS = 159 };
#elif defined(AB_ORACLE_UI_MENU_NAVIGATION)
enum { UI_MENU_STATE_WORDS = 144, UI_MENU_CALLBACK_WORDS = 152 };
#elif defined(AB_ORACLE_UI_MENU_LINKS)
enum { UI_MENU_STATE_WORDS = 92, UI_MENU_CALLBACK_WORDS = 100 };
#else
enum { UI_MENU_STATE_WORDS = 72, UI_MENU_CALLBACK_WORDS = 80 };
#endif

static const char *const ui_menu_channels[] = {"root", "overlay", "dialog", ""};
static const char *const ui_menu_texts[] = {"", "root", "overlay", "dialog", "flag"};
static const char *const ui_menu_nodes[] = {"_ab_menu_one", "_ab_menu_two",
                                            "_ab_menu_three", "_ab_menu_four"};
static uint32_t ui_menu_command[16], ui_menu_order[6], ui_menu_events[256][159];
static uint32_t ui_menu_event_count, ui_menu_dummy;
static unsigned ui_menu_require_count;
static const void *ui_menu_node_pointers[4];
static void (*ui_menu_push_value)(void *, int);
static void (*ui_menu_raw_seti)(void *, int, int);
static void (*ui_menu_raw_geti)(void *, int, int);
static unsigned (*ui_menu_length)(void *, int);
static int (*ui_menu_gc)(void *, int, int);
#if defined(AB_ORACLE_UI_MENU_INPUT)
static const char *const ui_menu_pointer_events[] = {
    "LPRESS", "LHOLD", "LRELEASE", "WHEEL", "HOVER", "", "custom"};
#endif
#if defined(AB_ORACLE_UI_MENU_KEYS)
static uint8_t ui_menu_key_down[3][72];
static const char *const ui_menu_key_devices[] = {"android", "windows", "osx", "roku",
                                                  "other"};
#endif

static uint32_t ui_menu_node_id(void *lua, int index) {
    if (damage_lua.type(lua, index) == 0) {
        return 0;
    }
    const void *pointer = damage_lua.to_pointer(lua, index);
    for (uint32_t node = 0; node < 4; ++node) {
        if (pointer == ui_menu_node_pointers[node]) {
            return node + 1;
        }
    }
    fail("Original menu controller selected an unknown menu object\n");
}

static uint32_t ui_menu_channel_id(void *lua, int index) {
    for (uint32_t channel = 0; channel < 4; ++channel) {
        if (damage_named(lua, index, ui_menu_channels[channel])) {
            return channel;
        }
    }
    fail("Original menu controller used an unknown channel\n");
}

static int ui_menu_identity(void *lua) {
    (void)lua;
    fail("Original menu controller invoked an identity-only flag\n");
}

static void ui_menu_push(void *lua, uint32_t kind, uint32_t payload) {
    if (kind == 5) {
        damage_lua.get_field(lua, -10002, "_ab_menu_identity");
    } else {
        ui_reference_value_push(lua, kind, payload, ui_menu_texts, 5);
    }
}

static void ui_menu_value(void *lua, int index, uint32_t output[2]) {
    if (damage_lua.type(lua, index) == 6) {
        output[0] = 5;
        output[1] = 0;
    } else {
        ui_reference_value_read(lua, index, output, ui_menu_texts, 5);
    }
}

static uint32_t ui_menu_key_id(uint32_t pointer) {
    const uint32_t *string = (const uint32_t *)(uintptr_t)pointer;
    const unsigned char *bytes = (const unsigned char *)(string + 4);
    const uint32_t lengths[] = {4, 7, 6, 0};
    for (uint32_t key = 0; key < 4; ++key) {
        if (string[3] != lengths[key]) {
            continue;
        }
        uint32_t offset = 0;
        while (offset < lengths[key] &&
               bytes[offset] == (unsigned char)ui_menu_channels[key][offset]) {
            ++offset;
        }
        if (offset == lengths[key]) {
            return key;
        }
    }
    fail("Original menu map contains an unknown pinned string\n");
}

static void ui_menu_map(void *lua, int index, uint32_t output[23], int links) {
    for (size_t field = 0; field < 23; ++field) {
        output[field] = 0;
    }
    const uint32_t *table = damage_lua.to_pointer(lua, index);
    uint32_t capacity = table[4] == ui_menu_dummy
                            ? 0
                            : UINT32_C(1) << ((const unsigned char *)table)[7];
    if (capacity > 4 || table[7]) {
        fail("Original menu map exceeds prepared channel storage\n");
    }
    output[1] = capacity;
    output[2] = capacity ? (table[5] - table[4]) / 20 : 0;
    const uint32_t *nodes = (const uint32_t *)(uintptr_t)table[4];
    for (uint32_t slot = 0; slot < capacity; ++slot) {
        const uint32_t *node = nodes + slot * 5;
        uint32_t *record = output + 3 + slot * 5;
        record[4] = node[3];
        if (node[3]) {
            if (node[3] != 4 && node[3] != 11) {
                fail("Original menu channel key changed to an unsupported type\n");
            }
            record[0] = ui_menu_key_id(node[2]) + 1;
        }
        if (node[1]) {
            if (node[1] != 5) {
                fail("Original menu map has a non-object root\n");
            }
            record[1] = 1;
            if (links) {
                int top = damage_lua.get_top(lua);
                damage_lua.get_field(lua, index, ui_menu_channels[record[0] - 1]);
                damage_lua.get_field(lua, top + 1, "type");
                if (!damage_named(lua, top + 2, "link")) {
                    fail("Original menu link record changed type\n");
                }
                damage_lua.set_top(lua, top + 1);
                damage_lua.get_field(lua, top + 1, "instance");
                record[2] = ui_menu_node_id(lua, top + 2);
                damage_lua.set_top(lua, top);
            } else {
                for (uint32_t menu = 0; menu < 4; ++menu) {
                    if ((const void *)(uintptr_t)node[0] ==
                        ui_menu_node_pointers[menu]) {
                        record[2] = menu + 1;
                    }
                }
                if (!record[2]) {
                    fail("Original menu map selected an unknown object\n");
                }
            }
            ++output[0];
        }
        record[3] = node[4] ? (node[4] - table[4]) / 20 + 1 : 0;
    }
}

static void ui_menu_capture(void *lua, uint32_t output[151]) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, "_ab_menu_manager");
    int self = top + 1;
    const char *flags[] = {"allowInput", "updateEnabled", "delegateBackClicks"};
    for (size_t field = 0; field < 3; ++field) {
        damage_lua.get_field(lua, self, flags[field]);
        ui_menu_value(lua, self + 1, output + field * 2);
        damage_lua.set_top(lua, self);
    }
    const char *dimensions[] = {"screenWidth", "screenHeight"};
    for (size_t field = 0; field < 2; ++field) {
        damage_lua.get_field(lua, self, dimensions[field]);
        FloatBits value = {.number = damage_lua.to_number(lua, self + 1)};
        output[6 + field] = value.bits;
        damage_lua.set_top(lua, self);
    }
    damage_lua.get_field(lua, self, "menu_order");
    output[8] = ui_menu_length(lua, self + 1);
    if (output[8] > 6) {
        fail("Original menu order exceeded prepared storage\n");
    }
    for (uint32_t index = 0; index < 6; ++index) {
        output[9 + index] = 0;
        if (index < output[8]) {
            ui_menu_raw_geti(lua, self + 1, (int)index + 1);
            output[9 + index] = ui_menu_channel_id(lua, self + 2) + 1;
            damage_lua.set_top(lua, self + 1);
        }
    }
    damage_lua.set_top(lua, self);
    for (size_t node = 0; node < 4; ++node) {
        damage_lua.get_field(lua, -10002, ui_menu_nodes[node]);
        damage_lua.get_field(lua, self + 1, "visible");
        ui_menu_value(lua, self + 2, output + 15 + node * 2);
        damage_lua.set_top(lua, self);
    }
    damage_lua.get_field(lua, self, "menus");
    ui_menu_map(lua, self + 1, output + 23, 0);
    damage_lua.set_top(lua, self);
    damage_lua.get_field(lua, self, "menu_queue");
    ui_menu_map(lua, self + 1, output + 46, 0);
    damage_lua.set_top(lua, self);
    damage_lua.get_field(lua, self, "links");
    ui_menu_map(lua, self + 1, output + 69, 1);
    damage_lua.set_top(lua, top);
#if defined(AB_ORACLE_UI_MENU_NAVIGATION)
    for (size_t node = 0; node < 4; ++node) {
        uint32_t *row = output + 92 + node * 13;
        for (size_t field = 0; field < 13; ++field) {
            row[field] = 0;
        }
        damage_lua.get_field(lua, -10002, ui_menu_nodes[node]);
        int object = top + 1;
        damage_lua.get_field(lua, object, "entered");
        ui_menu_value(lua, object + 1, row);
        damage_lua.set_top(lua, object);
        damage_lua.get_field(lua, object, "_parent");
        row[2] = ui_menu_node_id(lua, object + 1);
        damage_lua.set_top(lua, object);
        damage_lua.get_field(lua, object, "children");
        row[3] = ui_menu_length(lua, object + 1);
        if (row[3] > 4) {
            fail("Original menu frame exceeds unique child storage\n");
        }
        for (uint32_t child = 0; child < row[3]; ++child) {
            ui_menu_raw_geti(lua, object + 1, (int)child + 1);
            row[4 + child] = ui_menu_node_id(lua, object + 2);
            damage_lua.set_top(lua, object + 1);
        }
        damage_lua.set_top(lua, object);
        damage_lua.get_field(lua, object, "_lookup");
        for (size_t child = 0; child < 4; ++child) {
            damage_lua.get_field(lua, object + 1, ui_menu_nodes[child]);
            if (damage_lua.type(lua, object + 2) != 0) {
                damage_lua.get_field(lua, object + 2, "child");
                if (ui_menu_node_id(lua, object + 3) != child + 1) {
                    fail("Original menu frame lookup changed child identity\n");
                }
                damage_lua.set_top(lua, object + 2);
                damage_lua.get_field(lua, object + 2, "index");
                row[8 + child] = (uint32_t)damage_lua.to_number(lua, object + 3);
            }
            damage_lua.set_top(lua, object + 1);
        }
        damage_lua.set_top(lua, object);
        damage_lua.get_field(lua, object, "removeFromFrame");
        row[12] = damage_lua.type(lua, object + 1) == 6;
        damage_lua.set_top(lua, top);
    }
#endif
#if defined(AB_ORACLE_UI_MENU_INPUT)
    damage_lua.get_field(lua, -10002, "_ab_menu_manager");
    damage_lua.get_field(lua, top + 1, "input_order");
    output[144] = ui_menu_length(lua, top + 2);
    if (output[144] > 6) {
        fail("Original menu input order exceeds prepared storage\n");
    }
    for (uint32_t index = 0; index < 6; ++index) {
        output[145 + index] = 0;
        if (index < output[144]) {
            ui_menu_raw_geti(lua, top + 2, (int)index + 1);
            output[145 + index] = ui_menu_channel_id(lua, top + 3) + 1;
            damage_lua.set_top(lua, top + 2);
        }
    }
    damage_lua.set_top(lua, top);
#endif
}

static uint32_t *ui_menu_event(void *lua, uint32_t kind, uint32_t menu,
                               uint32_t channel) {
    if (ui_menu_event_count >= 256) {
        fail("Original menu callbacks exceed prepared storage\n");
    }
    if (ui_menu_command[10] & 2) {
        (void)ui_menu_gc(lua, 2, 0);
        (void)ui_menu_gc(lua, 0, 0);
    }
    uint32_t *record = ui_menu_events[ui_menu_event_count++];
    for (size_t field = 0; field < 159; ++field) {
        record[field] = 0;
    }
    record[0] = kind;
    record[1] = menu;
    record[2] = channel;
    ui_menu_capture(lua, record + 8);
    return record;
}

static int ui_menu_method(void *lua, uint32_t kind) {
    int arguments = kind == 4 || kind == 5 ? 3 : 1;
    if (damage_lua.get_top(lua) != arguments) {
        fail("Original menu node method received unexpected arguments\n");
    }
    uint32_t menu = ui_menu_node_id(lua, 1);
    uint32_t *event = ui_menu_event(lua, kind, menu, 0);
    if (arguments == 3) {
        for (size_t index = 0; index < 2; ++index) {
            FloatBits value = {.number = damage_lua.to_number(lua, (int)index + 2)};
            event[4 + index] = value.bits;
        }
    }
    return 0;
}

static int ui_menu_prepare(void *lua) {
    return ui_menu_method(lua, 0);
}

static int ui_menu_enter(void *lua) {
    return ui_menu_method(lua, 1);
}

static int ui_menu_exit(void *lua) {
    return ui_menu_method(lua, 2);
}

static int ui_menu_layout(void *lua) {
    return ui_menu_method(lua, 3);
}

static int ui_menu_update(void *lua) {
    return ui_menu_method(lua, 4);
}

static int ui_menu_draw(void *lua) {
    return ui_menu_method(lua, 5);
}

static int ui_menu_clear(void *lua) {
    return ui_menu_method(lua, 6);
}

#if defined(AB_ORACLE_UI_MENU_INPUT)
static int ui_menu_pointer(void *lua) {
    if (damage_lua.get_top(lua) != 4 || ui_menu_command[12] >= 7 ||
        !damage_named(lua, 2, ui_menu_pointer_events[ui_menu_command[12]])) {
        fail("Original menu pointer arguments changed\n");
    }
    uint32_t node = ui_menu_node_id(lua, 1);
    uint32_t *event = ui_menu_event(lua, 19, node, 0);
    event[4] = ui_menu_command[12];
    for (size_t axis = 0; axis < 2; ++axis) {
        FloatBits value = {.number = damage_lua.to_number(lua, (int)axis + 3)};
        event[5 + axis] = value.bits;
    }
    uint32_t kind = (ui_menu_command[13] % 6 + node) % 6;
    uint32_t payload = ui_menu_command[14];
    if (kind == 0 || kind == 5) {
        payload = 0;
    } else if (kind == 1) {
        payload &= 1;
    } else if (kind == 3) {
        payload %= 5;
    } else if (kind == 4) {
        payload %= 32;
    }
    ui_menu_push(lua, kind, payload);
    /* Auxiliary results must not control continuation. */
    damage_lua.push_boolean(lua, 1);
    damage_lua.get_field(lua, -10002, ui_menu_nodes[3]);
    return 3;
}
#endif

static int ui_menu_requirements(void *lua) {
    if (damage_lua.get_top(lua) != 1) {
        fail("Original menu requirements arity changed\n");
    }
    uint32_t *event = ui_menu_event(lua, 7, ui_menu_node_id(lua, 1), 0);
    event[4] = ui_menu_command[4];
    event[5] = ui_menu_command[5];
    ui_menu_push(lua, ui_menu_command[4], ui_menu_command[5]);
    return 1;
}

static int ui_menu_dynamic_call(void *lua, uint32_t kind) {
    if (damage_lua.get_top(lua) != 1) {
        fail("Original menu dynamic asset call arity changed\n");
    }
    uint32_t *event = ui_menu_event(lua, kind, 0, 0);
    ui_menu_value(lua, 1, event + 4);
    return 0;
}

static int ui_menu_load(void *lua) {
    return ui_menu_dynamic_call(lua, 8);
}

static int ui_menu_release(void *lua) {
    return ui_menu_dynamic_call(lua, 9);
}

static int ui_menu_numeric_call(void *lua, uint32_t kind, int count) {
    if (damage_lua.get_top(lua) != count) {
        fail("Original menu numeric boundary arity changed\n");
    }
    uint32_t *event = ui_menu_event(lua, kind, 0, 0);
    for (int index = 0; index < count; ++index) {
        if (damage_lua.type(lua, index + 1) != 3) {
            fail("Original menu boundary omitted a number\n");
        }
        FloatBits value = {.number = damage_lua.to_number(lua, index + 1)};
        event[4 + index] = value.bits;
    }
    return 0;
}

static int ui_menu_rectangle(void *lua) {
    return ui_menu_numeric_call(lua, 10, 4);
}

static int ui_menu_notify(void *lua) {
    if (damage_lua.get_top(lua) != 2 || damage_lua.type(lua, 2) != 5) {
        fail("Original menu notification event is not a table\n");
    }
    damage_lua.get_field(lua, 2, "id");
#if defined(AB_ORACLE_UI_MENU_NAVIGATION)
    float id = damage_lua.to_number(lua, 3);
    if (id == 2346 || id == 2349) {
        damage_lua.set_top(lua, 2);
        uint32_t frame = 0, channel = 0;
        if (id == 2346) {
            damage_lua.get_field(lua, 2, "frame");
            frame = ui_menu_node_id(lua, 3);
            damage_lua.set_top(lua, 2);
            damage_lua.get_field(lua, 2, "menu");
            channel = ui_menu_channel_id(lua, 3);
        } else {
            if (ui_menu_length(lua, 2)) {
                fail("Original frame removal added positional event data\n");
            }
            damage_lua.get_field(lua, 2, "menu");
            if (damage_lua.type(lua, 3) != 0) {
                fail("Original frame removal unexpectedly captured a channel\n");
            }
        }
        (void)ui_menu_event(lua, id == 2346 ? 17 : 18, frame, channel);
        return 0;
    }
#endif
    if (damage_lua.to_number(lua, 3) != 2345) {
        fail("Original menu notification selected a different event\n");
    }
    damage_lua.set_top(lua, 2);
    damage_lua.get_field(lua, 2, "root");
    uint32_t menu = ui_menu_node_id(lua, 3);
    damage_lua.set_top(lua, 2);
    damage_lua.get_field(lua, 2, "menu");
    uint32_t channel = ui_menu_channel_id(lua, 3);
    (void)ui_menu_event(lua, 11, menu, channel);
    return 0;
}

static int ui_menu_clip(void *lua) {
    return ui_menu_numeric_call(lua, 12, 4);
}

static int ui_menu_background(void *lua) {
    return ui_menu_numeric_call(lua, 13, 3);
}

static int ui_menu_input_call(void *lua, uint32_t kind) {
    if (damage_lua.get_top(lua) != 1 || damage_lua.type(lua, 1) != 5) {
        fail("Original menu input boundary omitted self\n");
    }
    if (kind == 14) {
        ui_menu_push_value(lua, 1);
        damage_lua.set_field(lua, -10002, "_ab_menu_manager");
    }
    (void)ui_menu_event(lua, kind, 0, 0);
    return 0;
}

static int ui_menu_reset_input(void *lua) {
    return ui_menu_input_call(lua, 14);
}

static int ui_menu_clicks(void *lua) {
    return ui_menu_input_call(lua, 15);
}

static int ui_menu_keys(void *lua) {
#if defined(AB_ORACLE_UI_MENU_KEYS)
    (void)ui_menu_input_call(lua, 16);
    damage_lua.get_field(lua, -10002, "_ab_menu_key_original");
    ui_menu_push_value(lua, 1);
    if (damage_lua.call(lua, 1, 0, 0)) {
        const char *message = damage_lua.to_string(lua, -1, NULL);
        fail(message ? message : "Original menu key delegate failed\n");
    }
    return 0;
#else
    return ui_menu_input_call(lua, 16);
#endif
}

#if defined(AB_ORACLE_UI_MENU_KEYS)
static int ui_menu_key_method(void *lua) {
    if (damage_lua.get_top(lua) != 3) {
        fail("Original menu key method arity changed\n");
    }
    uint32_t phase = 3, label = UI_KEY_LABEL_COUNT;
    const char *phases[] = {"PRESS", "HOLD", "RELEASE"};
    for (uint32_t index = 0; index < 3; ++index) {
        if (damage_named(lua, 2, phases[index])) {
            phase = index;
        }
    }
    for (uint32_t index = 0; index < UI_KEY_LABEL_COUNT; ++index) {
        if (damage_named(lua, 3, ui_key_names[index])) {
            label = index;
        }
    }
    if (phase >= 3 || label >= UI_KEY_LABEL_COUNT) {
        fail("Original menu key method selected an unknown phase/label\n");
    }
    uint32_t node = ui_menu_node_id(lua, 1);
    uint32_t kind = (ui_menu_command[14] % 6 + node) % 6;
    uint32_t payload = ui_menu_command[15];
    if (kind == 0 || kind == 5) {
        payload = 0;
    } else if (kind == 1) {
        payload &= 1;
    } else if (kind == 3) {
        payload %= 5;
    } else if (kind == 4) {
        payload %= 32;
    }
    uint32_t *event = ui_menu_event(lua, 20, node, 0);
    event[4] = phase;
    event[5] = label;
    event[6] = kind;
    event[7] = payload;
#if defined(AB_ORACLE_UI_MENU_FRAME_KEYS)
    if (node <= 2) {
        damage_lua.get_field(lua, -10002, "_ab_menu_frame_key");
        for (int index = 1; index <= 3; ++index) {
            ui_menu_push_value(lua, index);
        }
        if (damage_lua.call(lua, 3, 3, 0)) {
            const char *message = damage_lua.to_string(lua, -1, NULL);
            fail(message ? message : "Original frame key dispatch failed\n");
        }
        ui_menu_value(lua, 4, event + 6);
        uint32_t *reply_event = ui_menu_event(lua, 21, node, 0);
        for (size_t value = 0; value < 3; ++value) {
            ui_menu_value(lua, (int)value + 4, reply_event + 2 + value * 2);
        }
        return 3;
    }
#endif
    ui_menu_push(lua, kind, payload);
    damage_lua.push_boolean(lua, 1);
    damage_lua.push_nil(lua);
    return 3;
}
#endif

static int ui_menu_require(void *lua) {
    const char *paths[] = {"lua/lua.lua", "lua/Class.lua", "lua/Class.lua"};
    if (ui_menu_require_count >= 3 || damage_lua.get_top(lua) != 1 ||
        !damage_named(lua, 1, paths[ui_menu_require_count++])) {
        fail("Original menu dependency request changed\n");
    }
    return 0;
}

static void ui_menu_create_nodes(void *lua) {
    const char *methods[] = {
        "prepareForDynamicAssets", "onEntry", "onExit", "layout", "update", "draw",
        "clearPointerFlags"};
    int (*callbacks[])(void *) = {ui_menu_prepare, ui_menu_enter,  ui_menu_exit,
                                  ui_menu_layout,  ui_menu_update, ui_menu_draw,
                                  ui_menu_clear};
    for (size_t node = 0; node < 4; ++node) {
        damage_lua.create_table(lua, 0, 9);
        for (size_t method = 0; method < 7; ++method) {
            damage_function(lua, 1, methods[method], callbacks[method]);
        }
#if defined(AB_ORACLE_UI_MENU_INPUT)
        damage_function(lua, 1, "onPointerEvent", ui_menu_pointer);
#endif
#if defined(AB_ORACLE_UI_MENU_KEYS)
        damage_function(lua, 1, "onKeyEvent", ui_menu_key_method);
#endif
#if defined(AB_ORACLE_UI_MENU_FRAME_KEYS)
        damage_lua.push_boolean(lua, 1);
        damage_lua.set_field(lua, 1, "visible");
        damage_lua.push_boolean(lua, 1);
        damage_lua.set_field(lua, 1, "active");
#endif
#if defined(AB_ORACLE_UI_MENU_NAVIGATION)
        damage_lua.push_string(lua, ui_menu_nodes[node]);
        damage_lua.set_field(lua, 1, "name");
        damage_lua.create_table(lua, 0, 0);
        damage_lua.set_field(lua, 1, "children");
        damage_lua.create_table(lua, 0, 0);
        damage_lua.set_field(lua, 1, "_lookup");
        const char *tree_methods[] = {"addChild", "removeChild", "popChild"};
        for (size_t method = 0; method < 3; ++method) {
            damage_lua.get_field(lua, -10002, "ui");
            damage_lua.get_field(lua, 2, "Frame");
            damage_lua.get_field(lua, 3, tree_methods[method]);
            damage_lua.set_field(lua, 1, tree_methods[method]);
            damage_lua.set_top(lua, 1);
        }
#endif
        damage_lua.create_table(lua, 0, 4);
        const char *components[] = {"r", "g", "b", "a"};
        for (size_t component = 0; component < 4; ++component) {
            damage_lua.push_number(lua, (float)(51 * (component + 1)));
            damage_lua.set_field(lua, 2, components[component]);
        }
        damage_lua.set_field(lua, 1, "backgroundColour");
        ui_menu_node_pointers[node] = damage_lua.to_pointer(lua, 1);
        damage_lua.set_field(lua, -10002, ui_menu_nodes[node]);
    }
}

static void ui_menu_globals(void *lua) {
    damage_lua.create_table(lua, 0, 8);
    damage_function(lua, 1, "drawFullscreenRect", ui_menu_rectangle);
    damage_function(lua, 1, "setScreenClipRect", ui_menu_clip);
    damage_function(lua, 1, "setBGColor", ui_menu_background);
    damage_lua.create_table(lua, 0, 3);
    damage_function(lua, 2, "getRequirements", ui_menu_requirements);
    damage_function(lua, 2, "load", ui_menu_load);
    damage_function(lua, 2, "delayrelease", ui_menu_release);
    damage_lua.set_field(lua, 1, "dynamic");
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 2, "notify", ui_menu_notify);
    damage_lua.set_field(lua, 1, "eventManager");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.push_number(lua, 2345);
    damage_lua.set_field(lua, 2, "EID_MENUMANAGER_ROOT_CHANGED");
#if defined(AB_ORACLE_UI_MENU_NAVIGATION)
    const char *event_names[] = {"EID_FRAME_PUSHED", "EID_CHANGE_SCENE",
                                 "EID_PUSH_FRAME", "EID_POP_FRAME"};
    for (size_t index = 0; index < 4; ++index) {
        damage_lua.push_number(lua, (float)(2346 + index));
        damage_lua.set_field(lua, 2, event_names[index]);
    }
#endif
    damage_lua.set_field(lua, 1, "events");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 1);
    damage_lua.push_number(lua, 0);
    damage_lua.set_field(lua, 3, "entryCounts");
    damage_lua.set_field(lua, 2, "Frame");
    damage_lua.set_field(lua, 1, "ui");
    damage_lua.set_field(lua, -10002, "gamelua");
    /* Keep all channel strings alive across nil writes and collection. */
    damage_lua.create_table(lua, 4, 0);
    for (size_t channel = 0; channel < 4; ++channel) {
        damage_lua.push_string(lua, ui_menu_channels[channel]);
        ui_menu_raw_seti(lua, 1, (int)channel + 1);
    }
    damage_lua.set_field(lua, -10002, "_ab_menu_channels");
}

static void ui_menu_push_node(void *lua, uint32_t node) {
    if (!node) {
        damage_lua.push_nil(lua);
    } else if (node <= 4) {
        damage_lua.get_field(lua, -10002, ui_menu_nodes[node - 1]);
    } else {
        fail("Original menu fixture root index exceeds nodes\n");
    }
}

static void ui_menu_push_channel(void *lua) {
    if (ui_menu_command[3] == 1) {
        damage_lua.push_nil(lua);
    } else if (ui_menu_command[3] == 2) {
        damage_lua.push_boolean(lua, 0);
    } else {
        damage_lua.push_string(lua, ui_menu_channels[ui_menu_command[2]]);
    }
}

static void ui_menu_command_globals(void *lua) {
    damage_number(lua, -10002, "screenWidth", ui_menu_command[8]);
    damage_number(lua, -10002, "screenHeight", ui_menu_command[9]);
    damage_lua.get_field(lua, -10002, "gamelua");
    damage_number(lua, 1, "screenWidth", ui_menu_command[8]);
    damage_number(lua, 1, "screenHeight", ui_menu_command[9]);
    damage_lua.push_boolean(lua, (int)(ui_menu_command[10] & 1));
    damage_lua.set_field(lua, 1, "useDynamicAssets");
#if defined(AB_ORACLE_UI_MENU_KEYS)
    ui_key_fixture(ui_menu_command, ui_menu_key_down);
    damage_lua.push_string(lua, ui_menu_key_devices[ui_menu_command[12] % 5]);
    damage_lua.set_field(lua, 1, "deviceModel");
    damage_lua.push_boolean(lua, (int)((ui_menu_command[13] >> 16) & 1));
    damage_lua.set_field(lua, 1, "showEditor");
    const char *key_tables[] = {"keyPressed", "keyHold", "keyReleased"};
    for (size_t phase = 0; phase < 3; ++phase) {
        damage_lua.create_table(lua, 0, 16);
        for (size_t key = 0; key < UI_KEY_INPUT_COUNT; ++key) {
            if (ui_menu_key_down[phase][key]) {
                damage_lua.push_boolean(lua, 1);
                damage_lua.set_field(lua, 2, ui_key_names[key]);
            }
        }
        damage_lua.set_field(lua, 1, key_tables[phase]);
    }
#endif
    damage_lua.set_top(lua, 0);
}

static void ui_menu_order_fixture(void *lua) {
    damage_lua.get_field(lua, -10002, "_ab_menu_manager");
    damage_lua.create_table(lua, (int)ui_menu_command[11], 0);
    for (uint32_t index = 0; index < ui_menu_command[11]; ++index) {
        damage_lua.push_string(lua, ui_menu_channels[ui_menu_order[index]]);
        ui_menu_raw_seti(lua, 2, (int)index + 1);
    }
    damage_lua.set_field(lua, 1,
                         ui_menu_command[0] == 24 ? "input_order" : "menu_order");
    damage_lua.set_top(lua, 0);
}

static void ui_menu_node_fixture(void *lua) {
    damage_lua.get_field(lua, -10002, ui_menu_nodes[ui_menu_command[1] - 1]);
    ui_menu_push(lua, ui_menu_command[4], ui_menu_command[5]);
    damage_lua.set_field(lua, 1, "visible");
    if (ui_menu_command[2] & 1) {
        damage_lua.create_table(lua, 0, 4);
        const char *components[] = {"r", "g", "b", "a"};
        for (size_t index = 0; index < 4; ++index) {
            damage_number(lua, 2, components[index], ui_menu_command[12 + index]);
        }
    } else {
        damage_lua.push_boolean(lua, 0);
    }
    damage_lua.set_field(lua, 1, "backgroundColour");
    damage_lua.set_top(lua, 0);
}

#if defined(AB_ORACLE_UI_MENU_NAVIGATION)
static void ui_menu_navigation_fixture(void *lua) {
    if (ui_menu_command[0] == 22) {
        damage_lua.get_field(lua, -10002, "_ab_menu_manager");
        ui_menu_push(lua, ui_menu_command[4], ui_menu_command[5]);
        damage_lua.set_field(lua, 1, "delegateBackClicks");
    } else {
        ui_menu_push_node(lua, ui_menu_command[1]);
        ui_menu_push(lua, ui_menu_command[4], ui_menu_command[5]);
        damage_lua.set_field(lua, 1, "entered");
    }
    damage_lua.set_top(lua, 0);
}

static void ui_menu_push_event_target(void *lua) {
    if (ui_menu_command[12] == 2) {
        damage_lua.push_string(lua, ui_menu_channels[ui_menu_order[0]]);
    } else {
        ui_menu_push_node(lua, ui_menu_command[12] ? ui_menu_command[1] : 0);
    }
}

static void ui_menu_avoid_duplicate_child(void *lua) {
    if (ui_menu_command[0] != 19 || ui_menu_command[11] != 2) {
        return;
    }
    damage_lua.get_field(lua, -10002, "_ab_menu_manager");
    damage_lua.get_field(lua, 1, "getRoot");
    ui_menu_push_value(lua, 1);
    ui_menu_push_channel(lua);
    if (damage_lua.call(lua, 2, 1, 0)) {
        fail("Original menu duplicate guard could not get root\n");
    }
    if (damage_lua.type(lua, 2) == 0) {
        damage_lua.set_top(lua, 0);
        return;
    }
    damage_lua.get_field(lua, 2, "_lookup");
    uint32_t child = ui_menu_command[12] ? ui_menu_command[1] : 0;
    if (ui_menu_command[12] == 2) {
        damage_lua.get_field(lua, 1, "links");
        damage_lua.get_field(lua, 4, ui_menu_channels[ui_menu_order[0]]);
        child = 0;
        if (damage_lua.type(lua, 5) != 0) {
            damage_lua.get_field(lua, 5, "instance");
            child = ui_menu_node_id(lua, 6);
        }
        damage_lua.set_top(lua, 3);
    }
    if (child) {
#if defined(AB_ORACLE_UI_MENU_FRAME_KEYS)
        if (child <= 2) {
            /* Base-key roots use custom leaf widgets3/4. Keep the prepared
             * one-level graph acyclic; arbitrary malformed cycles are excluded. */
            ui_menu_command[0] = 20;
        }
#endif
        damage_lua.get_field(lua, 3, ui_menu_nodes[child - 1]);
        if (damage_lua.type(lua, 4) != 0) {
            /* Duplicate-child assertion occurs after native array mutation;
             * malformed-table prefixes are outside the canonical tree model. */
            ui_menu_command[0] = 20;
        }
    }
    damage_lua.set_top(lua, 0);
}
#endif

static void ui_menu_run_command(void *lua) {
    static const char *const names[] = {"init",
                                        "setRoot",
                                        "exitCurrentRoot",
                                        "changeRoot",
                                        "getRoot",
                                        "isMenuActive",
                                        "setRootVisible",
                                        "update",
                                        "draw",
                                        "gameResumed",
                                        "gamePaused",
                                        "setAllowInput",
                                        "setUpdateEnabled",
                                        "",
                                        "",
                                        "deactivate",
                                        "addLink",
                                        "getLink",
                                        "handleResult",
                                        "eventTriggered",
                                        "popFrame",
                                        "removeFromFrame",
                                        "",
                                        "",
                                        "",
                                        "delegatePointerEvent",
                                        "delegateKeyEvents"};
    uint32_t operation = ui_menu_command[0], reply[3] = {1, 0, 0};
    if (operation >= sizeof(names) / sizeof(*names)) {
        fail("Original menu fixture command exceeds method table\n");
    }
    ui_menu_event_count = 0;
    if (operation == 13 || operation == 24) {
        ui_menu_order_fixture(lua);
    } else if (operation == 14) {
        ui_menu_node_fixture(lua);
#if defined(AB_ORACLE_UI_MENU_NAVIGATION)
    } else if (operation == 22 || operation == 23) {
        ui_menu_navigation_fixture(lua);
#endif
    } else {
        if (operation == 21) {
            ui_menu_push_node(lua, ui_menu_command[1]);
        } else {
            damage_lua.get_field(lua, -10002, "MenuManager");
        }
        damage_lua.get_field(lua, 1, names[operation]);
        ui_menu_push_value(lua, 2);
        damage_lua.set_field(lua, -10002, "_ab_menu_call");
        damage_lua.set_top(lua, 0);
        damage_lua.get_field(lua, -10002, "_ab_menu_call");
        damage_lua.get_field(lua, -10002, "_ab_menu_manager");
        int arguments = 1;
#if defined(AB_ORACLE_UI_MENU_NAVIGATION)
        if (operation == 19) {
            damage_lua.create_table(lua, 0, 3);
            const uint32_t event_ids[] = {0, 2347, 2348, 2349};
            damage_lua.push_number(lua, (float)event_ids[ui_menu_command[11]]);
            damage_lua.set_field(lua, 3, "id");
            ui_menu_push_event_target(lua);
            damage_lua.set_field(lua, 3, "target");
            ui_menu_push_channel(lua);
            damage_lua.set_field(lua, 3, "menu");
            ++arguments;
        } else
#endif
            if (operation == 25) {
#if defined(AB_ORACLE_UI_MENU_INPUT)
            damage_lua.push_string(lua, ui_menu_pointer_events[ui_menu_command[12]]);
            /* Coordinates are ordinary arguments, not global cursor fields. */
            for (size_t axis = 0; axis < 2; ++axis) {
                FloatBits value = {.bits = ui_menu_command[6 + axis]};
                damage_lua.push_number(lua, value.number);
            }
            arguments += 3;
#endif
        } else if (operation == 16) {
            damage_lua.push_string(lua, ui_menu_channels[ui_menu_order[0]]);
            ui_menu_push_node(lua, ui_menu_command[1]);
            arguments += 2;
        } else if (operation == 17 || operation == 18) {
            damage_lua.push_string(lua, ui_menu_channels[ui_menu_order[0]]);
            ++arguments;
            if (operation == 18) {
                ui_menu_push_channel(lua);
                ++arguments;
            }
        } else if (operation == 1 || operation == 3) {
            ui_menu_push_node(lua, ui_menu_command[1]);
            ui_menu_push_channel(lua);
            arguments += 2;
        } else if (operation == 2 || operation == 4 || operation == 5 ||
                   operation == 20) {
            ui_menu_push_channel(lua);
            ++arguments;
        } else if (operation == 6) {
            ui_menu_push(lua, ui_menu_command[4], ui_menu_command[5]);
            ui_menu_push_channel(lua);
            arguments += 2;
        } else if (operation == 7) {
            for (size_t index = 0; index < 2; ++index) {
                FloatBits value = {.bits = ui_menu_command[6 + index]};
                damage_lua.push_number(lua, value.number);
            }
            arguments += 2;
        } else if (operation == 11 || operation == 12) {
            ui_menu_push(lua, ui_menu_command[4], ui_menu_command[5]);
            ++arguments;
        }
        if (operation == 21) {
            damage_lua.set_top(lua, 1);
            arguments = 0;
        }
        reply[0] = damage_lua.call(lua, arguments, 1, 0) == 0;
        if (reply[0] && (operation == 4 || operation == 17 || operation == 20)) {
            reply[1] = damage_lua.type(lua, 1) == 0 ? 0 : 1;
            reply[2] = ui_menu_node_id(lua, 1);
        } else if (reply[0] && (operation == 5 || operation == 25)) {
            reply[1] = 2;
            reply[2] = (uint32_t)damage_lua.to_boolean(lua, 1);
        }
        damage_lua.set_top(lua, 0);
    }
    print_words(reply, 3);
    print_words(&ui_menu_event_count, 1);
    for (uint32_t index = 0; index < ui_menu_event_count; ++index) {
        print_words(ui_menu_events[index], UI_MENU_CALLBACK_WORDS);
    }
    uint32_t state[151];
    ui_menu_capture(lua, state);
    print_words(state, UI_MENU_STATE_WORDS);
}

static void ui_menu_random_value(uint32_t *random, uint32_t output[2]) {
    output[0] = random_word(random) % 6;
    output[1] = random_word(random);
    if (output[0] == 0 || output[0] == 5) {
        output[1] = 0;
    } else if (output[0] == 1) {
        output[1] &= 1;
    } else if (output[0] == 3) {
        output[1] %= 5;
    } else if (output[0] == 4) {
        output[1] %= 32;
    }
}

static void probe_ui_menu_manager(void) {
    initialize_damage_lua();
    ui_menu_push_value =
        (void (*)(void *, int))find_function("_Z13lua_pushvalueP9lua_Statei");
    ui_menu_raw_seti =
        (void (*)(void *, int, int))find_function("_Z11lua_rawsetiP9lua_Stateii");
    ui_menu_raw_geti =
        (void (*)(void *, int, int))find_function("_Z11lua_rawgetiP9lua_Stateii");
    ui_menu_length =
        (unsigned (*)(void *, int))find_function("_Z10lua_objlenP9lua_Statei");
    ui_menu_gc = (int (*)(void *, int, int))find_function("_Z6lua_gcP9lua_Stateii");
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original menu controller state\n");
    }
    (void)ui_menu_gc(lua, 0, 0);
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    damage_lua.create_table(lua, 0, 0);
    ui_menu_dummy = ((const uint32_t *)damage_lua.to_pointer(lua, 1))[4];
    damage_lua.set_top(lua, 0);
    damage_function(lua, -10002, "requireFile", ui_menu_require);
    damage_function(lua, -10002, "_ab_menu_identity", ui_menu_identity);
    ui_menu_globals(lua);
    const char *paths[] = {"/data/local/tmp/ab520/ui/menu-lua-base.lua",
                           "/data/local/tmp/ab520/ui/menu-class.lua",
                           "/data/local/tmp/ab520/ui/menu-manager.lua"};
    for (size_t index = 0; index < 3; ++index) {
        load_damage_chunk(lua, paths[index]);
    }
    if (ui_menu_require_count != 2) {
        fail("Original menu controller dependencies were not prepared\n");
    }
#if defined(AB_ORACLE_UI_MENU_NAVIGATION)
    int (*open_table)(void *) =
        (int (*)(void *))find_function("_Z13luaopen_tableP9lua_State");
    (void)open_table(lua);
    damage_lua.set_top(lua, 0);
    load_damage_chunk(lua, "/data/local/tmp/ab520/ui/menu-frame.lua");
    damage_lua.get_field(lua, -10002, "gamelua");
    damage_lua.get_field(lua, -10002, "ui");
    damage_lua.get_field(lua, 2, "Frame");
    damage_lua.push_number(lua, 0);
    damage_lua.set_field(lua, 3, "entryCounts");
#if defined(AB_ORACLE_UI_MENU_FRAME_KEYS)
    damage_lua.get_field(lua, 3, "onKeyEvent");
    damage_lua.set_field(lua, -10002, "_ab_menu_frame_key");
#endif
    damage_lua.set_top(lua, 2);
    damage_lua.set_field(lua, 1, "ui");
    damage_lua.set_top(lua, 0);
#endif
    damage_lua.get_field(lua, -10002, "MenuManager");
    damage_function(lua, 1, "resetFilteredInput", ui_menu_reset_input);
    damage_function(lua, 1, "delegateClicks", ui_menu_clicks);
#if defined(AB_ORACLE_UI_MENU_KEYS)
    damage_lua.get_field(lua, 1, "delegateKeyEvents");
    damage_lua.set_field(lua, -10002, "_ab_menu_key_original");
#endif
    damage_function(lua, 1, "delegateKeyEvents", ui_menu_keys);
    damage_lua.set_top(lua, 0);
    uint32_t random = UINT32_C(0x5206e001);
#if defined(AB_ORACLE_UI_MENU_FRAME_KEYS)
    semihost(4, "AB_ANDROID_UI_MENU_MANAGER_V7 32 128\n");
#elif defined(AB_ORACLE_UI_MENU_KEYS)
    semihost(4, "AB_ANDROID_UI_MENU_MANAGER_V6 32 128\n");
#elif defined(AB_ORACLE_UI_MENU_INPUT)
    semihost(4, "AB_ANDROID_UI_MENU_MANAGER_V5 32 128\n");
#elif defined(AB_ORACLE_UI_MENU_NAVIGATION)
    semihost(4, "AB_ANDROID_UI_MENU_MANAGER_V4 32 128\n");
#elif defined(AB_ORACLE_UI_MENU_LINKS)
    semihost(4, "AB_ANDROID_UI_MENU_MANAGER_V3 32 128\n");
#else
    semihost(4, "AB_ANDROID_UI_MENU_MANAGER_V2 32 128\n");
#endif
    for (uint32_t instance = 0; instance < 32; ++instance) {
        print_words(&instance, 1);
        for (size_t field = 0; field < 16; ++field) {
            ui_menu_command[field] = 0;
        }
        ui_menu_command[8] = UINT32_C(0x43f00000);
        ui_menu_command[9] = UINT32_C(0x43a00000);
        ui_menu_command_globals(lua);
        ui_menu_create_nodes(lua);
        ui_menu_event_count = 0;
        damage_lua.get_field(lua, -10002, "MenuManager");
        damage_lua.get_field(lua, 1, "new");
        damage_lua.set_field(lua, -10002, "_ab_menu_new");
        damage_lua.set_top(lua, 0);
        damage_lua.get_field(lua, -10002, "_ab_menu_new");
        damage_lua.get_field(lua, -10002, "MenuManager");
        if (damage_lua.call(lua, 1, 1, 0)) {
            const char *message = damage_lua.to_string(lua, -1, NULL);
            fail(message ? message : "Original menu constructor failed\n");
        }
        damage_lua.set_field(lua, -10002, "_ab_menu_manager");
        print_words(&ui_menu_event_count, 1);
        for (uint32_t index = 0; index < ui_menu_event_count; ++index) {
            print_words(ui_menu_events[index], UI_MENU_CALLBACK_WORDS);
        }
        uint32_t initial[151];
        ui_menu_capture(lua, initial);
        print_words(initial, UI_MENU_STATE_WORDS);
        for (size_t step = 0; step < 128; ++step) {
#if defined(AB_ORACLE_UI_MENU_KEYS)
            ui_menu_command[0] = random_word(&random) % 27;
#elif defined(AB_ORACLE_UI_MENU_INPUT)
            ui_menu_command[0] = random_word(&random) % 26;
#elif defined(AB_ORACLE_UI_MENU_NAVIGATION)
            ui_menu_command[0] = random_word(&random) % 24;
#elif defined(AB_ORACLE_UI_MENU_LINKS)
            ui_menu_command[0] = random_word(&random) % 19;
#else
            ui_menu_command[0] = random_word(&random) % 16;
#endif
            ui_menu_command[1] = random_word(&random) % 5;
            ui_menu_command[2] = random_word(&random) % 4;
            ui_menu_command[3] = random_word(&random) % 3;
            ui_menu_random_value(&random, ui_menu_command + 4);
            for (size_t field = 6; field < 16; ++field) {
                FloatBits value = {
                    .number =
                        (float)((int32_t)(random_word(&random) % 2049) - 1024) / 8};
                ui_menu_command[field] = value.bits;
            }
            FloatBits width = {.number = (float)(random_word(&random) % 5 + 1) * 96};
            FloatBits height = {.number = (float)(random_word(&random) % 5 + 1) * 64};
            ui_menu_command[8] = width.bits;
            ui_menu_command[9] = height.bits;
            ui_menu_command[10] = random_word(&random) % 4;
            ui_menu_command[11] = random_word(&random) % 7;
            for (size_t index = 0; index < 6; ++index) {
                ui_menu_order[index] = random_word(&random) % 4;
            }
            if (ui_menu_command[0] == 14 && !ui_menu_command[1]) {
                ui_menu_command[1] = 1;
            }
            if (step < 32) {
                const uint32_t operations[] = {1,  1, 1, 1, 13, 3, 3,  3,  3, 4,  5,
                                               6,  7, 7, 8, 10, 9, 11, 12, 7, 12, 3,
                                               15, 4, 5, 6, 1,  3, 7,  0,  8, 7};
                ui_menu_command[0] = operations[step];
                ui_menu_command[3] = 0;
                ui_menu_command[10] = instance % 4;
                if (step < 4) {
                    ui_menu_command[1] = (uint32_t)step + 1;
                    ui_menu_command[2] = (uint32_t)step;
                } else if (step == 4) {
                    ui_menu_command[11] = 6;
                    const uint32_t order[] = {2, 0, 1, 3, 0, 2};
                    for (size_t index = 0; index < 6; ++index) {
                        ui_menu_order[index] = order[index];
                    }
                } else if (step >= 5 && step <= 8) {
                    ui_menu_command[1] = 9 - (uint32_t)step;
                    ui_menu_command[2] = (uint32_t)step - 5;
                } else {
                    ui_menu_command[1] = step == 21 ? 2u : 1u;
                    ui_menu_command[2] = step == 27 ? 1u : 0u;
                    if (step == 27) {
                        ui_menu_command[1] = 0;
                    }
                }
                if (step == 11 || step == 17 || step == 18 || step == 25) {
                    ui_menu_command[4] = 1;
                    ui_menu_command[5] = 0;
                } else if (step == 20) {
                    ui_menu_command[4] = 2;
                    ui_menu_command[5] = 0;
                }
                if (step >= 12) {
                    FloatBits dense_width = {.number = step == 19 ? 1024 : 960};
                    FloatBits dense_height = {.number = step == 19 ? 768 : 640};
                    ui_menu_command[8] = dense_width.bits;
                    ui_menu_command[9] = dense_height.bits;
                }
            }
#if defined(AB_ORACLE_UI_MENU_LINKS)
            if (step >= 32 && step < 48) {
                const uint32_t operations[] = {16, 16, 16, 16, 17, 18, 7,  16,
                                               18, 17, 0,  17, 18, 16, 18, 7};
                ui_menu_command[0] = operations[step - 32];
                ui_menu_command[3] = 0;
                ui_menu_command[10] = instance % 4;
                if (step < 36) {
                    ui_menu_order[0] = (uint32_t)step - 32;
                    ui_menu_command[1] = 36 - (uint32_t)step;
                } else {
                    ui_menu_order[0] = step >= 45 ? 3u : 0u;
                    ui_menu_command[1] = step == 39 ? 0u : 2u;
                }
                ui_menu_command[2] = step >= 45 ? 1u : 0u;
            }
#endif
#if defined(AB_ORACLE_UI_MENU_NAVIGATION)
            if (step >= 48 && step < 80) {
                const uint32_t cases[32][6] = {
                    {0, 1, 0, 0, 0, 0},  {1, 1, 0, 0, 0, 0},  {1, 2, 1, 0, 0, 0},
                    {23, 1, 0, 0, 0, 0}, {19, 3, 0, 2, 1, 0}, {21, 3, 0, 0, 0, 0},
                    {20, 1, 0, 0, 0, 0}, {23, 1, 0, 0, 0, 0}, {16, 3, 0, 0, 0, 0},
                    {19, 3, 0, 2, 2, 0}, {19, 4, 1, 2, 1, 0}, {19, 1, 1, 2, 0, 0},
                    {16, 0, 1, 0, 0, 1}, {19, 1, 1, 2, 2, 1}, {19, 1, 1, 3, 0, 0},
                    {20, 1, 0, 0, 0, 0}, {3, 4, 0, 0, 0, 0},  {19, 3, 0, 2, 1, 0},
                    {19, 1, 0, 3, 0, 0}, {22, 1, 0, 0, 0, 0}, {19, 1, 0, 1, 0, 0},
                    {19, 1, 0, 1, 2, 2}, {16, 2, 0, 0, 0, 3}, {19, 1, 0, 1, 2, 3},
                    {4, 1, 0, 0, 0, 0},  {7, 1, 0, 0, 0, 0},  {21, 3, 0, 0, 0, 0},
                    {23, 2, 0, 0, 0, 0}, {19, 4, 0, 2, 1, 0}, {20, 1, 0, 0, 0, 0},
                    {19, 1, 0, 0, 0, 0}, {10, 1, 0, 0, 0, 0}};
                const uint32_t *item = cases[step - 48];
                ui_menu_command[0] = item[0];
                ui_menu_command[1] = item[1];
                ui_menu_command[2] = item[2];
                ui_menu_command[3] = 0;
                ui_menu_command[4] = 1;
                ui_menu_command[5] = step == 51 || step == 67 ? 0u : 1u;
                ui_menu_command[10] = instance % 4;
                ui_menu_command[11] = item[3];
                ui_menu_command[12] = item[4];
                ui_menu_order[0] = item[5];
                if (step == 75) {
                    ui_menu_command[4] = 2;
                    ui_menu_command[5] = 0;
                }
            }
#if defined(AB_ORACLE_UI_MENU_INPUT)
            if (step == 58) {
                ui_menu_command[2] = 0;
            }
            if (step >= 80 && step < 112) {
                const uint32_t cases[32][6] = {
                    {0, 1, 0, 0, 0, 0},  {1, 1, 0, 0, 0, 0},  {1, 2, 1, 0, 0, 0},
                    {1, 3, 2, 0, 0, 0},  {1, 4, 3, 0, 0, 0},  {24, 1, 0, 6, 0, 0},
                    {25, 1, 0, 0, 0, 0}, {25, 1, 0, 0, 5, 0}, {25, 1, 0, 0, 4, 0},
                    {25, 1, 0, 0, 1, 0}, {25, 1, 0, 0, 2, 0}, {25, 1, 0, 0, 3, 0},
                    {11, 1, 0, 0, 0, 0}, {3, 4, 0, 0, 0, 0},  {25, 1, 0, 0, 5, 0},
                    {24, 1, 0, 0, 0, 0}, {25, 1, 0, 0, 0, 0}, {0, 1, 0, 0, 0, 0},
                    {25, 1, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0},  {24, 1, 0, 6, 0, 0},
                    {25, 1, 0, 0, 5, 0}, {25, 1, 0, 0, 0, 0}, {25, 1, 0, 0, 1, 0},
                    {14, 1, 0, 0, 0, 0}, {25, 1, 0, 0, 1, 0}, {1, 0, 0, 0, 0, 0},
                    {25, 1, 0, 0, 0, 0}, {1, 2, 0, 0, 0, 0},  {25, 1, 0, 0, 4, 0},
                    {24, 1, 0, 0, 0, 0}, {25, 1, 0, 0, 0, 0}};
                const uint32_t *item = cases[step - 80];
                ui_menu_command[0] = item[0];
                ui_menu_command[1] = item[1];
                ui_menu_command[2] = item[2];
                ui_menu_command[3] = 0;
                ui_menu_command[4] = 1;
                ui_menu_command[5] = 0;
                ui_menu_command[10] = instance % 4;
                ui_menu_command[11] = item[3];
                ui_menu_command[12] = (uint32_t)(step + instance) % 7;
                ui_menu_command[13] = item[4];
                ui_menu_command[14] = item[5];
                if (step == 85 || step == 100) {
                    const uint32_t order[] = {0, 1, 2, 3, 0, 2};
                    for (size_t index = 0; index < 6; ++index) {
                        ui_menu_order[index] = step == 100 ? 0u : order[index];
                    }
                }
            }
            if (ui_menu_command[0] == 25) {
                ui_menu_command[12] %= 7;
            }
#if defined(AB_ORACLE_UI_MENU_KEYS)
            if (step >= 80 && step < 112) {
                const uint32_t cases[32][3] = {
                    {0, 1, 0},  {1, 1, 0},  {1, 2, 1},  {1, 3, 2},  {1, 4, 3},
                    {24, 1, 6}, {26, 1, 0}, {26, 1, 0}, {26, 1, 0}, {26, 1, 0},
                    {26, 1, 0}, {26, 1, 0}, {26, 1, 0}, {26, 1, 0}, {11, 1, 0},
                    {26, 1, 0}, {11, 1, 0}, {22, 1, 0}, {26, 1, 0}, {22, 1, 0},
                    {1, 0, 0},  {3, 2, 0},  {26, 1, 0}, {7, 1, 0},  {26, 1, 0},
                    {6, 1, 0},  {26, 1, 0}, {24, 1, 0}, {26, 1, 0}, {24, 1, 6},
                    {26, 1, 0}, {26, 1, 0}};
                const uint32_t *item = cases[step - 80];
                ui_menu_command[0] = item[0];
                ui_menu_command[1] = item[1];
                ui_menu_command[2] = step >= 82 && step <= 84 ? (uint32_t)step - 81 : 0;
                ui_menu_command[3] = 0;
                ui_menu_command[4] = 1;
                ui_menu_command[5] = step == 94 || step == 97 ? 0u : 1u;
                if (step == 96) {
                    ui_menu_command[4] = 2;
                    ui_menu_command[5] = 0;
                }
                ui_menu_command[10] = instance % 4;
                ui_menu_command[11] = item[2];
                ui_menu_command[12] = instance % 5;
                ui_menu_command[13] = (instance / 5 % 4) << 16;
                ui_menu_command[14] = (uint32_t)(instance + step) % 6;
                ui_menu_command[15] = (uint32_t)(step - 80) % 8;
                if (step == 85 || step == 109) {
                    const uint32_t order[] = {2, 0, 1, 3, 0, 2};
                    for (size_t index = 0; index < 6; ++index) {
                        ui_menu_order[index] = step == 109 ? 0u : order[index];
                    }
                }
            }
#endif
#endif
            if (ui_menu_command[0] == 19) {
                ui_menu_command[11] %= 4;
                ui_menu_command[12] %= 3;
            }
            if ((ui_menu_command[0] == 21 || ui_menu_command[0] == 23) &&
                !ui_menu_command[1]) {
                ui_menu_command[1] = 1;
            }
            ui_menu_command_globals(lua);
            ui_menu_avoid_duplicate_child(lua);
#endif
            print_words(ui_menu_command, 16);
            print_words(ui_menu_order, 6);
            ui_menu_command_globals(lua);
            ui_menu_run_command(lua);
        }
    }
    damage_lua.close_state(lua);
    semihost(4, "AB UI menu manager reference complete\n");
}

#endif
