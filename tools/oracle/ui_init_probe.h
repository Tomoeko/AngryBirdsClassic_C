#ifndef AB_ORACLE_UI_INIT_H
#define AB_ORACLE_UI_INIT_H

#include "ui_init_cases.h"
#include "ui_value_probe.h"

static uint32_t ui_init_input[44], ui_init_events[4][7], ui_init_event_count;
static void (*ui_init_push_value)(void *, int);
static unsigned (*ui_init_length)(void *, int);

static void ui_init_read(void *lua, int index, uint32_t output[2]) {
    ui_reference_value_read(lua, index, output, ui_init_texts,
                            sizeof(ui_init_texts) / sizeof(*ui_init_texts));
}

static void ui_init_push(void *lua, const uint32_t value[2]) {
    ui_reference_value_push(lua, value[0], value[1], ui_init_texts,
                            sizeof(ui_init_texts) / sizeof(*ui_init_texts));
}

static uint32_t *ui_init_event(uint32_t kind) {
    if (ui_init_event_count >= 4) {
        fail("Original UI initialization event overflow\n");
    }
    uint32_t *event = ui_init_events[ui_init_event_count++];
    for (size_t index = 0; index < 7; ++index) {
        event[index] = 0;
    }
    event[0] = kind;
    return event;
}

static int ui_init_name(void *lua) {
    if (damage_lua.get_top(lua) != 1 || damage_lua.type(lua, 1) != 5) {
        fail("Unexpected original Frame default name argument\n");
    }
    uint32_t *event = ui_init_event(1);
    event[5] = 3;
    event[6] = 5;
    damage_lua.push_string(lua, ui_init_texts[5]);
    return 1;
}

static int ui_init_font(void *lua) {
    if (damage_lua.get_top(lua) != 1) {
        fail("Unexpected original Text initial font arguments\n");
    }
    ui_init_read(lua, 1, ui_init_event(2) + 1);
    return 0;
}

static int ui_init_localize(void *lua) {
    if (damage_lua.get_top(lua) != 2) {
        fail("Unexpected original Text localization arguments\n");
    }
    uint32_t *event = ui_init_event(3);
    ui_init_read(lua, 1, event + 1);
    ui_init_read(lua, 2, event + 3);
    event[5] = ui_init_input[40];
    event[6] = ui_init_input[41];
    ui_init_push(lua, event + 5);
    return 1;
}

static int ui_init_measure(void *lua) {
    if (damage_lua.get_top(lua) != 1) {
        fail("Unexpected original Text measurement arguments\n");
    }
    uint32_t *event = ui_init_event(4);
    ui_init_read(lua, 1, event + 1);
    event[5] = ui_init_input[42];
    event[6] = ui_init_input[43];
    ui_init_push(lua, event + 5);
    return 1;
}

static void ui_init_arguments(void *lua, uint32_t mode) {
    if (mode != 2) {
        if (mode == 1) {
            damage_lua.push_boolean(lua, 0);
        } else {
            damage_lua.push_nil(lua);
        }
        return;
    }
    damage_lua.create_table(lua, 0, 20);
    int table = damage_lua.get_top(lua);
    for (size_t index = 0; index < 16; ++index) {
        ui_init_push(lua, ui_init_input + index * 2);
        damage_lua.set_field(lua, table, ui_init_fields[index]);
    }
    damage_lua.push_number(lua, 999);
    damage_lua.set_field(lua, table, "height");
    damage_lua.push_boolean(lua, 1);
    damage_lua.set_field(lua, table, "unrecognizedArgument");
}

static void ui_init_existing(void *lua) {
    damage_lua.create_table(lua, 0, 24);
    int object = damage_lua.get_top(lua);
    ui_init_push(lua, ui_init_input + 32);
    damage_lua.set_field(lua, object, "font");
    ui_init_push(lua, ui_init_input + 34);
    damage_lua.set_field(lua, object, "getFontFunction");
    damage_lua.push_boolean(lua, 1);
    damage_lua.set_field(lua, object, "width");
    damage_lua.push_boolean(lua, 0);
    damage_lua.set_field(lua, object, "floorCoordinates");
    damage_lua.push_number(lua, -1);
    damage_lua.set_field(lua, object, "fontLeadingScale");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.push_boolean(lua, 1);
    damage_lua.set_field(lua, -2, "old");
    ui_init_push_value(lua, -1);
    damage_lua.set_field(lua, object, "children");
    damage_lua.set_field(lua, object, "_lookup");
}

