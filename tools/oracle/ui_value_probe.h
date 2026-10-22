#ifndef AB_ORACLE_UI_VALUE_H
#define AB_ORACLE_UI_VALUE_H

static void ui_reference_value_push(void *lua, uint32_t kind, uint32_t payload,
                                    const char *const *texts, size_t text_count) {
    switch (kind) {
        case 0:
            damage_lua.push_nil(lua);
            break;
        case 1:
            damage_lua.push_boolean(lua, (int)payload);
            break;
        case 2: {
            FloatBits value = {.bits = payload};
            damage_lua.push_number(lua, value.number);
            break;
        }
        case 3:
            if (payload >= text_count) {
                fail("Original UI fixture text index exceeds bounds\n");
            }
            damage_lua.push_string(lua, texts[payload]);
            break;
        case 4:
            damage_lua.create_table(lua, 0, 1);
            damage_lua.push_number(lua, (float)payload);
            damage_lua.set_field(lua, -2, "_ab_ui_identity");
            break;
        default:
            fail("Unsupported original UI value fixture\n");
    }
}

static void ui_reference_value_read(void *lua, int index, uint32_t output[2],
                                    const char *const *texts, size_t text_count) {
    int top = damage_lua.get_top(lua);
    int type = damage_lua.type(lua, index);
    output[0] = output[1] = 0;
    if (type == 1) {
        output[0] = 1;
        output[1] = (uint32_t)damage_lua.to_boolean(lua, index);
    } else if (type == 3) {
        output[0] = 2;
        FloatBits value = {.number = damage_lua.to_number(lua, index)};
        output[1] = value.bits;
    } else if (type == 4) {
        output[0] = 3;
        output[1] = (uint32_t)text_count;
        for (size_t text = 0; text < text_count; ++text) {
            if (damage_named(lua, index, texts[text])) {
                output[1] = (uint32_t)text;
                break;
            }
        }
        if (output[1] == text_count) {
            fail("Unexpected original UI text value\n");
        }
    } else if (type == 5) {
        output[0] = 4;
        damage_lua.get_field(lua, index, "_ab_ui_identity");
        if (damage_lua.type(lua, -1) != 3) {
            fail("Missing original UI logical object identity\n");
        }
        output[1] = (uint32_t)damage_lua.to_number(lua, -1);
    } else if (type != 0) {
        fail("Unsupported original UI value type\n");
    }
    damage_lua.set_top(lua, top);
}

#endif
