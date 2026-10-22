#ifndef AB_ORACLE_CAMERA_MOVER_H
#define AB_ORACLE_CAMERA_MOVER_H

static int (*mover_set_metatable)(void *, int);

static float mover_number(void *lua, int table, const char *field) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, table, field);
    if (damage_lua.type(lua, -1) != 3) {
        fail("Unexpected original camera numeric state\n");
    }
    float number = damage_lua.to_number(lua, -1);
    damage_lua.set_top(lua, top);
    return number;
}

/* The selected methods' Vector2 construction and identity multiplication are
 * controlled boundaries. Full original vector metatables are not executed. */
static void mover_vector(void *lua, float x, float y) {
    int table = damage_lua.get_top(lua) + 1;
    damage_lua.create_table(lua, 0, 2);
    damage_lua.push_number(lua, x);
    damage_lua.set_field(lua, table, "x");
    damage_lua.push_number(lua, y);
    damage_lua.set_field(lua, table, "y");
    damage_lua.get_field(lua, -10002, "_ab_mover_vector_meta");
    (void)mover_set_metatable(lua, table);
}

static int mover_vector_multiply(void *lua) {
    if (damage_lua.get_top(lua) != 2 || damage_lua.type(lua, 1) != 5 ||
        damage_lua.type(lua, 2) != 3 || damage_lua.to_number(lua, 2) != 1.0f) {
        fail("Unexpected controlled camera vector multiplication\n");
    }
    float x = mover_number(lua, 1, "x");
    float y = mover_number(lua, 1, "y");
    mover_vector(lua, x, y);
    return 1;
}

static int mover_vector_new(void *lua) {
    int count = damage_lua.get_top(lua);
    if ((count != 1 && count != 3) || damage_lua.type(lua, 1) != 5) {
        fail("Unexpected controlled camera vector construction\n");
    }
    float x = count == 3 ? damage_lua.to_number(lua, 2) : 0;
    float y = count == 3 ? damage_lua.to_number(lua, 3) : 0;
    mover_vector(lua, x, y);
    return 1;
}

static void mover_bind_vector(void *lua, const char *function) {
    const char *(*setup_value)(void *, int, int) =
        (const char *(*)(void *, int, int))find_function(
            "_Z14lua_setupvalueP9lua_Stateii");
    damage_lua.get_field(lua, -10002, function);
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 2, "new", mover_vector_new);
    if (!setup_value(lua, 1, 1)) {
        fail("Missing selected camera vector upvalue\n");
    }
    damage_lua.set_top(lua, 0);
}

static void mover_call(void *lua, const char *name, const uint32_t input[6]) {
    damage_lua.get_field(lua, -10002, name);
    damage_lua.get_field(lua, -10002, "_ab_mover_state");
    int arguments = 1;
    if (input[0] == 1) {
        for (size_t field = 1; field <= 2; ++field) {
            FloatBits value = {.bits = input[field]};
            damage_lua.push_number(lua, value.number);
        }
        FloatBits duration = {.bits = input[4]};
        damage_lua.push_number(lua, duration.number);
        arguments = 4;
    } else if (input[0] == 2) {
        FloatBits target = {.bits = input[3]}, duration = {.bits = input[4]};
        damage_lua.push_number(lua, target.number);
        damage_lua.push_number(lua, duration.number);
        arguments = 3;
    } else if (input[0] == 4) {
        FloatBits seconds = {.bits = input[5]};
        damage_lua.push_number(lua, seconds.number);
        arguments = 2;
    }
    if (damage_lua.call(lua, arguments, 0, 0)) {
        fail("Original camera mover command failed\n");
    }
    damage_lua.set_top(lua, 0);
}

static void mover_snapshot(void *lua, uint32_t output[17]) {
    static const char *const vectors[] = {"currentPosition", "targetPosition",
                                          "startPosition"};
    static const char *const scalars[] = {
        "currentZoomScale", "targetZoomScale",   "currentZoomVelocity",
        "startZoomScale",   "startZoom",         "movementTweenDuration",
        "movementTimer",    "zoomTweenDuration", "zoomTimer"};
    damage_lua.get_field(lua, -10002, "_ab_mover_state");
    for (size_t vector = 0; vector < 3; ++vector) {
        damage_lua.get_field(lua, 1, vectors[vector]);
        int type = damage_lua.type(lua, 2);
        if (type != 0 && type != 5) {
            fail("Unexpected original camera vector state\n");
        }
        if (type == 5) {
            if (vector) {
                output[0] |= vector == 1 ? 1u : 4u;
            }
            FloatBits x = {.number = mover_number(lua, 2, "x")};
            FloatBits y = {.number = mover_number(lua, 2, "y")};
            output[1 + vector * 2] = x.bits;
            output[2 + vector * 2] = y.bits;
        }
        damage_lua.set_top(lua, 1);
    }
    for (size_t field = 0; field < 9; ++field) {
        damage_lua.get_field(lua, 1, scalars[field]);
        int type = damage_lua.type(lua, 2);
        if (type != 0 && type != 3) {
            fail("Unexpected original scalar camera field\n");
        }
        if (type == 3) {
            if (field == 1 || field == 3 || field == 4) {
                output[0] |= field == 1 ? 2u : field == 3 ? 8u : 16u;
            }
            FloatBits value = {.number = damage_lua.to_number(lua, 2)};
            output[7 + field] = value.bits;
        }
        damage_lua.set_top(lua, 1);
    }
    damage_lua.set_top(lua, 0);
}

