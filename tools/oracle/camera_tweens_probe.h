#ifndef AB_ORACLE_CAMERA_TWEENS_H
#define AB_ORACLE_CAMERA_TWEENS_H

static const char *const tween_names[] = {"mover-cubic", "mover-cubic-out", "mover-end",
                                          "mover-cosine"};

static uint32_t tween_kind(void *lua, const char *field) {
    damage_lua.get_field(lua, -10002, "_ab_mover_state");
    damage_lua.get_field(lua, 1, field);
    const void *selected = damage_lua.to_pointer(lua, 2);
    uint32_t result = 4;
    for (uint32_t kind = 0; kind < 4; ++kind) {
        damage_lua.get_field(lua, -10002, tween_names[kind]);
        if (selected == damage_lua.to_pointer(lua, 3)) {
            result = kind;
        }
        damage_lua.set_top(lua, 2);
    }
    damage_lua.set_top(lua, 0);
    if (result == 4) {
        fail("Unexpected original camera tween selection\n");
    }
    return result;
}

static void tween_command(void *lua, const uint32_t input[6]) {
    uint32_t output[20] = {0};
    if (input[0] == 6) {
        damage_lua.get_field(lua, -10002, "mover-set-tweens");
        damage_lua.get_field(lua, -10002, "_ab_mover_state");
        for (size_t field = 1; field < 3; ++field) {
            if (input[field]) {
                damage_lua.get_field(lua, -10002, tween_names[input[field] - 1]);
            } else {
                damage_lua.push_nil(lua);
            }
        }
        if (damage_lua.call(lua, 3, 0, 0)) {
            fail("Original camera tween selection failed\n");
        }
        damage_lua.set_top(lua, 0);
    } else if (input[0] == 5) {
        damage_lua.get_field(lua, -10002, tween_names[input[5]]);
        for (size_t field = 1; field <= 4; ++field) {
            FloatBits value = {.bits = input[field]};
            damage_lua.push_number(lua, value.number);
        }
        if (damage_lua.call(lua, 4, 1, 0) || damage_lua.type(lua, 1) != 3) {
            fail("Original selected camera tween evaluation failed\n");
        }
        FloatBits value = {.number = damage_lua.to_number(lua, 1)};
        output[18] = 1;
        output[19] = value.bits;
        damage_lua.set_top(lua, 0);
    } else {
        const char *names[] = {"mover-reset", "mover-set-position", "mover-set-zoom",
                               "mover-clear", "mover-update"};
        mover_call(lua, names[input[0]], input);
    }
    mover_snapshot(lua, output);
    output[16] = tween_kind(lua, "movementTweenFunction");
    output[17] = tween_kind(lua, "zoomTweenFunction");
    print_words(output, 20);
}

