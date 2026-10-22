#ifndef AB_ORACLE_UI_POINTER_H
#define AB_ORACLE_UI_POINTER_H

static void (*ui_pointer_raw_set)(void *, int, int);
static uint32_t ui_pointer_input[7], ui_pointer_children[8][7];
static uint32_t ui_pointer_calls[8][4], ui_pointer_call_count;
static const char *const ui_pointer_events[] = {"LPRESS", "LHOLD", "LRELEASE",
                                                "WHEEL",  "HOVER", "CUSTOM"};
static const char *const ui_pointer_texts[] = {"", "CLICK", "HOVER", "reply"};

static void ui_pointer_push(void *lua, uint32_t kind, uint32_t value) {
    switch (kind) {
        case 0:
            damage_lua.push_nil(lua);
            break;
        case 1:
            damage_lua.push_boolean(lua, (int)value);
            break;
        case 2: {
            FloatBits number = {.bits = value};
            damage_lua.push_number(lua, number.number);
            break;
        }
        case 3:
            damage_lua.push_string(lua, ui_pointer_texts[value]);
            break;
        case 4:
            damage_lua.create_table(lua, 0, 1);
            damage_lua.push_number(lua, (float)value);
            damage_lua.set_field(lua, -2, "_ab_reply_object");
            break;
        default:
            fail("Unsupported original pointer reply fixture\n");
    }
}

static int ui_pointer_child(void *lua) {
    damage_lua.get_field(lua, 1, "_ab_index");
    uint32_t index = (uint32_t)damage_lua.to_number(lua, 5);
    if (index >= ui_pointer_input[0] || ui_pointer_call_count >= 8 ||
        !damage_named(lua, 2, ui_pointer_events[ui_pointer_input[1]])) {
        fail("Unexpected original UI child callback\n");
    }
    uint32_t *call = ui_pointer_calls[ui_pointer_call_count++];
    call[0] = index;
    call[1] = ui_pointer_input[1];
    for (size_t axis = 0; axis < 2; ++axis) {
        FloatBits point = {.number = damage_lua.to_number(lua, (int)axis + 3)};
        call[2 + axis] = point.bits;
    }
    damage_lua.set_top(lua, 4);
    const uint32_t *values = ui_pointer_children[index];
    for (size_t field = 0; field < 3; ++field) {
        ui_pointer_push(lua, values[1 + field * 2], values[2 + field * 2]);
    }
    return 3;
}

static void ui_pointer_prepare(void *lua) {
    ui_pointer_call_count = 0;
    damage_lua.create_table(lua, 0, 5);
    damage_number(lua, 1, "x", ui_pointer_input[2]);
    damage_number(lua, 1, "y", ui_pointer_input[3]);
    damage_lua.push_boolean(lua, (int)(ui_pointer_input[6] & 1));
    damage_lua.set_field(lua, 1, "visible");
    damage_lua.push_boolean(lua, (int)(ui_pointer_input[6] & 2));
    damage_lua.set_field(lua, 1, "active");
    damage_lua.create_table(lua, (int)ui_pointer_input[0], 0);
    for (uint32_t index = 0; index < ui_pointer_input[0]; ++index) {
        damage_lua.create_table(lua, 0, 4);
        damage_lua.push_number(lua, (float)index);
        damage_lua.set_field(lua, 3, "_ab_index");
        damage_lua.push_boolean(lua, (int)(ui_pointer_children[index][0] & 1));
        damage_lua.set_field(lua, 3, "visible");
        damage_lua.push_boolean(lua, (int)(ui_pointer_children[index][0] & 2));
        damage_lua.set_field(lua, 3, "active");
        damage_function(lua, 3, "onPointerEvent", ui_pointer_child);
        ui_pointer_raw_set(lua, 2, (int)index + 1);
    }
    damage_lua.set_field(lua, 1, "children");
    damage_lua.set_field(lua, -10002, "_ab_ui_pointer_frame");
}

