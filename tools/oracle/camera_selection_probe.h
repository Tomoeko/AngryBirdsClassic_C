#ifndef AB_ORACLE_CAMERA_SELECTION_H
#define AB_ORACLE_CAMERA_SELECTION_H

static uint32_t selection_input[11];
static void (*selection_raw_set)(void *, int, int);

static void selection_method(void *lua, int table, const char *method,
                             const char *name) {
    damage_lua.get_field(lua, -10002, name);
    damage_lua.set_field(lua, table, method);
}

static int selection_zoom(void *lua) {
    FloatBits value = {.bits = selection_input[3]};
    damage_lua.push_number(lua, value.number);
    return 1;
}

/* Selected snap tables are flat x/y/zoom/id records; no general deep-copy or
 * class implementation is supplied at this controlled allocation boundary. */
static int selection_copy(void *lua) {
    if (damage_lua.get_top(lua) != 1 || damage_lua.type(lua, 1) != 5) {
        fail("Unexpected selection flat-copy boundary\n");
    }
    damage_lua.create_table(lua, 0, 4);
    const char *fields[] = {"x", "y", "zoom", "id"};
    for (size_t index = 0; index < 4; ++index) {
        damage_lua.get_field(lua, 1, fields[index]);
        if (damage_lua.type(lua, 3) != 3) {
            fail("Missing original selection snap field\n");
        }
        damage_lua.set_field(lua, 2, fields[index]);
    }
    return 1;
}

static void selection_values(void *lua, int table, uint32_t output[4]) {
    int top = damage_lua.get_top(lua);
    const char *fields[] = {"x", "y", "zoom", "id"};
    for (size_t index = 0; index < 4; ++index) {
        damage_lua.get_field(lua, table, fields[index]);
        if (damage_lua.type(lua, -1) != 3) {
            fail("Missing original selection output field\n");
        }
        FloatBits value = {.number = damage_lua.to_number(lua, -1)};
        output[index] = value.bits;
        damage_lua.set_top(lua, top);
    }
}

static void selection_prepare(void *lua, uint32_t snaps[8][3]) {
    damage_lua.create_table(lua, 0, 12);
    const char *fields[] = {"getClosestCameraSnapPoint",
                            "getSnapPointInDirection",
                            "getSnapPoint",
                            "getScreenPosition",
                            "getDistanceToSnapPoint",
                            "getRelativeZoomLevel",
                            "getZoomScale"};
    const char *names[] = {"selection-closest",  "selection-direction",
                           "selection-snap",     "selection-position",
                           "selection-distance", "selection-relative",
                           "selection-zoom"};
    for (size_t index = 0; index < 7; ++index) {
        selection_method(lua, 1, fields[index], names[index]);
    }
    damage_lua.create_table(lua, 0, 2);
    damage_number(lua, 2, "x", selection_input[1]);
    damage_number(lua, 2, "y", selection_input[2]);
    damage_lua.set_field(lua, 1, "screen");
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 2, "getZoomScale", selection_zoom);
    damage_lua.set_field(lua, 1, "cameraMover");
    damage_lua.create_table(lua, 0, 3);
    damage_number(lua, 2, "closeZoomLevel", selection_input[4]);
    damage_number(lua, 2, "farZoomLevel", selection_input[5]);
    selection_method(lua, 2, "getZoomLevelLimits", "selection-zoom-limits");
    damage_lua.set_field(lua, 1, "cameraLimitEnforcer");
    damage_lua.push_boolean(lua, (int)selection_input[6]);
    damage_lua.set_field(lua, 1, "snapZoom");
    damage_lua.create_table(lua, (int)selection_input[0], 0);
    for (uint32_t index = 0; index < selection_input[0]; ++index) {
        damage_lua.create_table(lua, 0, 4);
        const char *coordinates[] = {"x", "y", "zoom"};
        for (size_t field = 0; field < 3; ++field) {
            damage_number(lua, 3, coordinates[field], snaps[index][field]);
        }
        damage_lua.push_number(lua, (float)(index + 1));
        damage_lua.set_field(lua, 3, "id");
        selection_raw_set(lua, 2, (int)index + 1);
    }
    damage_lua.set_field(lua, 1, "snapPoints");
    damage_lua.set_field(lua, -10002, "_ab_selection");
}

static void selection_capture(void *lua) {
    uint32_t output[11] = {0};
    const char *names[] = {"selection-relative", "selection-snap-zoom",
                           "selection-closest", "selection-direction"};
    for (size_t method = 0; method < 4; ++method) {
        damage_lua.get_field(lua, -10002, names[method]);
        damage_lua.get_field(lua, -10002, "_ab_selection");
        int arguments = 1, returns = method == 2 ? 2 : 1;
        if (method == 1) {
            FloatBits velocity = {.bits = selection_input[7]};
            damage_lua.push_number(lua, velocity.number);
            ++arguments;
        } else if (method >= 2) {
            damage_lua.create_table(lua, 0, 2);
            size_t start = method == 2 ? 9u : 7u;
            damage_number(lua, 3, "x", selection_input[start]);
            damage_number(lua, 3, "y", selection_input[start + 1]);
            ++arguments;
        }
        if (damage_lua.call(lua, arguments, returns, 0)) {
            const char *message = damage_lua.to_string(lua, -1, NULL);
            fail(message ? message : "Original camera selection query failed\n");
        }
        if (method < 2) {
            if (damage_lua.type(lua, 1) != 3) {
                fail("Missing original camera zoom query result\n");
            }
            FloatBits value = {.number = damage_lua.to_number(lua, 1)};
            output[method] = value.bits;
        } else {
            selection_values(lua, 1, output + (method == 2 ? 2 : 7));
            if (method == 2) {
                FloatBits index = {.number = damage_lua.to_number(lua, 2)};
                output[6] = index.bits;
            }
        }
        damage_lua.set_top(lua, 0);
    }
    print_words(output, 11);
}