static void ui_init_run(void *lua, const uint32_t setup[4]) {
    damage_lua.create_table(lua, 0, 3);
    ui_init_push(lua, ui_init_input + 36);
    damage_lua.set_field(lua, 1, "screenWidth");
    ui_init_push(lua, ui_init_input + 38);
    damage_lua.set_field(lua, 1, "fontBasic");
    if (setup[3] == 2) {
        damage_function(lua, 1, "setFont", ui_init_font);
    } else if (setup[3] == 1) {
        damage_lua.push_boolean(lua, 0);
        damage_lua.set_field(lua, 1, "setFont");
    }
    damage_lua.set_field(lua, -10002, "gamelua");
    ui_init_event_count = 0;
    const char *method = setup[0] ? "text-init" : "frame-init";
    if (setup[1]) {
        damage_lua.get_field(lua, -10002, "class-new");
        damage_lua.create_table(lua, 0, 2);
        damage_lua.get_field(lua, -10002, method);
        damage_lua.set_field(lua, 2, "init");
        ui_init_arguments(lua, setup[2]);
        if (damage_lua.call(lua, 2, 1, 0)) {
            const char *message = damage_lua.to_string(lua, -1, NULL);
            fail(message ? message : "Original UI class construction failed\n");
        }
    } else {
        ui_init_existing(lua);
        damage_lua.set_field(lua, -10002, "_ab_init_object");
        damage_lua.get_field(lua, -10002, method);
        damage_lua.get_field(lua, -10002, "_ab_init_object");
        ui_init_arguments(lua, setup[2]);
        if (damage_lua.call(lua, 2, 0, 0)) {
            const char *message = damage_lua.to_string(lua, -1, NULL);
            fail(message ? message : "Original UI initialization failed\n");
        }
        damage_lua.get_field(lua, -10002, "_ab_init_object");
    }
    uint32_t output[40];
    for (size_t index = 0; index < 20; ++index) {
        damage_lua.get_field(lua, 1, ui_init_fields[index]);
        ui_init_read(lua, 2, output + index * 2);
        damage_lua.set_top(lua, 1);
    }
    print_words(output, 40);
    damage_lua.get_field(lua, 1, "children");
    damage_lua.get_field(lua, 1, "_lookup");
    if (damage_lua.type(lua, 2) != 5 || damage_lua.type(lua, 3) != 5) {
        fail("Original Frame did not allocate child and lookup tables\n");
    }
    uint32_t metadata[5] = {ui_init_length(lua, 2), ui_init_length(lua, 3),
                            damage_lua.to_pointer(lua, 2) !=
                                damage_lua.to_pointer(lua, 3),
                            0, ui_init_event_count};
    damage_lua.set_top(lua, 1);
    damage_lua.get_field(lua, 1, "height");
    damage_lua.get_field(lua, 1, "unrecognizedArgument");
    metadata[3] = damage_lua.type(lua, 2) == 0 && damage_lua.type(lua, 3) == 0;
    print_words(metadata, 5);
    for (size_t index = 0; index < ui_init_event_count; ++index) {
        print_words(ui_init_events[index], 7);
    }
    damage_lua.set_top(lua, 0);
}

static void probe_ui_init(void) {
    initialize_damage_lua();
    ui_init_push_value =
        (void (*)(void *, int))find_function("_Z13lua_pushvalueP9lua_Statei");
    ui_init_length =
        (unsigned (*)(void *, int))find_function("_Z10lua_objlenP9lua_Statei");
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original UI initialization state\n");
    }
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    load_selected_function(lua, "/data/local/tmp/ab520/ui/class-new-oracle.lua",
                           "class-new");
    load_selected_function(lua, "/data/local/tmp/ab520/ui/frame-init-oracle.lua",
                           "frame-init");
    load_selected_function(lua, "/data/local/tmp/ab520/ui/text-init-oracle.lua",
                           "text-init");
    damage_function(lua, -10002, "tostring", ui_init_name);
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 1);
    damage_lua.get_field(lua, -10002, "frame-init");
    damage_lua.set_field(lua, 2, "init");
    damage_lua.set_field(lua, 1, "Frame");
    damage_lua.set_field(lua, -10002, "ui");
    damage_lua.create_table(lua, 0, 2);
    damage_function(lua, 1, "getString", ui_init_localize);
    damage_function(lua, 1, "getStringWidth", ui_init_measure);
    damage_lua.set_field(lua, -10002, "res");
    static const uint32_t numbers[] = {
        0, 0x80000000, 0x3f800000, 0xbf800000, 0x7fc12345, 0x7f800000, 1, 0x43f00000,
    };
    uint32_t random = 0x520c7011;
    semihost(4, "AB_ANDROID_UI_INIT_V1 4096\n");
    for (size_t test = 0; test < 4096; ++test) {
        uint32_t setup[] = {(uint32_t)(test & 1), (uint32_t)((test >> 1) & 1),
                            test % 11 < 9 ? 2 : (uint32_t)(test % 2),
                            (uint32_t)((test >> 2) % 3)};
        for (size_t index = 0; index < 22; ++index) {
            uint32_t kind = (random_word(&random) >> 16) % 5;
            uint32_t value = random_word(&random) >> 16;
            uint32_t payload = kind == 1   ? value & 1
                               : kind == 2 ? numbers[value % 8]
                               : kind == 3 ? value % 11
                               : kind == 4 ? value % 17 + 1
                                           : 0;
            ui_init_input[index * 2] = kind;
            ui_init_input[index * 2 + 1] = payload;
        }
        print_words(setup, 4);
        print_words(ui_init_input, 44);
        ui_init_run(lua, setup);
    }
    damage_lua.close_state(lua);
    semihost(4, "AB UI initialization reference complete\n");
}

#endif
