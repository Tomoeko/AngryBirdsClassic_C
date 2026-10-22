#ifndef AB_ORACLE_UI_FONT_H
#define AB_ORACLE_UI_FONT_H

#include "ui_init_cases.h"
#include "ui_value_probe.h"

static const char *const ui_font_names[] = {"_ab_font_getter_0", "_ab_font_getter_1",
                                            "_ab_font_getter_2"};
static uint32_t ui_font_responses[3][2], ui_font_events[3][7], ui_font_event_count;

static void ui_font_push(void *lua, const uint32_t value[2]) {
    if (value[0] == 5) {
        if (value[1] >= 3) {
            fail("Original font getter token exceeds fixture bounds\n");
        }
        damage_lua.get_field(lua, -10002, ui_font_names[value[1]]);
    } else {
        ui_reference_value_push(lua, value[0], value[1], ui_init_texts,
                                sizeof(ui_init_texts) / sizeof(*ui_init_texts));
    }
}

static void ui_font_read(void *lua, int index, uint32_t output[2]) {
    if (damage_lua.type(lua, index) != 6) {
        ui_reference_value_read(lua, index, output, ui_init_texts,
                                sizeof(ui_init_texts) / sizeof(*ui_init_texts));
        return;
    }
    const void *function = damage_lua.to_pointer(lua, index);
    int top = damage_lua.get_top(lua);
    for (uint32_t token = 0; token < 3; ++token) {
        damage_lua.get_field(lua, -10002, ui_font_names[token]);
        int match = function == damage_lua.to_pointer(lua, -1);
        damage_lua.set_top(lua, top);
        if (match) {
            output[0] = 5;
            output[1] = token;
            return;
        }
    }
    fail("Unexpected original font function identity\n");
}

static uint32_t *ui_font_event(uint32_t kind) {
    if (ui_font_event_count >= 3) {
        fail("Original font getter event capacity exceeded\n");
    }
    uint32_t *event = ui_font_events[ui_font_event_count++];
    for (size_t index = 0; index < 7; ++index) {
        event[index] = 0;
    }
    event[0] = kind;
    return event;
}

static int ui_font_getter(void *lua) {
    uint32_t token = (uint32_t)damage_lua.to_number(lua, -10003);
    if (token >= 3 || damage_lua.get_top(lua)) {
        fail("Original font getter was called with arguments\n");
    }
    uint32_t *event = ui_font_event(5);
    event[1] = 5;
    event[2] = token;
    event[5] = ui_font_responses[token][0];
    event[6] = ui_font_responses[token][1];
    ui_font_push(lua, event + 5);
    return 1;
}

static int ui_font_select(void *lua) {
    if (damage_lua.get_top(lua) != 1) {
        fail("Unexpected original font selection argument count\n");
    }
    ui_font_read(lua, 1, ui_font_event(2) + 1);
    return 0;
}

static int ui_font_measure(void *lua) {
    if (damage_lua.get_top(lua) != 1 || !damage_named(lua, 1, "")) {
        fail("Unexpected original font fixture measure input\n");
    }
    uint32_t *event = ui_font_event(4);
    ui_font_read(lua, 1, event + 1);
    event[5] = 2;
    damage_lua.push_number(lua, 0);
    return 1;
}

static void ui_font_run(void *lua, const uint32_t command[10]) {
    damage_lua.get_field(lua, -10002, "gamelua");
    if (command[1] == 2) {
        damage_function(lua, 1, "setFont", ui_font_select);
    } else if (command[1] == 1) {
        damage_lua.push_boolean(lua, 0);
        damage_lua.set_field(lua, 1, "setFont");
    } else {
        damage_lua.push_nil(lua);
        damage_lua.set_field(lua, 1, "setFont");
    }
    damage_lua.set_top(lua, 0);
    ui_font_event_count = 0;
    const char *methods[] = {"text-init", "text-font", "text-layout"};
    damage_lua.get_field(lua, -10002, methods[command[0]]);
    damage_lua.get_field(lua, -10002, "_ab_font_state");
    int arguments = 1;
    if (command[0] == 0) {
        damage_lua.create_table(lua, 0, 4);
        damage_lua.push_string(lua, "named");
        damage_lua.set_field(lua, 3, "name");
        damage_lua.push_string(lua, "");
        damage_lua.set_field(lua, 3, "text");
        damage_lua.push_string(lua, "");
        damage_lua.set_field(lua, 3, "group");
        ui_font_push(lua, command + 2);
        damage_lua.set_field(lua, 3, "font");
        arguments = 2;
    } else if (command[0] == 1) {
        ui_font_push(lua, command + 2);
        arguments = 2;
    }
    uint32_t status = damage_lua.call(lua, arguments, 0, 0) != 0;
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "_ab_font_state");
    uint32_t output[40];
    for (size_t field = 0; field < 20; ++field) {
        damage_lua.get_field(lua, 1, ui_init_fields[field]);
        ui_font_read(lua, 2, output + field * 2);
        damage_lua.set_top(lua, 1);
    }
    uint32_t metadata[] = {status, ui_font_event_count};
    print_words(metadata, 2);
    print_words(output, 40);
    for (size_t event = 0; event < ui_font_event_count; ++event) {
        print_words(ui_font_events[event], 7);
    }
    damage_lua.set_top(lua, 0);
}