static void ui_pointer_reply_value(void *lua, int index, uint32_t output[2]) {
    int type = damage_lua.type(lua, index);
    if (type == 0) {
        output[0] = output[1] = 0;
    } else if (type == 1) {
        output[0] = 1;
        output[1] = (uint32_t)damage_lua.to_boolean(lua, index);
    } else if (type == 3) {
        output[0] = 2;
        FloatBits value = {.number = damage_lua.to_number(lua, index)};
        output[1] = value.bits;
    } else if (type == 4) {
        output[0] = 3;
        output[1] = 4;
        for (uint32_t text = 0; text < 4; ++text) {
            if (damage_named(lua, index, ui_pointer_texts[text])) {
                output[1] = text;
            }
        }
        if (output[1] == 4) {
            fail("Unexpected original UI reply string\n");
        }
    } else if (type == 5) {
        output[0] = 4;
        damage_lua.get_field(lua, index, "_ab_reply_object");
        output[1] = (uint32_t)damage_lua.to_number(lua, -1);
        damage_lua.set_top(lua, 3);
    } else {
        fail("Unsupported original UI reply type\n");
    }
}

static void ui_pointer_run(void *lua) {
    damage_lua.get_field(lua, -10002, "frame-pointer");
    damage_lua.get_field(lua, -10002, "_ab_ui_pointer_frame");
    damage_lua.push_string(lua, ui_pointer_events[ui_pointer_input[1]]);
    for (size_t axis = 0; axis < 2; ++axis) {
        FloatBits point = {.bits = ui_pointer_input[4 + axis]};
        damage_lua.push_number(lua, point.number);
    }
    if (damage_lua.call(lua, 4, 3, 0)) {
        const char *message = damage_lua.to_string(lua, -1, NULL);
        fail(message ? message : "Original frame pointer call failed\n");
    }
    uint32_t output[8] = {0};
    for (size_t field = 0; field < 3; ++field) {
        ui_pointer_reply_value(lua, (int)field + 1, output + field * 2);
    }
    output[6] = (uint32_t)damage_lua.to_boolean(lua, 1);
    output[7] = ui_pointer_call_count;
    print_words(output, 8);
    for (uint32_t index = 0; index < ui_pointer_call_count; ++index) {
        print_words(ui_pointer_calls[index], 4);
    }
    damage_lua.set_top(lua, 0);
}

static void probe_ui_pointer(void) {
    initialize_damage_lua();
    ui_pointer_raw_set =
        (void (*)(void *, int, int))find_function("_Z11lua_rawsetiP9lua_Stateii");
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original UI pointer state\n");
    }
    load_selected_function(lua, "/data/local/tmp/ab520/ui/frame-pointer-oracle.lua",
                           "frame-pointer");
    uint32_t random = UINT32_C(0x520fcb11);
    semihost(4, "AB_ANDROID_UI_POINTER_V1 4096\n");
    for (size_t item = 0; item < 4096; ++item) {
        ui_pointer_input[0] = (uint32_t)(item % 9);
        ui_pointer_input[1] = (uint32_t)(item % 6);
        for (size_t field = 2; field < 6; ++field) {
            ui_pointer_input[field] = random_float(&random, 118, 20, 1);
        }
        if (item % 17 == 0) {
            ui_pointer_input[2] = 0;
            ui_pointer_input[4] = UINT32_C(0x80000000);
        }
        ui_pointer_input[6] = (uint32_t)(item % 4);
        print_words(ui_pointer_input, 7);
        for (uint32_t index = 0; index < ui_pointer_input[0]; ++index) {
            uint32_t *child = ui_pointer_children[index];
            child[0] = (random_word(&random) >> 16) & 3;
            for (size_t field = 0; field < 3; ++field) {
                uint32_t kind = (random_word(&random) >> 16) % 5;
                uint32_t value = random_word(&random);
                if (kind == 0) {
                    value = 0;
                } else if (kind == 1) {
                    value &= 1;
                } else if (kind == 3) {
                    value &= 3;
                } else if (kind == 4) {
                    value = (value >> 16) % 31 + 1;
                }
                child[1 + field * 2] = kind;
                child[2 + field * 2] = value;
            }
            if (item % 7 == 0) {
                child[1] = child[2] = 0;
            } else if (item % 8 == 0) {
                child[0] = 3;
                child[1] = 1;
                child[2] = 0;
            }
            print_words(child, 7);
        }
        ui_pointer_prepare(lua);
        ui_pointer_run(lua);
    }
    damage_lua.close_state(lua);
    semihost(4, "AB UI pointer reference complete\n");
}

#endif
