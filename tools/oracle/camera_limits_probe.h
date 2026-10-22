#ifndef AB_ORACLE_CAMERA_LIMITS_H
#define AB_ORACLE_CAMERA_LIMITS_H

static uint32_t limits_input[17];
static void (*limits_push_value)(void *, int);
static const char *const limits_value_fields[] = {"x", "y", "zoom"};

static int limits_current_x(void *lua) {
    FloatBits value = {.bits = limits_input[7]};
    damage_lua.push_number(lua, value.number);
    return 1;
}

static int limits_current_y(void *lua) {
    FloatBits value = {.bits = limits_input[8]};
    damage_lua.push_number(lua, value.number);
    return 1;
}

static int limits_current_zoom(void *lua) {
    FloatBits value = {.bits = limits_input[9]};
    damage_lua.push_number(lua, value.number);
    return 1;
}

/* Numeric-table copying and choosing the one available snap are controlled
 * boundaries. Original scalar math, projection, extents and correction methods
 * below execute from the selected target prototypes. */
static int limits_copy_values(void *lua) {
    if (damage_lua.get_top(lua) != 1 || damage_lua.type(lua, 1) != 5) {
        fail("Unexpected controlled camera values copy\n");
    }
    damage_lua.create_table(lua, 0, 3);
    for (size_t field = 0; field < 3; ++field) {
        damage_lua.get_field(lua, 1, limits_value_fields[field]);
        int type = damage_lua.type(lua, 3);
        if (type != 0 && type != 3) {
            fail("Unexpected controlled camera copied field\n");
        }
        damage_lua.set_field(lua, 2, limits_value_fields[field]);
    }
    return 1;
}

static int limits_default(void *lua) {
    if (damage_lua.get_top(lua) != 2) {
        fail("Unexpected controlled camera snap fallback\n");
    }
    int selected = damage_lua.type(lua, 1) == 0 ? 2 : 1;
    if (damage_lua.type(lua, selected) != 5) {
        fail("Missing controlled camera snap fallback\n");
    }
    limits_push_value(lua, selected);
    return 1;
}

static void limits_method(void *lua, int table, const char *field,
                          const char *function) {
    damage_lua.get_field(lua, -10002, function);
    damage_lua.set_field(lua, table, field);
}

static void limits_values(void *lua) {
    int table = damage_lua.get_top(lua) + 1;
    damage_lua.create_table(lua, 0, 3);
    for (size_t field = 0; field < 3; ++field) {
        if (limits_input[1] & (UINT32_C(1) << field)) {
            damage_number(lua, table, limits_value_fields[field],
                          limits_input[10 + field]);
        }
    }
}

static void limits_values_words(void *lua, int table, uint32_t output[4]) {
    int top = damage_lua.get_top(lua);
    for (size_t field = 0; field < 3; ++field) {
        damage_lua.get_field(lua, table, limits_value_fields[field]);
        int type = damage_lua.type(lua, -1);
        if (type != 0 && type != 3) {
            fail("Unexpected original camera result field\n");
        }
        if (type == 3) {
            FloatBits value = {.number = damage_lua.to_number(lua, -1)};
            output[0] |= UINT32_C(1) << field;
            output[field + 1] = value.bits;
        }
        damage_lua.set_top(lua, top);
    }
}

