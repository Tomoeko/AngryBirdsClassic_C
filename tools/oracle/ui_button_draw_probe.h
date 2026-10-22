#ifndef AB_ORACLE_UI_BUTTON_DRAW_H
#define AB_ORACLE_UI_BUTTON_DRAW_H

static void (*ui_button_draw_raw_set)(void *, int, int);

static int ui_button_draw_child(void *lua) {
    uint32_t *call = ui_image_draw_call(2, 6);
    for (size_t field = 0; field < 6; ++field) {
        if (damage_lua.type(lua, (int)field + 2) != 3) {
            fail("Missing original button child draw number\n");
        }
        FloatBits value = {.number = damage_lua.to_number(lua, (int)field + 2)};
        call[2 + field] = value.bits;
    }
    damage_lua.get_field(lua, 1, "_ab_index");
    call[8] = (uint32_t)damage_lua.to_number(lua, -1);
    return 0;
}

static void ui_button_draw_prepare(void *lua, const uint32_t extra[3],
                                   const uint32_t flags[8]) {
    ui_image_draw_prepare(lua);
    damage_lua.get_field(lua, -10002, "gamelua");
    damage_lua.get_field(lua, -10002, "button-draw-linear");
    damage_lua.set_field(lua, 1, "tweenLinear");
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "_ab_image_draw");
    damage_lua.push_boolean(lua, (int)extra[0]);
    damage_lua.set_field(lua, 1, "hover_enabled");
    damage_number(lua, 1, "hover_scale", extra[1]);
    damage_lua.get_field(lua, -10002, "button-draw-factor");
    damage_lua.set_field(lua, 1, "hoverScale");
    damage_lua.get_field(lua, -10002, "button-draw-tagged");
    damage_lua.set_field(lua, 1, "drawWithTag");
    damage_lua.create_table(lua, (int)extra[2], 0);
    for (uint32_t index = 0; index < extra[2]; ++index) {
        damage_lua.create_table(lua, 0, 4);
        damage_lua.push_number(lua, (float)index);
        damage_lua.set_field(lua, 3, "_ab_index");
        damage_lua.push_boolean(lua, (int)(flags[index] & 1));
        damage_lua.set_field(lua, 3, "visible");
        damage_lua.push_boolean(lua, (int)(flags[index] & 2));
        damage_lua.set_field(lua, 3, "active");
        damage_function(lua, 3, "draw", ui_button_draw_child);
        ui_button_draw_raw_set(lua, 2, (int)index + 1);
    }
    damage_lua.set_field(lua, 1, "children");
    damage_lua.set_top(lua, 0);
}

static void ui_button_draw_run(void *lua) {
    ui_image_draw_count = 0;
    damage_lua.get_field(lua, -10002, "button-draw-draw");
    damage_lua.get_field(lua, -10002, "_ab_image_draw");
    for (size_t field = 0; field < 6; ++field) {
        if (ui_image_draw_setup[2] & (1u << field)) {
            FloatBits value = {.bits = ui_image_draw_setup[7 + field]};
            damage_lua.push_number(lua, value.number);
        } else {
            damage_lua.push_boolean(lua, 0);
        }
    }
    if (damage_lua.call(lua, 7, 0, 0)) {
        const char *message = damage_lua.to_string(lua, -1, NULL);
        fail(message ? message : "Original ImageButton draw graph failed\n");
    }
    print_words(&ui_image_draw_count, 1);
    for (uint32_t index = 0; index < ui_image_draw_count; ++index) {
        print_words(ui_image_draw_calls[index], 10);
    }
    damage_lua.set_top(lua, 0);
}

