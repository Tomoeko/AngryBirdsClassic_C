#ifndef AB_ORACLE_CAMERA_VIEWPORT_H
#define AB_ORACLE_CAMERA_VIEWPORT_H

static uint32_t viewport_input[7], viewport_zoom_queries;

static int viewport_zoom(void *lua) {
    if (damage_lua.get_top(lua) != 1 || damage_lua.type(lua, 1) != 5) {
        fail("Unexpected original viewport zoom query\n");
    }
    ++viewport_zoom_queries;
    FloatBits zoom = {.bits = viewport_input[6]};
    damage_lua.push_number(lua, zoom.number);
    return 1;
}

static void viewport_prepare(void *lua) {
    const char *globals[] = {"screenWidth", "screenHeight", "cameraShakeX",
                             "cameraShakeY"};
    for (size_t field = 0; field < 4; ++field) {
        damage_number(lua, -10002, globals[field], viewport_input[field + 2]);
    }
    damage_lua.get_field(lua, -10002, "_ab_viewport");
    damage_lua.create_table(lua, 0, 2);
    damage_number(lua, 2, "x", viewport_input[0]);
    damage_number(lua, 2, "y", viewport_input[1]);
    damage_lua.set_field(lua, 1, "screen");
    damage_lua.set_top(lua, 0);
}

static void probe_camera_viewport(void) {
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original viewport reference state\n");
    }
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    const char *paths[] = {
        "/data/local/tmp/ab520/viewport/viewport-left-oracle.lua",
        "/data/local/tmp/ab520/viewport/viewport-top-oracle.lua",
        "/data/local/tmp/ab520/viewport/viewport-right-oracle.lua",
        "/data/local/tmp/ab520/viewport/viewport-bottom-oracle.lua",
        "/data/local/tmp/ab520/viewport/viewport-width-oracle.lua",
        "/data/local/tmp/ab520/viewport/viewport-height-oracle.lua",
        "/data/local/tmp/ab520/viewport/viewport-render-left-oracle.lua",
        "/data/local/tmp/ab520/viewport/viewport-render-top-oracle.lua"};
    const char *names[] = {"viewport-left",        "viewport-top",
                           "viewport-right",       "viewport-bottom",
                           "viewport-width",       "viewport-height",
                           "viewport-render-left", "viewport-render-top"};
    for (size_t function = 0; function < 8; ++function) {
        load_selected_function(lua, paths[function], names[function]);
    }
    damage_lua.create_table(lua, 0, 4);
    damage_function(lua, 1, "getZoomScale", viewport_zoom);
    damage_lua.get_field(lua, -10002, names[0]);
    damage_lua.set_field(lua, 1, "getScreenLeft");
    damage_lua.get_field(lua, -10002, names[1]);
    damage_lua.set_field(lua, 1, "getScreenTop");
    damage_lua.set_field(lua, -10002, "_ab_viewport");
    uint32_t random = UINT32_C(0x520ca4e5);
    const uint32_t boundaries[] = {0,
                                   UINT32_C(0x80000000),
                                   1,
                                   2,
                                   3,
                                   UINT32_C(0x80000001),
                                   UINT32_C(0x007fffff),
                                   UINT32_C(0x807fffff),
                                   UINT32_C(0x3f800000),
                                   UINT32_C(0xbf800000),
                                   UINT32_C(0x4b800000),
                                   UINT32_C(0xcb800000)};
    semihost(4, "AB_ANDROID_CAMERA_VIEWPORT_V1 4096\n");
    for (size_t item = 0; item < 4096; ++item) {
        for (size_t field = 0; field < 6; ++field) {
            viewport_input[field] = random_float(&random, 0, 145, 1);
        }
        viewport_input[6] = random_float(&random, 122, 10, 1);
        if (item < 144) {
            viewport_input[2] = boundaries[item % 12];
            viewport_input[3] = boundaries[item / 12];
        }
        viewport_prepare(lua);
        viewport_zoom_queries = 0;
        print_words(viewport_input, 7);
        uint32_t output[9];
        for (size_t function = 0; function < 8; ++function) {
            damage_lua.get_field(lua, -10002, names[function]);
            damage_lua.get_field(lua, -10002, "_ab_viewport");
            if (damage_lua.call(lua, 1, 1, 0)) {
                const char *error = damage_lua.to_string(lua, -1, NULL);
                fail(error ? error : "Original viewport function failed\n");
            }
            if (damage_lua.type(lua, -1) != 3) {
                fail("Original viewport output is not numeric\n");
            }
            FloatBits value = {.number = damage_lua.to_number(lua, -1)};
            output[function] = value.bits;
            damage_lua.set_top(lua, 0);
        }
        output[8] = viewport_zoom_queries;
        print_words(output, 9);
    }
    damage_lua.close_state(lua);
    semihost(4, "AB camera viewport reference complete\n");
}

#endif
