#ifndef AB_ORACLE_UI_FRAME_GEOMETRY_H
#define AB_ORACLE_UI_FRAME_GEOMETRY_H

static void ui_geometry_call(void *lua, int arguments, int results) {
    if (damage_lua.call(lua, arguments, results, 0)) {
        const char *message = damage_lua.to_string(lua, -1, NULL);
        fail(message ? message : "Original UI geometry call failed\n");
    }
}

static void ui_geometry_frames(void *lua, uint32_t frames[8][7], uint32_t depth) {
    damage_lua.push_nil(lua);
    damage_lua.set_field(lua, -10002, "_ab_ui_frame");
    for (uint32_t cursor = depth; cursor > 0;) {
        const uint32_t *input = frames[--cursor];
        damage_lua.create_table(lua, 0, 6);
        const char *fields[] = {"x", "y", "scaleX", "scaleY"};
        for (size_t field = 0; field < 4; ++field) {
            damage_number(lua, 1, fields[field], input[field]);
        }
        damage_lua.get_field(lua, -10002, "_ab_ui_frame");
        damage_lua.set_field(lua, 1, "_parent");
        if (input[4] & 4) {
            damage_lua.create_table(lua, 0, 2);
            const char *axes[] = {"x", "y"};
            for (size_t axis = 0; axis < 2; ++axis) {
                if (input[4] & (1u << axis)) {
                    damage_number(lua, 2, axes[axis], input[5 + axis]);
                } else {
                    damage_lua.push_boolean(lua, 0);
                    damage_lua.set_field(lua, 2, axes[axis]);
                }
            }
        } else {
            damage_lua.push_boolean(lua, (int)(input[4] & 1));
        }
        damage_lua.set_field(lua, 1, "scroll");
        damage_lua.set_field(lua, -10002, "_ab_ui_frame");
    }
}

static void ui_geometry_queries(void *lua, const uint32_t setup[8]) {
    uint32_t output[6];
    const char *names[] = {"geometry-position", "geometry-scale", "geometry-scale"};
    for (size_t query = 0; query < 3; ++query) {
        damage_lua.get_field(lua, -10002, names[query]);
        if (query == 2) {
            damage_lua.push_nil(lua);
        } else {
            damage_lua.get_field(lua, -10002, "_ab_ui_frame");
        }
        ui_geometry_call(lua, 1, 2);
        for (size_t axis = 0; axis < 2; ++axis) {
            if (damage_lua.type(lua, (int)axis + 1) != 3) {
                fail("Missing original UI geometry result\n");
            }
            FloatBits value = {.number = damage_lua.to_number(lua, (int)axis + 1)};
            output[query * 2 + axis] = value.bits;
        }
        damage_lua.set_top(lua, 0);
    }
    print_words(output, 6);
    damage_lua.create_table(lua, 0, 6);
    const char *fields[] = {"x", "y", "width", "height", "scaleX", "scaleY"};
    for (size_t field = 0; field < 6; ++field) {
        if (field < 4 || (setup[1] & (1u << (field - 4)))) {
            damage_number(lua, 1, fields[field], setup[2 + field]);
        } else {
            damage_lua.push_boolean(lua, 0);
            damage_lua.set_field(lua, 1, fields[field]);
        }
    }
    damage_lua.set_field(lua, -10002, "_ab_ui_button");
    FloatBits x = {.bits = setup[2]}, y = {.bits = setup[3]};
    FloatBits width = {.bits = setup[4]}, height = {.bits = setup[5]};
    FloatBits sx = {.bits = setup[6]}, sy = {.bits = setup[7]};
    float scale_x = setup[1] & 1 ? sx.number : 1;
    float scale_y = setup[1] & 2 ? sy.number : 1;
    float half_width = width.number * 0.5f, half_height = height.number * 0.5f;
    float left = x.number - half_width * scale_x;
    float right = x.number + (width.number - half_width) * scale_x;
    float top = y.number - half_height * scale_y;
    float bottom = y.number + (height.number - half_height) * scale_y;
    float points[8][2] = {{x.number, y.number},    {left, y.number},
                          {right, y.number},       {x.number, top},
                          {x.number, bottom},      {left - 1, top - 1},
                          {right + 1, bottom + 1}, {0, 0}};
    for (size_t query = 0; query < 8; ++query) {
        uint32_t record[3];
        for (size_t axis = 0; axis < 2; ++axis) {
            FloatBits value = {.number = points[query][axis]};
            record[axis] = value.bits;
        }
        damage_lua.get_field(lua, -10002, "geometry-hit");
        damage_lua.get_field(lua, -10002, "_ab_ui_button");
        damage_lua.push_number(lua, points[query][0]);
        damage_lua.push_number(lua, points[query][1]);
        ui_geometry_call(lua, 3, 1);
        if (damage_lua.type(lua, 1) != 1) {
            fail("Missing original UI hit-test Boolean\n");
        }
        record[2] = (uint32_t)damage_lua.to_boolean(lua, 1);
        print_words(record, 3);
        damage_lua.set_top(lua, 0);
    }
}

static void probe_ui_frame_geometry(void) {
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original UI frame geometry state\n");
    }
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    const char *names[] = {"geometry-position", "geometry-scale", "geometry-hit"};
    const char *paths[] = {"/data/local/tmp/ab520/ui/geometry-position-oracle.lua",
                           "/data/local/tmp/ab520/ui/geometry-scale-oracle.lua",
                           "/data/local/tmp/ab520/ui/geometry-hit-oracle.lua"};
    for (size_t index = 0; index < 3; ++index) {
        load_selected_function(lua, paths[index], names[index]);
    }
    uint32_t random = UINT32_C(0x520f4a21);
    semihost(4, "AB_ANDROID_UI_FRAME_GEOMETRY_V1 4096 8\n");
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t setup[8] = {(uint32_t)(item % 8) + 1, (uint32_t)(item % 4)};
        for (size_t field = 2; field < 8; ++field) {
            FloatBits value = {.bits = random_float(&random, 118, 18, 1)};
            if (field >= 6) {
                value.number = (float)((int)(item + field) % 9 - 4) * 0.5f;
            }
            setup[field] = value.bits;
        }
        if (item % 19 == 0) {
            setup[4] = 1;
            setup[5] = UINT32_C(0x80000001);
            setup[2] = setup[3] = 0;
        }
        uint32_t frames[8][7] = {{0}};
        for (uint32_t index = 0; index < setup[0]; ++index) {
            for (size_t field = 0; field < 7; ++field) {
                if (field == 4) {
                    frames[index][field] = (uint32_t)((item + index) % 8);
                } else {
                    FloatBits value = {.bits = random_float(&random, 118, 18, 1)};
                    if (field == 2 || field == 3) {
                        value.bits = random_float(&random, 124, 6, 1);
                        if ((item + index + field) % 11 == 0) {
                            value.bits &= UINT32_C(0x80000000);
                        }
                    }
                    frames[index][field] = value.bits;
                }
            }
        }
        print_words(setup, 8);
        for (uint32_t index = 0; index < setup[0]; ++index) {
            print_words(frames[index], 7);
        }
        ui_geometry_frames(lua, frames, setup[0]);
        ui_geometry_queries(lua, setup);
    }
    damage_lua.close_state(lua);
    semihost(4, "AB UI frame geometry reference complete\n");
}

#endif