static void probe_camera_selection(void) {
    initialize_damage_lua();
    selection_raw_set =
        (void (*)(void *, int, int))find_function("_Z11lua_rawsetiP9lua_Stateii");
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original camera selection state\n");
    }
    (void)damage_lua.open_base(lua);
    (void)damage_lua.open_math(lua);
    damage_lua.set_top(lua, 0);
    const char *names[] = {
        "closest",       "direction", "snap",     "position",    "distance",
        "relative",      "snap-zoom", "zoom",     "zoom-limits", "vector-distance",
        "vector-length", "length",    "subtract", "dot"};
    const char *paths[] = {
        "/data/local/tmp/ab520/camera/selection-closest-oracle.lua",
        "/data/local/tmp/ab520/camera/selection-direction-oracle.lua",
        "/data/local/tmp/ab520/camera/selection-snap-oracle.lua",
        "/data/local/tmp/ab520/camera/selection-position-oracle.lua",
        "/data/local/tmp/ab520/camera/selection-distance-oracle.lua",
        "/data/local/tmp/ab520/camera/selection-relative-oracle.lua",
        "/data/local/tmp/ab520/camera/selection-snap-zoom-oracle.lua",
        "/data/local/tmp/ab520/camera/selection-zoom-oracle.lua",
        "/data/local/tmp/ab520/camera/selection-zoom-limits-oracle.lua",
        "/data/local/tmp/ab520/camera/selection-vector-distance-oracle.lua",
        "/data/local/tmp/ab520/camera/selection-vector-length-oracle.lua",
        "/data/local/tmp/ab520/camera/selection-length-oracle.lua",
        "/data/local/tmp/ab520/camera/selection-subtract-oracle.lua",
        "/data/local/tmp/ab520/camera/selection-dot-oracle.lua"};
    for (size_t index = 0; index < 14; ++index) {
        load_selected_function(lua, paths[index], names[index]);
        damage_lua.get_field(lua, -10002, names[index]);
        char global[64] = "selection-";
        size_t cursor = 10;
        for (const char *name = names[index]; *name; ++name) {
            global[cursor++] = *name;
        }
        global[cursor] = 0;
        damage_lua.set_field(lua, -10002, global);
    }
    const char *utilities[] = {"vectorDistance", "vectorLength", "vLength",
                               "vectorSubtract", "vectorDot"};
    for (size_t index = 0; index < 5; ++index) {
        selection_method(lua, -10002, utilities[index], names[9 + index]);
    }
    damage_function(lua, -10002, "deepCopy", selection_copy);
    static const uint32_t velocities[] = {0xbf800000, 0x80000000, 0,
                                          0x3f800000, 0x7f800000, 0xff800000,
                                          0x7fc12345, 0x7f812345};
    uint32_t random = UINT32_C(0x5205e1ec);
    semihost(4, "AB_ANDROID_CAMERA_SELECTION_V1 4096\n");
    for (size_t item = 0; item < 4096; ++item) {
        selection_input[0] = (uint32_t)(item % 8) + 1;
        for (size_t field = 1; field < 11; ++field) {
            selection_input[field] = random_float(&random, 122, 16, 1);
        }
        selection_input[6] = (uint32_t)(item % 2);
        selection_input[7] = velocities[(item / 2) % 8];
        if (item % 7 == 0) {
            selection_input[4] = selection_input[5];
            if (item % 14 == 0) {
                selection_input[3] = selection_input[5];
            }
        }
        uint32_t snaps[8][3] = {{0}};
        for (size_t index = 0; index < selection_input[0]; ++index) {
            for (size_t field = 0; field < 3; ++field) {
                snaps[index][field] = random_float(&random, 123, 16, 1);
            }
            if (item % 11 == 0 && index) {
                snaps[index][0] = snaps[0][0];
                snaps[index][1] = snaps[0][1];
            }
            if (item % 37 == 0) {
                snaps[index][1] = UINT32_C(0x7f7fffff);
            }
        }
        if (item % 37 == 0) {
            selection_input[2] = selection_input[10] = UINT32_C(0xff7fffff);
        }
        print_words(selection_input, 11);
        for (size_t index = 0; index < selection_input[0]; ++index) {
            print_words(snaps[index], 3);
        }
        selection_prepare(lua, snaps);
        selection_capture(lua);
    }
    damage_lua.close_state(lua);
    semihost(4, "AB camera selection reference complete\n");
}

#endif