static void probe_camera_tweens(void) {
    initialize_damage_lua();
    mover_set_metatable =
        (int (*)(void *, int))find_function("_Z16lua_setmetatableP9lua_Statei");
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original camera tween reference state\n");
    }
    (void)damage_lua.open_base(lua);
    (void)damage_lua.open_math(lua);
    damage_lua.set_top(lua, 0);
    const char *names[] = {"mover-reset",     "mover-update",       "mover-position",
                           "mover-zoom",      "mover-set-position", "mover-set-zoom",
                           "mover-clear",     "mover-cubic",        "mover-set-tweens",
                           "mover-cubic-out", "mover-end",          "mover-cosine"};
    const char *paths[] = {"/data/local/tmp/ab520/camera/mover-reset-oracle.lua",
                           "/data/local/tmp/ab520/camera/mover-update-oracle.lua",
                           "/data/local/tmp/ab520/camera/mover-position-oracle.lua",
                           "/data/local/tmp/ab520/camera/mover-zoom-oracle.lua",
                           "/data/local/tmp/ab520/camera/mover-set-position-oracle.lua",
                           "/data/local/tmp/ab520/camera/mover-set-zoom-oracle.lua",
                           "/data/local/tmp/ab520/camera/mover-clear-oracle.lua",
                           "/data/local/tmp/ab520/camera/mover-cubic-oracle.lua",
                           "/data/local/tmp/ab520/camera/mover-set-tweens-oracle.lua",
                           "/data/local/tmp/ab520/camera/mover-cubic-out-oracle.lua",
                           "/data/local/tmp/ab520/camera/mover-end-oracle.lua",
                           "/data/local/tmp/ab520/camera/mover-cosine-oracle.lua"};
    for (size_t index = 0; index < 12; ++index) {
        load_selected_function(lua, paths[index], names[index]);
    }
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 1, "__mul", mover_vector_multiply);
    damage_lua.set_field(lua, -10002, "_ab_mover_vector_meta");
    mover_bind_vector(lua, "mover-reset");
    mover_bind_vector(lua, "mover-set-position");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.get_field(lua, -10002, "mover-cubic");
    damage_lua.set_field(lua, 1, "tweenEaseCubicInOut");
    damage_lua.set_field(lua, -10002, "gamelua");
    static const float durations[] = {0, -1, -0.0f, 0.125f, 0.5f, 1, 2, 8};
    static const float seconds[] = {0, 0x1.111112p-5f, 0.125f, 0.5f, 2, -0.125f, 1};
    uint32_t random = UINT32_C(0x5207aee1);
    semihost(4, "AB_ANDROID_CAMERA_TWEENS_V2 128 64\n");
    for (size_t item = 0; item < 128; ++item) {
        damage_lua.create_table(lua, 0, 24);
        const char *fields[] = {"updatePosition", "updateZoom", "movementTweenFunction",
                                "zoomTweenFunction"};
        const char *functions[] = {"mover-position", "mover-zoom", "mover-cubic",
                                   "mover-cubic"};
        for (size_t field = 0; field < 4; ++field) {
            damage_lua.get_field(lua, -10002, functions[field]);
            damage_lua.set_field(lua, 1, fields[field]);
        }
        damage_lua.set_field(lua, -10002, "_ab_mover_state");
        const uint32_t reset[6] = {0};
        mover_call(lua, "mover-reset", reset);
        for (size_t step = 0; step < 64; ++step) {
            uint32_t input[6] = {random_word(&random) % 8};
            if (input[0] > 6) {
                input[0] = 4;
            }
            for (size_t field = 1; field <= 3; ++field) {
                input[field] = random_float(&random, 108, 24, 1);
            }
            FloatBits duration = {.number = durations[random_word(&random) % 8]};
            FloatBits delta = {.number = seconds[random_word(&random) % 7]};
            input[4] = duration.bits;
            input[5] = delta.bits;
            if (step < 10) {
                static const uint32_t operations[] = {6, 1, 2, 4, 6, 4, 0, 5, 1, 2};
                input[0] = operations[step];
                duration.number = 1;
                delta.number = 0.25f;
                input[4] = duration.bits;
                input[5] = delta.bits;
            }
            if (input[0] == 6) {
                input[1] = (uint32_t)((item + step) % 5);
                input[2] = (uint32_t)((item / 5 + step) % 5);
            } else if (input[0] == 5) {
                input[5] = (uint32_t)((item + step) % 4);
                if (duration.number == 0) {
                    duration.number = 1;
                    input[4] = duration.bits;
                }
                if (input[5] == 3) {
                    FloatBits time = {.number = (float)((int)(step % 33) - 16) *
                                                0.125f * duration.number};
                    input[1] = time.bits;
                }
                if (input[5] == 2 && item % 2) {
                    input[1] = input[4] = UINT32_C(0x7fc00000);
                }
            }
            print_words(input, 6);
            tween_command(lua, input);
        }
    }
    damage_lua.close_state(lua);
    semihost(4, "AB camera tweens reference complete\n");
}

#endif
