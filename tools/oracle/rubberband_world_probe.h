#ifndef AB_ORACLE_RUBBERBAND_WORLD_H
#define AB_ORACLE_RUBBERBAND_WORLD_H

static RubberbandFixture rubberband_world_fixture;
static FloatBits rubberband_world_camera[3];
static uint32_t rubberband_world_events[4], rubberband_world_event_count;
static uint32_t rubberband_world_projected[5];

static void rubberband_world_event(uint32_t kind) {
    if (rubberband_world_event_count >= 4) {
        fail("Unexpected original rubberband world event count\n");
    }
    rubberband_world_events[rubberband_world_event_count++] = kind;
}

static int rubberband_world_camera_value(void *lua, unsigned field) {
    if (damage_lua.get_top(lua) != 1 || damage_lua.type(lua, 1) != 5) {
        fail("Unexpected original rubberband camera boundary\n");
    }
    rubberband_world_event(field + 1);
    damage_lua.push_number(lua, rubberband_world_camera[field].number);
    return 1;
}

static int rubberband_world_left(void *lua) {
    return rubberband_world_camera_value(lua, 0);
}

static int rubberband_world_top(void *lua) {
    return rubberband_world_camera_value(lua, 1);
}

static int rubberband_world_zoom(void *lua) {
    return rubberband_world_camera_value(lua, 2);
}

static int rubberband_world_draw(void *lua) {
    if (damage_lua.get_top(lua) != 6 || !damage_named(lua, 6, "SLINGSHOT_PIXEL")) {
        fail("Unexpected original rubberband draw boundary\n");
    }
    rubberband_world_event(4);
    FloatBits input[5];
    for (size_t field = 0; field < 5; ++field) {
        if (damage_lua.type(lua, (int)field + 1) != 3) {
            fail("Unexpected original rubberband projected argument\n");
        }
        input[field].number = damage_lua.to_number(lua, (int)field + 1);
        rubberband_world_projected[field] = input[field].bits;
    }
    RubberbandFixture *fixture = &rubberband_world_fixture;
    fixture->draw(fixture->game, input[0].number, input[1].number, input[2].number,
                  input[3].number, input[4].number, &fixture->name);
    return 0;
}

static void probe_rubberband_world(void) {
    initialize_damage_lua();
    rubberband_prepare(&rubberband_world_fixture);
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original rubberband world reference state\n");
    }
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    load_selected_function(lua, "/data/local/tmp/ab520/ui/rubberband-world-oracle.lua",
                           "_ab_band_world");
    damage_lua.push_boolean(lua, 0);
    damage_lua.set_field(lua, -10002, "g_isPC");
    damage_lua.push_string(lua, "android");
    damage_lua.set_field(lua, -10002, "deviceModel");
    damage_lua.create_table(lua, 0, 3);
    damage_function(lua, 1, "getScreenLeftForRendering", rubberband_world_left);
    damage_function(lua, 1, "getScreenTopForRendering", rubberband_world_top);
    damage_function(lua, 1, "getZoomScale", rubberband_world_zoom);
    damage_lua.set_field(lua, -10002, "cameraSystem");
    damage_function(lua, -10002, "drawRubberband", rubberband_world_draw);

    uint32_t random = UINT32_C(0x520ca3ba);
    semihost(4, "AB_ANDROID_RUBBERBAND_WORLD_V1 4096\n");
    for (size_t item = 0; item < 4096; ++item) {
        FloatBits input[8];
        uint32_t words[8];
        for (size_t field = 0; field < 7; ++field) {
            input[field].bits = random_float(&random, 123, 12, 1);
        }
        input[7].bits = random_float(&random, 124, 6, 1);
        if (item % 13 == 0) {
            input[4].bits &= UINT32_C(0x80000000);
        }
        if (item % 17 == 0) {
            input[2] = input[0];
            input[3] = input[1];
        }
        if (item % 19 == 0) {
            input[7].bits &= UINT32_C(0x80000000);
        }
        if (item % 23 == 0) {
            input[5].bits = UINT32_C(0x4c000000);
            input[6].bits = UINT32_C(0xcc000000);
        }
        if (item % 29 == 0) {
            input[5] = input[0];
            input[6] = input[1];
        }
        for (size_t field = 0; field < 8; ++field) {
            words[field] = input[field].bits;
        }
        for (size_t field = 0; field < 3; ++field) {
            rubberband_world_camera[field] = input[5 + field];
        }
        rubberband_calls = rubberband_world_event_count = 0;
        damage_lua.get_field(lua, -10002, "_ab_band_world");
        damage_lua.create_table(lua, 0, 0);
        for (size_t field = 0; field < 5; ++field) {
            damage_lua.push_number(lua, input[field].number);
        }
        if (damage_lua.call(lua, 6, 0, 0)) {
            const char *message = damage_lua.to_string(lua, -1, NULL);
            fail(message ? message : "Original rubberband world call failed\n");
        }
        damage_lua.set_top(lua, 0);
        if (rubberband_world_event_count != 4) {
            fail("Incomplete original rubberband world events\n");
        }
        print_words(words, 8);
        print_words(rubberband_world_projected, 5);
        print_words(rubberband_world_events, 4);
        print_words(&rubberband_calls, 1);
        if (rubberband_calls) {
            print_words(rubberband_points, 12);
            print_words(rubberband_uv, 8);
        }
    }
    damage_lua.close_state(lua);
    semihost(4, "AB rubberband world reference complete\n");
}

#endif