static void probe_ui_button_draw(void) {
    initialize_damage_lua();
    ui_button_draw_raw_set =
        (void (*)(void *, int, int))find_function("_Z11lua_rawsetiP9lua_Stateii");
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original ImageButton draw graph\n");
    }
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    (void)damage_lua.open_math(lua);
    damage_lua.set_top(lua, 0);
    const char *paths[] = {"/data/local/tmp/ab520/ui/button-draw-self-oracle.lua",
                           "/data/local/tmp/ab520/ui/button-draw-draw-oracle.lua",
                           "/data/local/tmp/ab520/ui/button-draw-factor-oracle.lua",
                           "/data/local/tmp/ab520/ui/button-draw-linear-oracle.lua",
                           "/data/local/tmp/ab520/ui/button-draw-image-oracle.lua",
                           "/data/local/tmp/ab520/ui/button-draw-frame-oracle.lua",
                           "/data/local/tmp/ab520/ui/button-draw-tagged-oracle.lua"};
    const char *names[] = {"button-draw-self",   "button-draw-draw",
                           "button-draw-factor", "button-draw-linear",
                           "button-draw-image",  "button-draw-frame",
                           "button-draw-tagged"};
    for (size_t index = 0; index < 7; ++index) {
        load_selected_function(lua, paths[index], names[index]);
    }
    const char *tables[] = {"ImageButton", "Image", "Frame"};
    const char *methods[] = {"drawSelf", "drawSelf", "draw"};
    const char *functions[] = {"button-draw-self", "button-draw-image",
                               "button-draw-frame"};
    for (size_t index = 0; index < 3; ++index) {
        damage_lua.create_table(lua, 0, 1);
        damage_lua.get_field(lua, -10002, functions[index]);
        damage_lua.set_field(lua, 1, methods[index]);
        damage_lua.set_field(lua, -10002, tables[index]);
    }
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 1, "drawSprite", ui_image_draw_sprite);
    damage_lua.set_field(lua, -10002, "res");
    uint32_t random = UINT32_C(0x520bda90);
    semihost(4, "AB_ANDROID_UI_BUTTON_DRAW_V1 4096\n");
    for (size_t item = 0; item < 4096; ++item) {
        ui_image_draw_setup[0] = (random_word(&random) >> 16) & 3;
        ui_image_draw_setup[1] = (uint32_t)((item / 7) % 4);
        ui_image_draw_setup[2] = (uint32_t)(item % 64);
        ui_image_draw_setup[3] = (uint32_t)((item / 64) % 16);
        ui_image_draw_setup[4] = (random_word(&random) >> 16) & 1;
        FloatBits width = {.number = 480}, height = {.number = 320};
        ui_image_draw_setup[5] = width.bits;
        ui_image_draw_setup[6] = height.bits;
        for (size_t field = 7; field < 25; ++field) {
            ui_image_draw_setup[field] = random_float(&random, 117, 20, 1);
            if ((item + field) % 11 == 0) {
                ui_image_draw_setup[field] &= UINT32_C(0x80000000);
            }
        }
        if ((item / 64) % 2 == 0) {
            ui_image_draw_setup[17] =
                ui_image_draw_setup[2] & 16
                    ? ui_image_draw_setup[11] ^ UINT32_C(0x80000000)
                    : 0;
        }
        if (item % 17 == 0) {
            const uint32_t unused[] = {0x7fc12345, 0xffc54321, 0x7f800001,
                                       0xff800001, 0x7f800000, 0xff800000};
            ui_image_draw_setup[12] = unused[(item / 17) % 6];
        }
        uint32_t extra[] = {(uint32_t)((item / 3) % 2),
                            random_float(&random, 121, 11, 1), (uint32_t)(item % 9)};
        if (item % 13 == 0) {
            FloatBits scale = {.number = -10};
            extra[1] = scale.bits;
        }
        uint32_t flags[8];
        for (uint32_t index = 0; index < extra[2]; ++index) {
            flags[index] = (random_word(&random) >> 16) & 3;
        }
        print_words(ui_image_draw_setup, 25);
        print_words(extra, 3);
        if (extra[2]) {
            print_words(flags, extra[2]);
        }
        ui_button_draw_prepare(lua, extra, flags);
        ui_button_draw_run(lua);
    }
    damage_lua.close_state(lua);
    semihost(4, "AB UI button draw reference complete\n");
}

#endif
