#ifndef AB_ORACLE_CAMERA_DRAG_H
#define AB_ORACLE_CAMERA_DRAG_H

static uint32_t drag_camera[30], drag_coordinates[4], drag_conversion_count;
static void (*drag_transform)(FloatBits[2], const void *, float, float);
static void (*drag_raw_get)(void *, int, int);
static unsigned (*drag_length)(void *, int);

static int drag_distance(void *lua) {
    if (damage_lua.get_top(lua) != 2 || damage_lua.type(lua, 1) != 3 ||
        damage_lua.type(lua, 2) != 3) {
        fail("Unexpected drag coordinate boundary\n");
    }
    FloatBits output[2];
    drag_transform(output, drag_coordinates, damage_lua.to_number(lua, 1),
                   damage_lua.to_number(lua, 2));
    damage_lua.push_number(lua, output[0].number);
    damage_lua.push_number(lua, output[1].number);
    ++drag_conversion_count;
    return 2;
}

static uint32_t drag_field(void *lua, int table, const char *name) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, table, name);
    if (damage_lua.type(lua, -1) != 3) {
        fail("Unexpected original drag vector field\n");
    }
    FloatBits value = {.number = damage_lua.to_number(lua, -1)};
    damage_lua.set_top(lua, top);
    return value.bits;
}

static void drag_snapshot(void *lua, uint32_t output[21]) {
    damage_lua.get_field(lua, -10002, "_ab_drag");
    const char *fields[] = {"startX",     "startY",         "anchorX",
                            "anchorY",    "currentScreenX", "currentScreenY",
                            "oldScreenX", "oldScreenY"};
    for (size_t index = 0; index < 8; ++index) {
        damage_lua.get_field(lua, 1, fields[index]);
        int type = damage_lua.type(lua, 2);
        if (type != 0 && type != 3) {
            fail("Unexpected original drag numeric presence\n");
        }
        if (type == 3) {
            output[0] |= 1u << index;
            FloatBits value = {.number = damage_lua.to_number(lua, 2)};
            output[1 + index] = value.bits;
        }
        damage_lua.set_top(lua, 1);
    }
    damage_lua.get_field(lua, 1, "velocityBuffer");
    if (damage_lua.type(lua, 2) != 5 || drag_length(lua, 2) > 5) {
        fail("Unexpected original drag velocity buffer\n");
    }
    output[9] = drag_length(lua, 2);
    for (unsigned index = 0; index < output[9]; ++index) {
        drag_raw_get(lua, 2, (int)index + 1);
        output[10 + index * 2] = drag_field(lua, 3, "x");
        output[11 + index * 2] = drag_field(lua, 3, "y");
        damage_lua.set_top(lua, 2);
    }
    damage_lua.set_top(lua, 1);
    output[20] = drag_field(lua, 1, "velocityBufferSize");
    damage_lua.set_top(lua, 0);
}

static void drag_command(void *lua, const uint32_t input[10]) {
    const char *names[] = {"drag-start",  "drag-update",  "drag-release",
                           "drag-sample", "drag-average", "drag-dragging"};
    for (size_t field = 0; field < 4; ++field) {
        drag_camera[26 + field] = input[6 + field];
    }
    drag_conversion_count = 0;
    damage_lua.get_field(lua, -10002, names[input[0]]);
    damage_lua.get_field(lua, -10002, "_ab_drag");
    int arguments = 1;
    int returns = input[0] == 1 || input[0] == 2   ? 2
                  : input[0] == 4 || input[0] == 5 ? 1
                                                   : 0;
    if (input[0] == 0 || input[0] == 1) {
        if (input[0] == 1) {
            FloatBits seconds = {.bits = input[1]};
            damage_lua.push_number(lua, seconds.number);
            ++arguments;
        }
        size_t end = input[0] == 0 ? 6u : 4u;
        for (size_t field = 2; field < end; ++field) {
            FloatBits value = {.bits = input[field]};
            damage_lua.push_number(lua, value.number);
            ++arguments;
        }
    } else if (input[0] == 3) {
        damage_lua.create_table(lua, 0, 2);
        damage_number(lua, 3, "x", input[2]);
        damage_number(lua, 3, "y", input[3]);
        ++arguments;
    }
    if (damage_lua.call(lua, arguments, returns, 0)) {
        const char *message = damage_lua.to_string(lua, -1, NULL);
        fail(message ? message : "Original camera drag command failed\n");
    }
    uint32_t output[28] = {0};
    if (input[0] == 1) {
        output[21] = 1;
        for (int axis = 0; axis < 2; ++axis) {
            FloatBits value = {.number = damage_lua.to_number(lua, axis + 1)};
            output[22 + (size_t)axis] = value.bits;
        }
    } else if (input[0] == 2) {
        output[21] = 3;
        output[22] = drag_field(lua, 1, "x");
        output[23] = drag_field(lua, 1, "y");
        output[24] = drag_field(lua, 2, "x");
        output[25] = drag_field(lua, 2, "y");
    } else if (input[0] == 4) {
        output[21] = 2;
        output[24] = drag_field(lua, 1, "x");
        output[25] = drag_field(lua, 1, "y");
    } else if (input[0] == 5) {
        output[21] = 4;
        output[26] = (uint32_t)damage_lua.to_boolean(lua, 1);
    }
    damage_lua.set_top(lua, 0);
    drag_snapshot(lua, output);
    output[27] = drag_conversion_count;
    print_words(output, 28);
}

