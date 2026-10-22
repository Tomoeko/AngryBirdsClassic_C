#ifndef AB_ORACLE_UI_IMAGE_HIT_H
#define AB_ORACLE_UI_IMAGE_HIT_H

static int (*ui_image_native_rectangle)(void *, const float *, const float *);
static uint32_t ui_image_setup[14], ui_image_rectangle[8];

static void ui_image_call(void *lua, int arguments, int results) {
    if (damage_lua.call(lua, arguments, results, 0)) {
        const char *message = damage_lua.to_string(lua, -1, NULL);
        fail(message ? message : "Original ImageButton hit query failed\n");
    }
}

static int ui_image_sprite(void *lua, size_t offset, size_t counter) {
    if (!damage_named(lua, 1, "controlled-sprite")) {
        fail("Unexpected original hit-test resource query\n");
    }
    ++ui_image_rectangle[counter];
    for (size_t axis = 0; axis < 2; ++axis) {
        FloatBits value = {.bits = ui_image_setup[offset + axis]};
        damage_lua.push_number(lua, value.number);
    }
    return 2;
}

static int ui_image_bounds(void *lua) {
    return ui_image_sprite(lua, 6, 6);
}

static int ui_image_pivot(void *lua) {
    return ui_image_sprite(lua, 8, 7);
}

static int ui_image_rectangle_query(void *lua) {
    float point[2], rectangle[5];
    const char *fields[] = {"x", "y", "w", "h", "angle"};
    for (size_t field = 0; field < 5; ++field) {
        if (field < 2) {
            damage_lua.get_field(lua, 1, fields[field]);
            point[field] = damage_lua.to_number(lua, -1);
            damage_lua.set_top(lua, 2);
        }
        damage_lua.get_field(lua, 2, fields[field]);
        if (damage_lua.type(lua, -1) != 3) {
            fail("Missing original ImageButton rectangle field\n");
        }
        FloatBits value = {.number = damage_lua.to_number(lua, -1)};
        rectangle[field] = value.number;
        ui_image_rectangle[1 + field] = value.bits;
        damage_lua.set_top(lua, 2);
    }
    ++ui_image_rectangle[0];
    damage_lua.push_boolean(lua, ui_image_native_rectangle(NULL, point, rectangle));
    return 1;
}

static void ui_image_prepare(void *lua, uint32_t scales[4][2]) {
    damage_lua.push_nil(lua);
    damage_lua.set_field(lua, -10002, "_ab_image_button");
    for (uint32_t cursor = ui_image_setup[0]; cursor > 0;) {
        --cursor;
        damage_lua.create_table(lua, 0, 8);
        damage_number(lua, 1, "scaleX", scales[cursor][0]);
        damage_number(lua, 1, "scaleY", scales[cursor][1]);
        damage_lua.get_field(lua, -10002, "_ab_image_button");
        damage_lua.set_field(lua, 1, "_parent");
        if (!cursor) {
            damage_number(lua, 1, "x", ui_image_setup[3]);
            damage_number(lua, 1, "y", ui_image_setup[4]);
            damage_number(lua, 1, "angle", ui_image_setup[5]);
            if (ui_image_setup[1]) {
                damage_lua.push_string(lua, "controlled-sprite");
            } else {
                damage_lua.push_boolean(lua, 0);
            }
            damage_lua.set_field(lua, 1, "image");
            damage_lua.create_table(lua, 0, 4);
            const char *names[] = {"left", "right", "top", "bottom"};
            for (size_t field = 0; field < 4; ++field) {
                if (ui_image_setup[2] & (1u << field)) {
                    damage_number(lua, 2, names[field], ui_image_setup[10 + field]);
                } else {
                    damage_lua.push_boolean(lua, 0);
                    damage_lua.set_field(lua, 2, names[field]);
                }
            }
            damage_lua.set_field(lua, 1, "hitAreaPadding");
        }
        damage_lua.set_field(lua, -10002, "_ab_image_button");
    }
}