static void limits_prepare(void *lua, uint32_t snaps[8][3]) {
    void (*raw_set)(void *, int, int) =
        (void (*)(void *, int, int))find_function("_Z11lua_rawsetiP9lua_Stateii");
    damage_number(lua, -10002, "screenWidth", limits_input[5]);
    damage_number(lua, -10002, "screenHeight", limits_input[6]);
    damage_lua.create_table(lua, 0, 6);
    limits_method(lua, 1, "clamp", "limits-clamp");
    limits_method(lua, 1, "vectorProjection", "limits-projection");
    limits_method(lua, 1, "vectorLengthSquared", "limits-squared");
    damage_number(lua, 1, "screenWidth", limits_input[5]);
    damage_number(lua, 1, "screenHeight", limits_input[6]);
    damage_function(lua, 1, "deepCopy", limits_copy_values);
    damage_lua.set_field(lua, -10002, "gamelua");
    for (size_t axis = 0; axis < 2; ++axis) {
        if (limits_input[2] & (UINT32_C(1) << axis)) {
            FloatBits value = {.bits = limits_input[13 + axis]};
            damage_lua.push_number(lua, value.number);
        } else {
            damage_lua.push_nil(lua);
        }
        damage_lua.set_field(lua, -10002, axis ? "originalY" : "originalX");
    }
    damage_lua.create_table(lua, 0, 16);
    damage_lua.create_table(lua, 0, 4);
    damage_function(lua, 2, "getScreenX", limits_current_x);
    damage_function(lua, 2, "getScreenY", limits_current_y);
    damage_function(lua, 2, "getZoomScale", limits_current_zoom);
    limits_method(lua, 2, "getScreenExtentsForPositionAndZoom", "limits-extents");
    damage_lua.set_field(lua, 1, "cameraSystem");
    damage_number(lua, 1, "absoluteBottom", limits_input[15]);
    damage_number(lua, 1, "maxHorizontalLeeway", limits_input[16]);
    damage_lua.push_boolean(lua, (int)limits_input[4]);
    damage_lua.set_field(lua, 1, "enabled");
    damage_lua.create_table(lua, (int)limits_input[0], 0);
    for (size_t index = 0; index < limits_input[0]; ++index) {
        damage_lua.create_table(lua, 0, 3);
        for (size_t field = 0; field < 3; ++field) {
            damage_number(lua, 3, limits_value_fields[field], snaps[index][field]);
        }
        raw_set(lua, 2, (int)index + 1);
    }
    damage_lua.set_field(lua, 1, "snapPoints");
    limits_method(lua, 1, "projectWantedValuesDown", "limits-project");
    limits_method(lua, 1, "checkForBrokenConstraints", "limits-broken");
    limits_method(lua, 1, "enforceUserInputZoom", "limits-input-zoom");
    limits_method(lua, 1, "enforceUserInputPosition", "limits-input-position");
    limits_method(lua, 1, "checkZoom", "limits-zoom");
    damage_lua.set_field(lua, -10002, "_ab_limits_state");
    damage_lua.get_field(lua, -10002, "limits-initialize");
    damage_lua.get_field(lua, -10002, "_ab_limits_state");
    if (damage_lua.call(lua, 1, 0, 0)) {
        fail("Original camera limit initialization failed\n");
    }
    damage_lua.set_top(lua, 0);
}

static void limits_capture(void *lua, uint32_t output[22]) {
    const char *fields[] = {"leftBorder",     "rightBorder",        "topBorder",
                            "bottomBorder",   "closeZoomLevel",     "farZoomLevel",
                            "absoluteBottom", "maxHorizontalLeeway"};
    damage_lua.get_field(lua, -10002, "_ab_limits_state");
    for (size_t field = 0; field < 8; ++field) {
        damage_lua.get_field(lua, 1, fields[field]);
        if (damage_lua.type(lua, 2) != 3) {
            fail("Missing original camera limit field\n");
        }
        FloatBits value = {.number = damage_lua.to_number(lua, 2)};
        output[field] = value.bits;
        damage_lua.set_top(lua, 1);
    }
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "limits-project");
    damage_lua.get_field(lua, -10002, "_ab_limits_state");
    limits_values(lua);
    if (damage_lua.call(lua, 2, 2, 0) || damage_lua.type(lua, 1) != 3 ||
        damage_lua.type(lua, 2) != 3) {
        fail("Original camera snap projection failed\n");
    }
    for (size_t axis = 0; axis < 2; ++axis) {
        FloatBits value = {.number = damage_lua.to_number(lua, (int)axis + 1)};
        output[8 + axis] = value.bits;
    }
    damage_lua.set_top(lua, 0);
    for (size_t method = 0; method < 3; ++method) {
        if (method == 1 && (limits_input[1] & 4) &&
            (((limits_input[12] & UINT32_C(0x7f800000)) == UINT32_C(0x7f800000)) ||
             !(limits_input[12] & UINT32_C(0x7fffffff)))) {
            /* Direct hard position requires nonzero finite zoom.
             * Zoom-only and input-before-position paths are compared below. */
            continue;
        }
        limits_values(lua);
        damage_lua.set_field(lua, -10002, "_ab_limits_values");
        damage_lua.get_field(lua, -10002,
                             method == 0   ? "limits-zoom"
                             : method == 1 ? "limits-position"
                                           : "limits-input");
        damage_lua.get_field(lua, -10002, "_ab_limits_state");
        damage_lua.get_field(lua, -10002, "_ab_limits_values");
        if (method == 1) {
            damage_lua.push_boolean(lua, (int)limits_input[3]);
        }
        if (damage_lua.call(lua, method == 1 ? 3 : 2, method == 2 ? 1 : 0, 0)) {
            fail("Original camera limit correction failed\n");
        }
        if (method != 2) {
            damage_lua.get_field(lua, -10002, "_ab_limits_values");
        }
        limits_values_words(lua, 1, output + 10 + method * 4);
        damage_lua.set_top(lua, 0);
    }
}

