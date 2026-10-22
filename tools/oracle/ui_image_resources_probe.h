#ifndef AB_ORACLE_UI_IMAGE_RESOURCES_H
#define AB_ORACLE_UI_IMAGE_RESOURCES_H

#include "ui_value_probe.h"

static const char *const ui_resource_texts[] = {"", "raw", "localized", "disabled",
                                                "alias"};
static const char *const ui_resource_numeric[] = {
    "w", "h", "px", "py", "rotatePivotX", "rotatePivotY", "pivotX", "pivotY", "angle"};
static uint32_t ui_resource_map[5], ui_resource_geometry[5][4];
static uint32_t ui_resource_calls[6][14], ui_resource_call_count;

static void ui_resource_value(void *lua, int table, const char *field,
                              uint32_t output[2]) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, table, field);
    ui_reference_value_read(lua, -1, output, ui_resource_texts, 5);
    damage_lua.set_top(lua, top);
}

static uint32_t ui_resource_number(void *lua, int table, const char *field) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, table, field);
    if (damage_lua.type(lua, -1) != 3) {
        fail("Missing original UI resource numeric state\n");
    }
    FloatBits value = {.number = damage_lua.to_number(lua, -1)};
    damage_lua.set_top(lua, top);
    return value.bits;
}

static uint32_t *ui_resource_boundary(void *lua, uint32_t kind) {
    if (ui_resource_call_count >= 6) {
        fail("Original UI resource boundary overflow\n");
    }
    uint32_t id = 5;
    for (uint32_t index = 0; index < 5; ++index) {
        if (damage_named(lua, 1, ui_resource_texts[index])) {
            id = index;
        }
    }
    if (id == 5) {
        fail("Unexpected original UI resource identifier\n");
    }
    uint32_t *record = ui_resource_calls[ui_resource_call_count++];
    record[0] = kind;
    record[1] = id;
    record[2] =
        kind == 0 ? ui_resource_map[id] : ui_resource_geometry[id][(kind - 1) * 2];
    record[3] = kind == 0 ? 0 : ui_resource_geometry[id][(kind - 1) * 2 + 1];
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, "_ab_resource");
    int self = top + 1;
    ui_resource_value(lua, self, "image", record + 4);
    for (size_t field = 0; field < 6; ++field) {
        record[6 + field] = ui_resource_number(lua, self, ui_resource_numeric[field]);
    }
    ui_resource_value(lua, self, "enabled", record + 12);
    damage_lua.set_top(lua, top);
    return record;
}

static int ui_resource_localize(void *lua) {
    uint32_t *record = ui_resource_boundary(lua, 0);
    damage_lua.push_string(lua, ui_resource_texts[record[2]]);
    return 1;
}

static int ui_resource_bounds(void *lua) {
    uint32_t *record = ui_resource_boundary(lua, 1);
    for (size_t field = 2; field < 4; ++field) {
        FloatBits value = {.bits = record[field]};
        damage_lua.push_number(lua, value.number);
    }
    return 2;
}

static int ui_resource_pivot(void *lua) {
    uint32_t *record = ui_resource_boundary(lua, 2);
    for (size_t field = 2; field < 4; ++field) {
        FloatBits value = {.bits = record[field]};
        damage_lua.push_number(lua, value.number);
    }
    return 2;
}

static void ui_resource_state(void *lua, const uint32_t setup[11]) {
    damage_lua.create_table(lua, 0, 16);
    for (size_t field = 0; field < 9; ++field) {
        damage_number(lua, 1, ui_resource_numeric[field], setup[field]);
    }
    ui_reference_value_push(lua, setup[9], setup[10], ui_resource_texts, 5);
    damage_lua.set_field(lua, 1, "enabled");
    damage_lua.get_field(lua, -10002, "resource-refresh");
    damage_lua.set_field(lua, 1, "refresh");
    damage_lua.get_field(lua, -10002, "resource-enable");
    damage_lua.set_field(lua, 1, "setEnabled");
    damage_lua.set_field(lua, -10002, "_ab_resource");
}

static void ui_resource_run(void *lua, const uint32_t command[11]) {
    const char *functions[] = {"resource-set",    "resource-refresh", "resource-size",
                               "resource-enable", "resource-button",  "resource-angle"};
    uint32_t operation = command[0];
    damage_lua.get_field(lua, -10002, functions[operation]);
    damage_lua.get_field(lua, -10002, "_ab_resource");
    int arguments = 1;
    if (operation == 0 || operation == 4) {
        ui_reference_value_push(lua, command[1], command[2], ui_resource_texts, 5);
        ++arguments;
        if (operation == 4) {
            ui_reference_value_push(lua, command[3], command[4], ui_resource_texts, 5);
            ++arguments;
        }
    } else if (operation == 2) {
        for (size_t field = 0; field < 2; ++field) {
            if (command[7] & (1u << field)) {
                FloatBits value = {.bits = command[8 + field]};
                damage_lua.push_number(lua, value.number);
            } else {
                damage_lua.push_nil(lua);
            }
        }
        arguments += 2;
    } else if (operation == 3) {
        ui_reference_value_push(lua, command[5], command[6], ui_resource_texts, 5);
        ++arguments;
    } else if (operation == 5) {
        FloatBits value = {.bits = command[10]};
        damage_lua.push_number(lua, value.number);
        ++arguments;
    }
    ui_resource_call_count = 0;
    if (damage_lua.call(lua, arguments, 0, 0)) {
        const char *message = damage_lua.to_string(lua, -1, NULL);
        fail(message ? message : "Original UI image resource operation failed\n");
    }
    uint32_t output[18];
    damage_lua.get_field(lua, -10002, "_ab_resource");
    const char *values[] = {"image", "enabled", "enabledImage", "disabledImage"};
    for (size_t field = 0; field < 4; ++field) {
        ui_resource_value(lua, 1, values[field], output + field * 2);
    }
    for (size_t field = 0; field < 9; ++field) {
        output[8 + field] = ui_resource_number(lua, 1, ui_resource_numeric[field]);
    }
    output[17] = ui_resource_call_count;
    print_words(output, 18);
    for (uint32_t index = 0; index < ui_resource_call_count; ++index) {
        print_words(ui_resource_calls[index], 14);
    }
    damage_lua.set_top(lua, 0);
}