static void probe_camera_drag(void) {
    initialize_damage_lua();
    drag_raw_get =
        (void (*)(void *, int, int))find_function("_Z11lua_rawgetiP9lua_Stateii");
    drag_length =
        (unsigned (*)(void *, int))find_function("_Z10lua_objlenP9lua_Statei");
    drag_transform = (void (*)(FloatBits[2], const void *, float, float))find_function(
        "_ZNK20CoordinateTransforms21screenToWorldDistanceEff");
    drag_coordinates[3] = (uint32_t)(uintptr_t)drag_camera;
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original camera drag reference state\n");
    }
    (void)damage_lua.open_base(lua);
    int (*open_table)(void *) =
        (int (*)(void *))find_function("_Z13luaopen_tableP9lua_State");
    (void)open_table(lua);
    damage_lua.set_top(lua, 0);
    const char *names[] = {"start",    "update", "release",
                           "dragging", "sample", "average"};
    const char *methods[] = {"startDrag",           "update",
                             "releaseDrag",         "isDragging",
                             "addToVelocityBuffer", "averageVelocityBuffer"};
    const char *paths[] = {"/data/local/tmp/ab520/camera/drag-start-oracle.lua",
                           "/data/local/tmp/ab520/camera/drag-update-oracle.lua",
                           "/data/local/tmp/ab520/camera/drag-release-oracle.lua",
                           "/data/local/tmp/ab520/camera/drag-dragging-oracle.lua",
                           "/data/local/tmp/ab520/camera/drag-sample-oracle.lua",
                           "/data/local/tmp/ab520/camera/drag-average-oracle.lua"};
    for (size_t index = 0; index < 6; ++index) {
        load_selected_function(lua, paths[index], names[index]);
        damage_lua.get_field(lua, -10002, names[index]);
        damage_lua.set_field(lua, -10002, methods[index]);
        damage_lua.get_field(lua, -10002, names[index]);
        const char *globals[] = {"drag-start",    "drag-update", "drag-release",
                                 "drag-dragging", "drag-sample", "drag-average"};
        damage_lua.set_field(lua, -10002, globals[index]);
    }
    damage_function(lua, -10002, "screenToWorldDistance", drag_distance);
    uint32_t random = UINT32_C(0x520d2a61);
    semihost(4, "AB_ANDROID_CAMERA_DRAG_V2 128 64\n");
    for (size_t item = 0; item < 128; ++item) {
        FloatBits width = {.number = item % 2 ? 960 : 480};
        FloatBits height = {.number = item % 3 ? 320 : 640};
        uint32_t setup[2] = {width.bits, height.bits};
        print_words(setup, 2);
        damage_number(lua, -10002, "screenWidth", width.bits);
        damage_number(lua, -10002, "screenHeight", height.bits);
        damage_lua.create_table(lua, 0, 16);
        for (size_t index = 0; index < 6; ++index) {
            damage_lua.get_field(lua, -10002, methods[index]);
            damage_lua.set_field(lua, 1, methods[index]);
        }
        damage_lua.set_field(lua, -10002, "_ab_drag");
        int active = 0;
        for (size_t step = 0; step < 64; ++step) {
            uint32_t input[10];
            uint32_t operation = random_word(&random) % 10;
            input[0] = operation < 6 ? operation : 1u;
            if (step == 0 || (input[0] == 1 && !active)) {
                input[0] = 0;
            }
            FloatBits seconds = {.number = step % 2 ? 0x1.111112p-5f : 0.125f};
            input[1] = seconds.bits;
            for (size_t field = 2; field < 8; ++field) {
                input[field] = random_float(&random, 123, 14, 1);
            }
            input[8] = random_float(&random, 125, 4, 0);
            input[9] = random_float(&random, 125, 4, 0);
            if (item % 5 == 0) {
                input[6] = UINT32_C(0x4c000000);
                input[7] = UINT32_C(0xcc000000);
            }
            static const uint32_t special[] = {0x7f800000, 0xff800000, 0x7fc12345,
                                               0xffc54321, 0x7f812345, 0xff854321,
                                               0,          0x80000000};
            if (step < 12) {
                static const uint32_t operations[] = {0, 4, 3, 3, 4, 1,
                                                      4, 2, 0, 1, 4, 2};
                input[0] = operations[step];
                if (step == 2 || step == 3) {
                    size_t first = step == 2 ? item % 8 : (item / 8) % 8;
                    size_t second = step == 2 ? (item / 8) % 8 : item % 8;
                    input[2] = special[first];
                    input[3] = special[second];
                }
            }
            if (input[0] == 1 && (step == 5 || step == 9 || step % 11 == 0)) {
                input[1] = step == 9 ? UINT32_C(0x80000000) : 0;
                damage_lua.get_field(lua, -10002, "_ab_drag");
                input[2] = drag_field(lua, 1, "currentScreenX");
                input[3] = drag_field(lua, 1, "currentScreenY");
                damage_lua.set_top(lua, 0);
                if (step == 5) {
                    FloatBits changed = {.bits = input[item % 2 ? 2 : 3]};
                    changed.number = changed.number + 1;
                    input[item % 2 ? 2 : 3] = changed.bits;
                }
            } else if (input[0] == 1 && step % 13 == 0) {
                FloatBits negative = {.number = -0.125f};
                input[1] = negative.bits;
            }
            if (input[0] == 0) {
                active = 1;
            } else if (input[0] == 2) {
                active = 0;
            }
            print_words(input, 10);
            drag_command(lua, input);
        }
    }
    damage_lua.close_state(lua);
    semihost(4, "AB camera drag reference complete\n");
}

#endif