static void probe_camera_limits(void) {
    initialize_damage_lua();
    limits_push_value =
        (void (*)(void *, int))find_function("_Z13lua_pushvalueP9lua_Statei");
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original camera limits state\n");
    }
    (void)damage_lua.open_base(lua);
    (void)damage_lua.open_math(lua);
    damage_lua.set_top(lua, 0);
    const char *names[] = {"initialize", "input",    "input-zoom", "input-position",
                           "project",    "position", "zoom",       "broken",
                           "extents",    "clamp",    "projection", "squared",
                           "norm"};
    const char *paths[] = {
        "/data/local/tmp/ab520/camera/limits-initialize-oracle.lua",
        "/data/local/tmp/ab520/camera/limits-input-oracle.lua",
        "/data/local/tmp/ab520/camera/limits-input-zoom-oracle.lua",
        "/data/local/tmp/ab520/camera/limits-input-position-oracle.lua",
        "/data/local/tmp/ab520/camera/limits-project-oracle.lua",
        "/data/local/tmp/ab520/camera/limits-position-oracle.lua",
        "/data/local/tmp/ab520/camera/limits-zoom-oracle.lua",
        "/data/local/tmp/ab520/camera/limits-broken-oracle.lua",
        "/data/local/tmp/ab520/camera/limits-extents-oracle.lua",
        "/data/local/tmp/ab520/camera/limits-clamp-oracle.lua",
        "/data/local/tmp/ab520/camera/limits-projection-oracle.lua",
        "/data/local/tmp/ab520/camera/limits-squared-oracle.lua",
        "/data/local/tmp/ab520/camera/limits-norm-oracle.lua"};
    for (size_t index = 0; index < 13; ++index) {
        char name[64] = "limits-";
        size_t count = 7;
        for (size_t character = 0; names[index][character]; ++character) {
            name[count++] = names[index][character];
        }
        name[count] = 0;
        load_selected_function(lua, paths[index], name);
    }
    damage_lua.get_field(lua, -10002, "limits-norm");
    damage_lua.set_field(lua, -10002, "vLengthsq");
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 1, "default", limits_default);
    damage_lua.set_field(lua, -10002, "lua");
    uint32_t random = UINT32_C(0x520ca117);
    semihost(4, "AB_ANDROID_CAMERA_LIMITS_V2 4096\n");
    for (size_t item = 0; item < 4096; ++item) {
        limits_input[0] = (uint32_t)(item % 8) + 1;
        limits_input[1] = (uint32_t)(item / 8) % 8;
        limits_input[2] = (uint32_t)(item / 64) % 4;
        limits_input[3] = (uint32_t)(item / 256) % 2;
        limits_input[4] = (uint32_t)(item / 512) % 2;
        FloatBits width = {.number = item % 3 ? 480.0f : 960.0f};
        FloatBits height = {.number = item % 5 ? 320.0f : 640.0f};
        limits_input[5] = width.bits;
        limits_input[6] = height.bits;
        limits_input[7] = random_float(&random, 123, 14, 1);
        limits_input[8] = random_float(&random, 123, 14, 1);
        limits_input[9] = random_float(&random, 125, 4, 0);
        limits_input[10] = random_float(&random, 123, 16, 1);
        limits_input[11] = random_float(&random, 123, 16, 1);
        limits_input[12] = random_float(&random, 125, 4, 0);
        static const uint32_t zoom_edges[] = {0,          0x80000000, 0x7f800000,
                                              0xff800000, 0x7fc12345, 0xffc54321,
                                              0x7f812345, 0xff854321};
        if (item % 2 == 0) {
            limits_input[12] = zoom_edges[(item / 2) % 8];
        }

        limits_input[13] = limits_input[item % 3 ? 7 : 10];
        limits_input[14] = limits_input[item % 3 ? 8 : 11];
        FloatBits bottom = {.number = 200};
        FloatBits leeway = {.number = item % 7 ? 200.0f : 0};
        limits_input[15] = bottom.bits;
        limits_input[16] = leeway.bits;
        uint32_t snaps[8][3] = {{0}};
        for (size_t index = 0; index < limits_input[0]; ++index) {
            FloatBits x = {.number = (float)((int)index * 480 - 900)};
            FloatBits y = {.number = -(float)((item + index * 137) % 500)};
            if (item % 11 == 0) {
                x.number = -100;
            }
            snaps[index][0] = x.bits;
            snaps[index][1] = y.bits;
            snaps[index][2] = random_float(&random, 125, 4, 0);
        }
        if (item % 8 == 0) {
            limits_input[10] = snaps[0][0];
        }
        print_words(limits_input, 17);
        for (size_t index = 0; index < limits_input[0]; ++index) {
            print_words(snaps[index], 3);
        }
        limits_prepare(lua, snaps);
        uint32_t output[22] = {0};
        limits_capture(lua, output);
        print_words(output, 22);
    }
    damage_lua.close_state(lua);
    semihost(4, "AB camera limits reference complete\n");
}

#endif
