#ifndef AB_ORACLE_UI_IMAGE_ANIMATION_H
#define AB_ORACLE_UI_IMAGE_ANIMATION_H

static uint32_t ui_animation_command[14], ui_animation_calls[3][6],
    ui_animation_call_count;
static const char *const ui_animation_states[] = {"NONE", "ENTER", "HOVER", "LEAVE",
                                                  "OTHER"};

static void ui_animation_boundary(void *lua, uint32_t kind, uint32_t a, uint32_t b) {
    if (ui_animation_call_count >= 3) {
        fail("Original ImageButton animation callback overflow\n");
    }
    uint32_t *call = ui_animation_calls[ui_animation_call_count++];
    call[0] = kind;
    damage_lua.get_field(lua, 1, "x");
    FloatBits x = {.number = damage_lua.to_number(lua, -1)};
    damage_lua.get_field(lua, 1, "y");
    FloatBits y = {.number = damage_lua.to_number(lua, -1)};
    call[1] = x.bits;
    call[2] = y.bits;
    call[3] = a;
    call[4] = b;
    damage_lua.get_field(lua, 1, "clickStarted");
    call[5] = (uint32_t)damage_lua.to_boolean(lua, -1);
    damage_lua.get_field(lua, 1, "shouldClearPointerFlags");
    call[5] |= damage_lua.to_boolean(lua, -1) ? 2u : 0u;
}

static int ui_animation_image_update(void *lua) {
    FloatBits seconds = {.number = damage_lua.to_number(lua, 2)};
    FloatBits time = {.number = damage_lua.to_number(lua, 3)};
    ui_animation_boundary(lua, 1, seconds.bits, time.bits);
    return 0;
}

static int ui_animation_position(void *lua) {
    ui_animation_boundary(lua, 2, ui_animation_command[6], ui_animation_command[7]);
    for (size_t axis = 0; axis < 2; ++axis) {
        FloatBits value = {.bits = ui_animation_command[6 + axis]};
        damage_lua.push_number(lua, value.number);
    }
    return 2;
}

static int ui_animation_hit(void *lua) {
    FloatBits x = {.number = damage_lua.to_number(lua, 2)};
    FloatBits y = {.number = damage_lua.to_number(lua, 3)};
    ui_animation_boundary(lua, 3, x.bits, y.bits);
    damage_lua.push_boolean(lua, (int)ui_animation_command[12]);
    return 1;
}

static void ui_animation_state(void *lua, const uint32_t setup[4]) {
    damage_lua.create_table(lua, 0, 10);
    damage_number(lua, 1, "hover_scale", setup[0]);
    damage_number(lua, 1, "hover_count", setup[1]);
    damage_lua.push_string(lua, ui_animation_states[setup[2]]);
    damage_lua.set_field(lua, 1, "hover_state");
    damage_function(lua, 1, "getPositionOnScreen", ui_animation_position);
    damage_function(lua, 1, "hitTest", ui_animation_hit);
    damage_lua.get_field(lua, -10002, "animation-flags");
    damage_lua.set_field(lua, 1, "clearPointerFlags");
    damage_lua.set_field(lua, -10002, "_ab_animation");
}

static void ui_animation_prepare(void *lua) {
    damage_lua.get_field(lua, -10002, "_ab_animation");
    damage_number(lua, 1, "x", ui_animation_command[10]);
    damage_number(lua, 1, "y", ui_animation_command[11]);
    const char *flags[] = {"hover_enabled", "scale_on_touch"};
    for (size_t index = 0; index < 2; ++index) {
        damage_lua.push_boolean(lua, (int)(ui_animation_command[1] & (1u << index)));
        damage_lua.set_field(lua, 1, flags[index]);
    }
    const char *pointer[] = {"clickStarted", "shouldClearPointerFlags"};
    for (size_t index = 0; index < 2; ++index) {
        damage_lua.push_boolean(lua, (int)(ui_animation_command[13] & (1u << index)));
        damage_lua.set_field(lua, 1, pointer[index]);
    }
    switch (ui_animation_command[2]) {
        case 0:
            damage_lua.push_nil(lua);
            break;
        case 1:
        case 2:
            damage_lua.push_boolean(lua, ui_animation_command[2] == 2);
            break;
        case 3:
            damage_lua.push_number(lua, 0);
            break;
        case 4:
            damage_lua.push_string(lua, "");
            break;
        default:
            damage_lua.create_table(lua, 0, 0);
            break;
    }
    damage_lua.set_field(lua, 1, "enabled");
    damage_lua.set_top(lua, 0);
    damage_lua.create_table(lua, 0, 4);
    damage_lua.push_boolean(lua, (int)(ui_animation_command[1] & 4));
    damage_lua.set_field(lua, 1, "g_scale_buttons_on_touch");
    damage_lua.get_field(lua, -10002, "animation-linear");
    damage_lua.set_field(lua, 1, "tweenLinear");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.push_boolean(lua, (int)(ui_animation_command[1] & 8));
    damage_lua.set_field(lua, 2, "LBUTTON");
    damage_lua.set_field(lua, 1, "keyHold");
    damage_lua.create_table(lua, 0, 2);
    damage_number(lua, 2, "x", ui_animation_command[8]);
    damage_number(lua, 2, "y", ui_animation_command[9]);
    damage_lua.set_field(lua, 1, "cursor");
    damage_lua.set_field(lua, -10002, "gamelua");
}