static void ui_resource_random_value(uint32_t *random, uint32_t output[2]) {
    output[0] = (random_word(random) >> 16) % 5;
    output[1] = random_word(random);
    if (!output[0]) {
        output[1] = 0;
    } else if (output[0] == 1) {
        output[1] &= 1;
    } else if (output[0] == 3) {
        output[1] = (output[1] >> 16) % 5;
    } else if (output[0] == 4) {
        output[1] = (output[1] >> 16) % 31 + 1;
    }
}

static void probe_ui_image_resources(void) {
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original UI image resource state\n");
    }
    const char *paths[] = {
        "/data/local/tmp/ab520/ui/image-resources-set-oracle.lua",
        "/data/local/tmp/ab520/ui/image-resources-refresh-oracle.lua",
        "/data/local/tmp/ab520/ui/image-resources-size-oracle.lua",
        "/data/local/tmp/ab520/ui/image-resources-enable-oracle.lua",
        "/data/local/tmp/ab520/ui/image-resources-button-oracle.lua",
        "/data/local/tmp/ab520/ui/image-resources-angle-oracle.lua"};
    const char *names[] = {"resource-set",    "resource-refresh", "resource-size",
                           "resource-enable", "resource-button",  "resource-angle"};
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    for (size_t index = 0; index < 6; ++index) {
        load_selected_function(lua, paths[index], names[index]);
    }
    damage_lua.create_table(lua, 0, 1);
    damage_lua.get_field(lua, -10002, "resource-set");
    damage_lua.set_field(lua, 1, "setImage");
    damage_lua.set_field(lua, -10002, "Image");
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 1, "getLocalizationString", ui_resource_localize);
    damage_lua.set_field(lua, -10002, "gamelua");
    damage_lua.create_table(lua, 0, 2);
    damage_function(lua, 1, "getSpriteBounds", ui_resource_bounds);
    damage_function(lua, 1, "getSpritePivot", ui_resource_pivot);
    damage_lua.set_field(lua, -10002, "res");
    uint32_t random = UINT32_C(0x5201e50a);
    semihost(4, "AB_ANDROID_UI_IMAGE_RESOURCES_V1 32 256\n");
    for (size_t configuration = 0; configuration < 32; ++configuration) {
        uint32_t setup[11];
        for (size_t field = 0; field < 9; ++field) {
            setup[field] = random_float(&random, 115, 22, 1);
        }
        ui_resource_random_value(&random, setup + 9);
        print_words(setup, 11);
        for (size_t index = 0; index < 5; ++index) {
            ui_resource_map[index] = (random_word(&random) >> 16) % 5;
            for (size_t field = 0; field < 4; ++field) {
                ui_resource_geometry[index][field] = random_float(&random, 117, 20, 1);
            }
        }
        print_words(ui_resource_map, 5);
        for (size_t index = 0; index < 5; ++index) {
            print_words(ui_resource_geometry[index], 4);
        }
        ui_resource_state(lua, setup);
        for (size_t step = 0; step < 256; ++step) {
            uint32_t command[11];
            command[0] = (random_word(&random) >> 16) % 6;
            command[1] = (random_word(&random) >> 16) % 4 ? 3u : 0;
            command[2] = command[1] ? (random_word(&random) >> 16) % 5 : 0;
            uint32_t disabled = (random_word(&random) >> 16) % 3;
            command[3] = disabled == 2 ? 3u : disabled;
            command[4] = disabled == 2 ? (random_word(&random) >> 16) % 5 : 0;
            ui_resource_random_value(&random, command + 5);
            command[7] = (random_word(&random) >> 16) % 4;
            for (size_t field = 8; field < 11; ++field) {
                command[field] = random_float(&random, 115, 22, 1);
            }
            if (step % 17 == 0) {
                command[8] = 0;
                command[9] = UINT32_C(0x80000000);
            }
            print_words(command, 11);
            ui_resource_run(lua, command);
        }
    }
    damage_lua.close_state(lua);
    semihost(4, "AB UI image resources reference complete\n");
}

#endif