static void ui_font_value(uint32_t *random, uint32_t output[2]) {
    output[0] = (random_word(random) >> 16) % 6;
    uint32_t value = random_word(random) >> 16;
    static const uint32_t numbers[] = {0,          0x80000000, 0x3f800000, 0x7fc12345,
                                       0x7f800000, 0xff800000, 1,          0xbf800000};
    output[1] = output[0] == 1   ? value & 1
                : output[0] == 2 ? numbers[value % 8]
                : output[0] == 3 ? value % 11
                : output[0] == 4 ? value % 17 + 1
                : output[0] == 5 ? value % 3
                                 : 0;
}

static void probe_ui_font(void) {
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original UI font getter state\n");
    }
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    const char *names[] = {"frame-init", "text-init", "text-font", "text-layout"};
    const char *paths[] = {"/data/local/tmp/ab520/ui/frame-init-oracle.lua",
                           "/data/local/tmp/ab520/ui/text-init-oracle.lua",
                           "/data/local/tmp/ab520/ui/text-font-oracle.lua",
                           "/data/local/tmp/ab520/ui/text-layout-oracle.lua"};
    for (size_t index = 0; index < 4; ++index) {
        load_selected_function(lua, paths[index], names[index]);
    }
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 1);
    damage_lua.get_field(lua, -10002, "frame-init");
    damage_lua.set_field(lua, 2, "init");
    damage_lua.set_field(lua, 1, "Frame");
    damage_lua.set_field(lua, -10002, "ui");
    damage_lua.create_table(lua, 0, 3);
    damage_lua.push_number(lua, 480);
    damage_lua.set_field(lua, 1, "screenWidth");
    damage_lua.push_string(lua, "font-basic");
    damage_lua.set_field(lua, 1, "fontBasic");
    damage_lua.set_field(lua, -10002, "gamelua");
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 1, "getStringWidth", ui_font_measure);
    damage_lua.set_field(lua, -10002, "res");
    for (uint32_t token = 0; token < 3; ++token) {
        damage_lua.push_number(lua, (float)token);
        damage_lua.push_closure(lua, ui_font_getter, 1);
        damage_lua.set_field(lua, -10002, ui_font_names[token]);
    }
    uint32_t random = 0x520f0173;
    semihost(4, "AB_ANDROID_UI_FONT_V1 64 64\n");
    for (size_t instance = 0; instance < 64; ++instance) {
        uint32_t setup[4];
        ui_font_value(&random, setup);
        ui_font_value(&random, setup + 2);
        print_words(setup, 4);
        damage_lua.create_table(lua, 0, 20);
        ui_font_push(lua, setup);
        damage_lua.set_field(lua, 1, "font");
        ui_font_push(lua, setup + 2);
        damage_lua.set_field(lua, 1, "getFontFunction");
        damage_lua.set_field(lua, -10002, "_ab_font_state");
        for (size_t step = 0; step < 64; ++step) {
            uint32_t command[10] = {(random_word(&random) >> 16) % 3,
                                    (random_word(&random) >> 16) % 3};
            for (size_t value = 0; value < 4; ++value) {
                ui_font_value(&random, command + 2 + value * 2);
            }
            for (size_t token = 0; token < 3; ++token) {
                ui_font_responses[token][0] = command[4 + token * 2];
                ui_font_responses[token][1] = command[5 + token * 2];
            }
            print_words(command, 10);
            ui_font_run(lua, command);
        }
    }
    damage_lua.close_state(lua);
    semihost(4, "AB UI font reference complete\n");
}

#endif