static void ui_image_hit_query(void *lua, float x, float y, uint32_t record[3]) {
    damage_lua.get_field(lua, -10002, "image-hit");
    damage_lua.get_field(lua, -10002, "_ab_image_button");
    damage_lua.push_number(lua, x);
    damage_lua.push_number(lua, y);
    ui_image_call(lua, 3, 1);
    if (damage_lua.type(lua, 1) != 1) {
        fail("Missing original ImageButton hit Boolean\n");
    }
    FloatBits px = {.number = x}, py = {.number = y};
    record[0] = px.bits;
    record[1] = py.bits;
    record[2] = (uint32_t)damage_lua.to_boolean(lua, 1);
    damage_lua.set_top(lua, 0);
}

static void ui_image_native_queries(uint32_t *random) {
    semihost(4, "AB_ANDROID_UI_RECTANGLE_V1 65536\n");
    for (size_t item = 0; item < 65536; ++item) {
        uint32_t record[8];
        FloatBits rectangle[5], point[2];
        for (size_t field = 0; field < 7; ++field) {
            record[field] = random_float(random, 110, 30, 1);
        }
        record[4] = math_values[item % 4096][0];
        if (item % 8 < 2) {
            record[4] = item & 1 ? UINT32_C(0x80000000) : 0;
        }
        for (size_t field = 0; field < 5; ++field) {
            rectangle[field].bits = record[field];
        }
        point[0].bits = record[5];
        point[1].bits = record[6];
        if (item % 8 >= 2) {
            size_t axis = (item / 8) % 2;
            point[0] = rectangle[0];
            point[1] = rectangle[1];
            if (item % 8 != 2) {
                float displacement = rectangle[2 + axis].number * 0.5f;
                if (item % 8 == 3) {
                    displacement = -displacement;
                } else if (item % 8 == 6) {
                    displacement = displacement * 0.99999988f;
                } else if (item % 8 == 7) {
                    displacement = displacement * 1.00000024f;
                }
                FloatBits cosine = {.number = cosf(rectangle[4].number)};
                FloatBits sine = {.number = sinf(rectangle[4].number)};
                point[0].number += displacement * (axis ? -sine.number : cosine.number);
                point[1].number += displacement * (axis ? cosine.number : sine.number);
            }
        }
        record[5] = point[0].bits;
        record[6] = point[1].bits;
        float input_rectangle[5], input_point[2];
        for (size_t field = 0; field < 5; ++field) {
            input_rectangle[field] = rectangle[field].number;
        }
        input_point[0] = point[0].number;
        input_point[1] = point[1].number;
        record[7] =
            (uint32_t)ui_image_native_rectangle(NULL, input_point, input_rectangle);
        print_words(record, 8);
    }
}

