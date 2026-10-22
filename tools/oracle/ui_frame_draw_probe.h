#ifndef AB_ORACLE_UI_FRAME_DRAW_H
#define AB_ORACLE_UI_FRAME_DRAW_H

static void (*ui_draw_raw_set)(void *, int, int);
static uint32_t ui_draw_setup[19], ui_draw_calls[8][8], ui_draw_call_count;
static const char *const ui_draw_texts[] = {"", "group", "alternate"};
static const char *const ui_draw_methods[] = {"draw", "paint", ""};
static const char *const ui_draw_objects[] = {"_ab_draw_one", "_ab_draw_two",
                                              "_ab_draw_three"};

static void ui_draw_tag(void *lua, uint32_t kind, uint32_t payload) {
    if (!kind) {
        damage_lua.push_nil(lua);
    } else if (kind == 1) {
        damage_lua.push_boolean(lua, (int)payload);
    } else if (kind == 2) {
        FloatBits value = {.bits = payload};
        damage_lua.push_number(lua, value.number);
    } else if (kind == 3) {
        damage_lua.push_string(lua, ui_draw_texts[payload]);
    } else {
        damage_lua.get_field(lua, -10002, ui_draw_objects[payload]);
    }
}

static int ui_draw_callback(void *lua, uint32_t method) {
    damage_lua.get_field(lua, 1, "_ab_index");
    uint32_t index = (uint32_t)damage_lua.to_number(lua, -1);
    if (index >= ui_draw_setup[1] || ui_draw_call_count >= 8) {
        fail("Unexpected original Frame draw callback\n");
    }
    uint32_t *call = ui_draw_calls[ui_draw_call_count++];
    call[0] = index;
    call[1] = method;
    for (size_t field = 0; field < 6; ++field) {
        if (damage_lua.type(lua, (int)field + 2) != 3) {
            fail("Missing original Frame draw numeric argument\n");
        }
        FloatBits value = {.number = damage_lua.to_number(lua, (int)field + 2)};
        call[2 + field] = value.bits;
    }
    return 0;
}

static int ui_draw_default(void *lua) {
    return ui_draw_callback(lua, 0);
}

static int ui_draw_paint(void *lua) {
    return ui_draw_callback(lua, 1);
}

static int ui_draw_empty(void *lua) {
    return ui_draw_callback(lua, 2);
}

static void ui_draw_prepare(void *lua, uint32_t children[8][3]) {
    damage_lua.create_table(lua, 0, 8);
    const char *fields[] = {"x", "y", "scaleX", "scaleY", "angle"};
    for (size_t field = 0; field < 5; ++field) {
        damage_number(lua, 1, fields[field], ui_draw_setup[14 + field]);
    }
    damage_lua.push_boolean(lua, (int)(ui_draw_setup[3] & 1));
    damage_lua.set_field(lua, 1, "visible");
    damage_lua.push_boolean(lua, (int)(ui_draw_setup[3] & 2));
    damage_lua.set_field(lua, 1, "active");
    damage_lua.get_field(lua, -10002, "frame-tagged");
    damage_lua.set_field(lua, 1, "drawWithTag");
    damage_lua.create_table(lua, (int)ui_draw_setup[1], 0);
    for (uint32_t index = 0; index < ui_draw_setup[1]; ++index) {
        damage_lua.create_table(lua, 0, 7);
        damage_lua.push_number(lua, (float)index);
        damage_lua.set_field(lua, 3, "_ab_index");
        damage_lua.push_boolean(lua, (int)(children[index][0] & 1));
        damage_lua.set_field(lua, 3, "visible");
        damage_lua.push_boolean(lua, (int)(children[index][0] & 2));
        damage_lua.set_field(lua, 3, "active");
        ui_draw_tag(lua, children[index][1], children[index][2]);
        damage_lua.set_field(lua, 3, "tag");
        damage_function(lua, 3, "draw", ui_draw_default);
        damage_function(lua, 3, "paint", ui_draw_paint);
        damage_function(lua, 3, "", ui_draw_empty);
        ui_draw_raw_set(lua, 2, (int)index + 1);
    }
    damage_lua.set_field(lua, 1, "children");
    damage_lua.set_field(lua, -10002, "_ab_draw_frame");
}