#if !defined(AB_ORACLE_CAMERA_TWEENS_ONLY)
static void probe_camera_mover(void) {
    initialize_damage_lua();
    mover_set_metatable =
        (int (*)(void *, int))find_function("_Z16lua_setmetatableP9lua_Statei");
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original camera mover reference state\n");
    }
    (void)damage_lua.open_base(lua);
    (void)damage_lua.open_math(lua);
    damage_lua.set_top(lua, 0);
    const char *paths[] = {"/data/local/tmp/ab520/camera/mover-reset-oracle.lua",
                           "/data/local/tmp/ab520/camera/mover-update-oracle.lua",
                           "/data/local/tmp/ab520/camera/mover-position-oracle.lua",
                           "/data/local/tmp/ab520/camera/mover-zoom-oracle.lua",
                           "/data/local/tmp/ab520/camera/mover-set-position-oracle.lua",
                           "/data/local/tmp/ab520/camera/mover-set-zoom-oracle.lua",
                           "/data/local/tmp/ab520/camera/mover-clear-oracle.lua",
                           "/data/local/tmp/ab520/camera/mover-cubic-oracle.lua"};
    const char *names[] = {"mover-reset", "mover-update",       "mover-position",
                           "mover-zoom",  "mover-set-position", "mover-set-zoom",
                           "mover-clear", "mover-cubic"};
    for (size_t index = 0; index < 8; ++index) {
        load_selected_function(lua, paths[index], names[index]);
    }
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 1, "__mul", mover_vector_multiply);
    damage_lua.set_field(lua, -10002, "_ab_mover_vector_meta");
    mover_bind_vector(lua, "mover-reset");
    mover_bind_vector(lua, "mover-set-position");
    static const float durations[] = {0, -1, -0.0f, 0.125f, 0.5f, 1, 2, 8};
    static const float seconds[] = {
        0, -0.0f, 0x1.111112p-6f, 0x1.111112p-5f, 0.125f, 0.5f, 2, -0.125f, 1};
    uint32_t random = UINT32_C(0x520ca6e1);
    semihost(4, "AB_ANDROID_CAMERA_MOVER_V1 128 64\n");
    for (size_t item = 0; item < 128; ++item) {
        damage_lua.create_table(lua, 0, 24);
        const char *fields[] = {"updatePosition", "updateZoom", "movementTweenFunction",
                                "zoomTweenFunction"};
        const char *functions[] = {names[2], names[3], names[7], names[7]};
        for (size_t field = 0; field < 4; ++field) {
            damage_lua.get_field(lua, -10002, functions[field]);
            damage_lua.set_field(lua, 1, fields[field]);
        }
        damage_lua.set_field(lua, -10002, "_ab_mover_state");
        const uint32_t reset[6] = {0};
        mover_call(lua, names[0], reset);
        for (size_t step = 0; step < 64; ++step) {
            uint32_t input[6] = {random_word(&random) % 8};
            if (input[0] > 5) {
                input[0] = 4;
            }
            for (size_t field = 1; field <= 3; ++field) {
                input[field] = random_float(&random, 106, 25, 1);
            }
            FloatBits duration = {.number = durations[random_word(&random) % 8]};
            FloatBits delta = {.number = seconds[random_word(&random) % 9]};
            if (step < 11) {
                static const uint32_t operations[] = {0, 1, 2, 4, 4, 4, 2, 4, 3, 4, 0};
                input[0] = operations[step];
                duration.number = step == 1 ? 0.5f : step == 2 ? 1.0f : 0;
                delta.number = step == 3 ? 0 : step == 4 ? 0.25f : 0.5f;
                if (step == 9) {
                    delta.bits = UINT32_C(0x7fc00000);
                }
            }
            if (input[0] == 5 && duration.number <= 0) {
                duration.number = 1;
            }
            input[4] = duration.bits;
            input[5] = delta.bits;
            print_words(input, 6);
            uint32_t output[17] = {0};
            if (input[0] == 5) {
                damage_lua.get_field(lua, -10002, names[7]);
                for (size_t field = 1; field <= 4; ++field) {
                    FloatBits value = {.bits = input[field]};
                    damage_lua.push_number(lua, value.number);
                }
                if (damage_lua.call(lua, 4, 1, 0) || damage_lua.type(lua, -1) != 3) {
                    fail("Original camera cubic tween failed\n");
                }
                FloatBits value = {.number = damage_lua.to_number(lua, -1)};
                output[16] = value.bits;
                damage_lua.set_top(lua, 0);
            } else {
                const char *method = input[0] == 0   ? names[0]
                                     : input[0] == 1 ? names[4]
                                     : input[0] == 2 ? names[5]
                                     : input[0] == 3 ? names[6]
                                                     : names[1];
                mover_call(lua, method, input);
            }
            mover_snapshot(lua, output);
            print_words(output, 17);
        }
    }
    damage_lua.close_state(lua);
    semihost(4, "AB camera mover reference complete\n");
}
#endif

#endif