static void ui_animation_run(void *lua) {
    const char *functions[] = {"animation-update", "animation-enter",
                               "animation-leave"};
    ui_animation_call_count = 0;
    damage_lua.get_field(lua, -10002, functions[ui_animation_command[0]]);
    damage_lua.get_field(lua, -10002, "_ab_animation");
    if (!ui_animation_command[0]) {
        for (size_t index = 0; index < 2; ++index) {
            FloatBits value = {.bits = ui_animation_command[4 + index]};
            damage_lua.push_number(lua, value.number);
        }
    }
    if (damage_lua.call(lua, ui_animation_command[0] ? 1 : 3, 0, 0)) {
        const char *message = damage_lua.to_string(lua, -1, NULL);
        fail(message ? message : "Original ImageButton animation failed\n");
    }
    uint32_t output[9];
    damage_lua.get_field(lua, -10002, "_ab_animation");
    const char *numeric[] = {"hover_scale", "hover_count", "x", "y"};
    const size_t slots[] = {0, 1, 5, 6};
    for (size_t index = 0; index < 4; ++index) {
        damage_lua.get_field(lua, 1, numeric[index]);
        FloatBits value = {.number = damage_lua.to_number(lua, -1)};
        output[slots[index]] = value.bits;
        damage_lua.set_top(lua, 1);
    }
    damage_lua.get_field(lua, 1, "hover_state");
    output[2] = 5;
    for (uint32_t index = 0; index < 5; ++index) {
        if (damage_named(lua, 2, ui_animation_states[index])) {
            output[2] = index;
        }
    }
    damage_lua.set_top(lua, 1);
    damage_lua.get_field(lua, 1, "clickStarted");
    output[3] = (uint32_t)damage_lua.to_boolean(lua, 2);
    damage_lua.get_field(lua, 1, "shouldClearPointerFlags");
    output[4] = (uint32_t)damage_lua.to_boolean(lua, 3);
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "animation-factor");
    damage_lua.get_field(lua, -10002, "_ab_animation");
    if (damage_lua.call(lua, 1, 1, 0)) {
        fail("Original ImageButton hover scale failed\n");
    }
    FloatBits factor = {.number = damage_lua.to_number(lua, 1)};
    output[7] = factor.bits;
    output[8] = ui_animation_call_count;
    print_words(output, 9);
    for (uint32_t index = 0; index < ui_animation_call_count; ++index) {
        print_words(ui_animation_calls[index], 6);
    }
    damage_lua.set_top(lua, 0);
}

static void probe_ui_image_animation(void) {
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original ImageButton animation state\n");
    }
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    (void)damage_lua.open_math(lua);
    damage_lua.set_top(lua, 0);
    const char *paths[] = {
        "/data/local/tmp/ab520/ui/image-animation-update-oracle.lua",
        "/data/local/tmp/ab520/ui/image-animation-enter-oracle.lua",
        "/data/local/tmp/ab520/ui/image-animation-leave-oracle.lua",
        "/data/local/tmp/ab520/ui/image-animation-factor-oracle.lua",
        "/data/local/tmp/ab520/ui/image-animation-flags-oracle.lua",
        "/data/local/tmp/ab520/ui/image-animation-linear-oracle.lua"};
    const char *names[] = {"animation-update", "animation-enter", "animation-leave",
                           "animation-factor", "animation-flags", "animation-linear"};
    for (size_t index = 0; index < 6; ++index) {
        load_selected_function(lua, paths[index], names[index]);
    }
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 1, "update", ui_animation_image_update);
    damage_lua.set_field(lua, -10002, "Image");
    uint32_t random = UINT32_C(0x520a71a0);
    semihost(4, "AB_ANDROID_UI_IMAGE_ANIMATION_V1 64 128\n");
    for (size_t configuration = 0; configuration < 64; ++configuration) {
        FloatBits count = {.number = (float)((int)(configuration % 7) - 3)};
        if (configuration % 8 == 7) {
            count.number = configuration & 8 ? -16777216.0f : 16777216.0f;
        }
        uint32_t setup[4] = {random_float(&random, 121, 10, 1), count.bits,
                             (uint32_t)(configuration % 5), (uint32_t)configuration};
        if (configuration < 2) {
            setup[0] = configuration ? UINT32_C(0x80000000) : 0;
        }
        print_words(setup, 4);
        ui_animation_state(lua, setup);
        for (size_t step = 0; step < 128; ++step) {
            ui_animation_command[0] = step % 5 ? 0u : step % 10 ? 1u : 2u;
            ui_animation_command[1] = (random_word(&random) >> 16) & 15;
            ui_animation_command[2] = (random_word(&random) >> 16) % 6;
            ui_animation_command[3] = 0;
            ui_animation_command[4] = random_float(&random, 115, 12, 1);
            for (size_t field = 5; field < 12; ++field) {
                ui_animation_command[field] = random_float(&random, 116, 20, 1);
            }
            ui_animation_command[12] = (random_word(&random) >> 16) & 1;
            ui_animation_command[13] = (random_word(&random) >> 16) & 3;
            if (step < 8) {
                const uint32_t operations[] = {1, 0, 2, 0, 0, 0, 1, 2};
                ui_animation_command[0] = operations[step];
                ui_animation_command[1] = step >= 6 ? 14u : step == 1 ? 1u : 15u;
                ui_animation_command[2] = 2;
                FloatBits seconds = {.number = step & 1 ? 0.2f : 1.0f / 30};
                ui_animation_command[4] = seconds.bits;
                ui_animation_command[12] = step != 5;
                ui_animation_command[13] = step == 4 ? 3u : 1u;
            }
            if (step % 19 == 18) {
                ui_animation_command[4] = step & 1 ? UINT32_C(0x80000000) : 0;
            }
            print_words(ui_animation_command, 14);
            ui_animation_prepare(lua);
            ui_animation_run(lua);
        }
    }
    damage_lua.close_state(lua);
    semihost(4, "AB UI image animation reference complete\n");
}

#endif