static void ui_draw_run(void *lua) {
    ui_draw_call_count = 0;
    damage_lua.get_field(lua, -10002, ui_draw_setup[0] ? "frame-draw" : "frame-tagged");
    damage_lua.get_field(lua, -10002, "_ab_draw_frame");
    if (!ui_draw_setup[0]) {
        ui_draw_tag(lua, ui_draw_setup[6], ui_draw_setup[7]);
    }
    for (size_t field = 0; field < 6; ++field) {
        if (ui_draw_setup[2] & (1u << field)) {
            FloatBits value = {.bits = ui_draw_setup[8 + field]};
            damage_lua.push_number(lua, value.number);
        } else if (field & 1) {
            damage_lua.push_boolean(lua, 0);
        } else {
            damage_lua.push_nil(lua);
        }
    }
    if (ui_draw_setup[4]) {
        damage_lua.push_string(lua, ui_draw_methods[ui_draw_setup[5]]);
    } else {
        damage_lua.push_boolean(lua, 0);
    }
    if (damage_lua.call(lua, ui_draw_setup[0] ? 8 : 9, 0, 0)) {
        const char *message = damage_lua.to_string(lua, -1, NULL);
        fail(message ? message : "Original Frame draw failed\n");
    }
    print_words(&ui_draw_call_count, 1);
    for (uint32_t index = 0; index < ui_draw_call_count; ++index) {
        print_words(ui_draw_calls[index], 8);
    }
    damage_lua.set_top(lua, 0);
}

static void ui_draw_random_tag(uint32_t *random, uint32_t tag[2]) {
    tag[0] = (random_word(random) >> 16) % 5;
    uint32_t payload = random_word(random) >> 16;
    const uint32_t numbers[] = {0,          0x80000000, 0x3f800000,
                                0xbf800000, 0x7fc00001, 0x7f800000};
    tag[1] = !tag[0]       ? 0
             : tag[0] == 1 ? payload & 1
             : tag[0] == 2 ? numbers[payload % 6]
                           : payload % 3;
}

static void probe_ui_frame_draw(void) {
    initialize_damage_lua();
    ui_draw_raw_set =
        (void (*)(void *, int, int))find_function("_Z11lua_rawsetiP9lua_Stateii");
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original Frame drawing state\n");
    }
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    load_selected_function(lua, "/data/local/tmp/ab520/ui/frame-draw-draw-oracle.lua",
                           "frame-draw");
    load_selected_function(lua, "/data/local/tmp/ab520/ui/frame-draw-tagged-oracle.lua",
                           "frame-tagged");
    for (size_t object = 0; object < 3; ++object) {
        damage_lua.create_table(lua, 0, 0);
        damage_lua.set_field(lua, -10002, ui_draw_objects[object]);
    }
    uint32_t random = UINT32_C(0x520fd2a0);
    semihost(4, "AB_ANDROID_UI_FRAME_DRAW_V1 4096\n");
    for (size_t item = 0; item < 4096; ++item) {
        ui_draw_setup[0] = (uint32_t)((item / 64) % 2);
        ui_draw_setup[1] = (uint32_t)(item % 9);
        ui_draw_setup[2] = (uint32_t)(item % 64);
        ui_draw_setup[3] = (uint32_t)(item % 4);
        ui_draw_setup[4] = (item / 7) % 4 != 0;
        ui_draw_setup[5] = (uint32_t)(item % 3);
        ui_draw_random_tag(&random, ui_draw_setup + 6);
        for (size_t field = 8; field < 19; ++field) {
            ui_draw_setup[field] = random_float(&random, 114, 22, 1);
            if ((item + field) % 13 == 0) {
                ui_draw_setup[field] &= UINT32_C(0x80000000);
            }
        }
        uint32_t children[8][3];
        for (uint32_t index = 0; index < ui_draw_setup[1]; ++index) {
            children[index][0] = (random_word(&random) >> 16) & 3;
            ui_draw_random_tag(&random, children[index] + 1);
            if ((item + index) % 3 == 0) {
                children[index][1] = ui_draw_setup[6];
                children[index][2] = ui_draw_setup[7];
            }
        }
        print_words(ui_draw_setup, 19);
        for (uint32_t index = 0; index < ui_draw_setup[1]; ++index) {
            print_words(children[index], 3);
        }
        ui_draw_prepare(lua, children);
        ui_draw_run(lua);
    }
    damage_lua.close_state(lua);
    semihost(4, "AB UI frame draw reference complete\n");
}

#endif