static void probe_ui_image_hit(void) {
    initialize_damage_lua();
    ui_image_native_rectangle =
        (int (*)(void *, const float *, const float *))find_function(
            "_ZN8LuaUtils9MathUtils13isPointInRectERKN4math6float2ERKNS_7LuaRectE");
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original ImageButton query state\n");
    }
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    (void)damage_lua.open_math(lua);
    damage_lua.set_top(lua, 0);
    const char *paths[] = {"/data/local/tmp/ab520/ui/image-hit-scale-oracle.lua",
                           "/data/local/tmp/ab520/ui/image-hit-rotate-oracle.lua",
                           "/data/local/tmp/ab520/ui/image-hit-hit-oracle.lua"};
    const char *globals[] = {"image-scale", "image-rotate", "image-hit"};
    for (size_t index = 0; index < 3; ++index) {
        load_selected_function(lua, paths[index], globals[index]);
    }
    damage_lua.create_table(lua, 0, 1);
    damage_lua.get_field(lua, -10002, "image-scale");
    damage_lua.set_field(lua, 1, "getCumulativeScale");
    damage_lua.set_field(lua, -10002, "Frame");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.get_field(lua, -10002, "image-rotate");
    damage_lua.set_field(lua, 1, "rotatePoint");
    damage_lua.set_field(lua, -10002, "gamelua");
    damage_lua.create_table(lua, 0, 2);
    damage_function(lua, 1, "getSpriteBounds", ui_image_bounds);
    damage_function(lua, 1, "getSpritePivot", ui_image_pivot);
    damage_lua.set_field(lua, -10002, "res");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 2, "isPointInRect", ui_image_rectangle_query);
    damage_lua.set_field(lua, 1, "MathUtils");
    damage_lua.set_field(lua, -10002, "native");
    uint32_t random = UINT32_C(0x52014a6e);
    semihost(4, "AB_ANDROID_UI_IMAGE_HIT_V1 4096 10\n");
    for (size_t item = 0; item < 4096; ++item) {
        ui_image_setup[0] = (uint32_t)(item % 4) + 1;
        ui_image_setup[1] = item % 17 != 0;
        ui_image_setup[2] = (uint32_t)(item % 16);
        for (size_t field = 3; field < 14; ++field) {
            ui_image_setup[field] = random_float(&random, 116, 19, 1);
        }
        FloatBits angle = {.bits = random_float(&random, 117, 20, 1)};
        angle.number = angle.number * 0.25f;
        if (item % 4 < 2) {
            angle.bits = item & 1 ? UINT32_C(0x80000000) : 0;
        }
        ui_image_setup[5] = angle.bits;
        ui_image_setup[6] &= UINT32_C(0x7fffffff);
        ui_image_setup[7] &= UINT32_C(0x7fffffff);
        uint32_t scales[4][2];
        for (uint32_t index = 0; index < ui_image_setup[0]; ++index) {
            for (size_t axis = 0; axis < 2; ++axis) {
                scales[index][axis] = random_float(&random, 125, 5, 1);
                if ((item + index + axis) % 29 == 0) {
                    scales[index][axis] &= UINT32_C(0x80000000);
                }
            }
        }
        if (item < 16) {
            const uint32_t angles[] = {0,          0x80000000, 1,          0x80000001,
                                       0x3fc90fdb, 0xbfc90fdb, 0x40490fdb, 0xc0490fdb,
                                       0x44800000, 0xc4800000, 0x447fffff, 0xc47fffff,
                                       0x3f490fdb, 0xbf490fdb, 0x00800000, 0x80800000};
            ui_image_setup[1] = 1;
            ui_image_setup[5] = angles[item];
            ui_image_setup[3] = ui_image_setup[4] = 0;
            ui_image_setup[6] = ui_image_setup[7] = UINT32_C(0x40000000);
            ui_image_setup[8] = ui_image_setup[9] = UINT32_C(0x3f800000);
            for (size_t field = 10; field < 14; ++field) {
                ui_image_setup[field] = 0;
            }
            for (uint32_t index = 0; index < ui_image_setup[0]; ++index) {
                scales[index][0] = scales[index][1] = UINT32_C(0x3f800000);
            }
        }
        print_words(ui_image_setup, 14);
        for (uint32_t index = 0; index < ui_image_setup[0]; ++index) {
            print_words(scales[index], 2);
        }
        ui_image_prepare(lua, scales);
        for (size_t field = 0; field < 8; ++field) {
            ui_image_rectangle[field] = 0;
        }
        FloatBits x = {.bits = ui_image_setup[3]}, y = {.bits = ui_image_setup[4]};
        uint32_t first[3];
        ui_image_hit_query(lua, x.number, y.number, first);
        print_words(ui_image_rectangle, 8);
        print_words(first, 3);
        FloatBits cx = {.bits = ui_image_rectangle[1]};
        FloatBits cy = {.bits = ui_image_rectangle[2]};
        FloatBits width = {.bits = ui_image_rectangle[3]};
        FloatBits height = {.bits = ui_image_rectangle[4]};
        float points[9][2] = {{cx.number, cy.number},
                              {cx.number - width.number * 0.5f, cy.number},
                              {cx.number + width.number * 0.5f, cy.number},
                              {cx.number, cy.number - height.number * 0.5f},
                              {cx.number, cy.number + height.number * 0.5f},
                              {cx.number + width.number * 0.25f, cy.number},
                              {cx.number, cy.number + height.number * 0.25f},
                              {cx.number + width.number, cy.number + height.number},
                              {0, 0}};
        for (size_t query = 0; query < 9; ++query) {
            uint32_t record[3];
            ui_image_hit_query(lua, points[query][0], points[query][1], record);
            print_words(record, 3);
        }
    }
    damage_lua.close_state(lua);
    ui_image_native_queries(&random);
    semihost(4, "AB UI image hit reference complete\n");
}

#endif
