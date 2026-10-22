#ifndef AB_ORACLE_GAME_SLING_MOUSE_H
#define AB_ORACLE_GAME_SLING_MOUSE_H

static uint32_t mouse_input[36], mouse_events[12][6], mouse_event_count;
static uint32_t mouse_camera[30], mouse_coordinates[4];
static bool mouse_phase, mouse_dragged;
static int (*mouse_error)(void *);
static void (*mouse_raw_get)(void *, int, int);
static void (*mouse_raw_set)(void *, int, int);
static void (*mouse_physics)(FloatBits[2], const void *, float, float);
static void (*mouse_world)(FloatBits[2], const void *, float, float);

#if defined(AB_ORACLE_GAME_UPDATE_TAIL_ONLY)
static int tail_begin_phase(void *lua);
#endif

static bool mouse_flag(uint32_t bit) {
    return (mouse_input[17] & bit) != 0;
}

static void mouse_boolean(void *lua, int table, const char *name, bool value) {
    damage_lua.push_boolean(lua, value);
    damage_lua.set_field(lua, table, name);
}

static uint32_t mouse_field(void *lua, int table, const char *name) {
    damage_lua.get_field(lua, table, name);
    if (damage_lua.type(lua, -1) != 3) {
        fail("Original mouse input field is not numeric\n");
    }
    FloatBits value = {.number = damage_lua.to_number(lua, -1)};
    damage_lua.set_top(lua, damage_lua.get_top(lua) - 1);
    return value.bits;
}

static void mouse_vector(void *lua, int table, const char *name,
                         const uint32_t values[2]) {
    damage_lua.create_table(lua, 0, 2);
    damage_number(lua, -2, "x", values[0]);
    damage_number(lua, -2, "y", values[1]);
    damage_lua.set_field(lua, table, name);
}

static void mouse_global_vector(void *lua, const char *name, uint32_t output[2]) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, name);
    output[0] = mouse_field(lua, -1, "x");
    output[1] = mouse_field(lua, -1, "y");
    damage_lua.set_top(lua, top);
}

static void mouse_event(void *lua, uint32_t kind, const uint32_t *vector) {
    if (!mouse_phase) {
        return;
    }
    if (mouse_event_count >= 12) {
        fail("Original mouse input exceeds event capacity\n");
    }
    uint32_t *event = mouse_events[mouse_event_count++];
    event[0] = kind;
    event[1] = vector ? vector[0] : 0;
    event[2] = vector ? vector[1] : 0;
    event[3] = mouse_dragged;
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, "dragStarted");
    event[4] = (uint32_t)damage_lua.to_boolean(lua, -1);
    damage_lua.get_field(lua, -10002, "inGamePressed");
    event[5] = (uint32_t)damage_lua.to_boolean(lua, -1);
    damage_lua.set_top(lua, top);
}

static int mouse_disabled(void *lua) {
    mouse_event(lua, 1, NULL);
    damage_lua.push_boolean(lua, mouse_flag(64));
    return 1;
}

static int mouse_range(void *lua) {
    mouse_event(lua, 2, NULL);
    FloatBits value = {.bits = mouse_input[13]};
    damage_lua.push_number(lua, value.number);
    return 1;
}

static int mouse_zoom(void *lua) {
    mouse_event(lua, 3, NULL);
    FloatBits value = {.bits = mouse_input[14]};
    damage_lua.push_number(lua, value.number);
    return 1;
}

static int mouse_set_dragged(void *lua) {
    if (damage_lua.get_top(lua) != 2 || damage_lua.type(lua, 2) != 5) {
        fail("Unexpected original mouse bird selection\n");
    }
    uint32_t vector[] = {mouse_field(lua, 2, "x"), mouse_field(lua, 2, "y")};
    mouse_event(lua, 4, vector);
    mouse_dragged = true;
    return 0;
}

static int mouse_is_dragged(void *lua) {
    mouse_event(lua, 5, NULL);
    damage_lua.push_boolean(lua, mouse_dragged);
    return 1;
}

static int mouse_rest(void *lua) {
    mouse_event(lua, 6, NULL);
    damage_lua.create_table(lua, 0, 2);
    damage_number(lua, -2, "x", mouse_input[9]);
    damage_number(lua, -2, "y", mouse_input[10]);
    return 1;
}

static int mouse_visible(void *lua) {
    mouse_event(lua, 7, NULL);
    damage_lua.push_boolean(lua, mouse_flag(256));
    return 1;
}

static int mouse_stretch(void *lua) {
    if (damage_lua.get_top(lua) != 2 || damage_lua.type(lua, 2) != 5) {
        fail("Unexpected original mouse stretch vector\n");
    }
    uint32_t vector[] = {mouse_field(lua, 2, "x"), mouse_field(lua, 2, "y")};
    mouse_event(lua, 8, vector);
    return 0;
}

static int mouse_release(void *lua) {
    mouse_event(lua, 9, NULL);
    return 0;
}

static int mouse_behavior(void *lua) {
    mouse_phase = true;
    damage_lua.push_boolean(lua, mouse_flag(8));
    return 1;
}

static int mouse_cancel_behavior(void *lua) {
    (void)lua;
    mouse_phase = true;
    return 0;
}

static int mouse_false(void *lua) {
    damage_lua.push_boolean(lua, false);
    return 1;
}

static int mouse_convert(void *lua, bool physics) {
    if (damage_lua.get_top(lua) != 2 || damage_lua.type(lua, 1) != 3 ||
        damage_lua.type(lua, 2) != 3) {
        fail("Unexpected original mouse point conversion\n");
    }
    FloatBits output[2];
    float x = damage_lua.to_number(lua, 1), y = damage_lua.to_number(lua, 2);
    (physics ? mouse_physics : mouse_world)(output, mouse_coordinates, x, y);
    damage_lua.push_number(lua, output[0].number);
    damage_lua.push_number(lua, output[1].number);
    return 2;
}

static int mouse_to_physics(void *lua) {
    return mouse_convert(lua, true);
}

static int mouse_to_world(void *lua) {
    return mouse_convert(lua, false);
}

static int mouse_end_phase(void *lua) {
#if defined(AB_ORACLE_GAME_UPDATE_TAIL_ONLY)
    return tail_begin_phase(lua);
#else
    damage_lua.push_string(lua, "AB_MOUSE_INPUT_PHASE_END");
    return mouse_error(lua);
#endif
}

static void mouse_load_update(void *lua) {
    static char bytes[16384];
    int file = system_call(
        5, (int)(uintptr_t)"/data/local/tmp/ab520/slingshot/game-update-oracle.lua",
        NULL, 0);
    if (file < 0) {
        fail("Cannot open original selected game update\n");
    }
    size_t size = 0;
    while (size < sizeof(bytes)) {
        int count = system_call(3, file, bytes + size, sizeof(bytes) - size);
        if (count < 0) {
            fail("Cannot read original selected game update\n");
        }
        if (!count) {
            break;
        }
        size += (size_t)count;
    }
    if (system_call(6, file, NULL, 0) || size == sizeof(bytes) ||
        damage_lua.load(lua, bytes, (unsigned)size, "original-game-update")) {
        fail("Cannot load original selected game update\n");
    }
    damage_lua.set_field(lua, -10002, "_ab_mouse_update");
    damage_lua.set_top(lua, 0);
}

static void mouse_globals(void *lua) {
    const char *updates[] = {"HeroBirdScreenTinter", "TrajectoryRenderer",
                             "BurnManager"};
    for (size_t index = 0; index < 3; ++index) {
        damage_lua.create_table(lua, 0, 1);
        damage_function(lua, 1, "update", damage_require);
        damage_lua.set_field(lua, -10002, updates[index]);
    }
    damage_function(lua, -10002, "callGameUpdateFunctions", damage_require);
    damage_function(lua, -10002, "setPhysicsEnabled", damage_require);
    damage_function(lua, -10002, "g_game_win_condition", mouse_false);
    damage_function(lua, -10002, "g_game_lose_condition", mouse_false);
    damage_function(lua, -10002, "screenToPhysicsTransform", mouse_to_physics);
    damage_function(lua, -10002, "screenToWorldTransform", mouse_to_world);
    damage_lua.create_table(lua, 0, 2);
    damage_function(lua, 1, "updateShake", damage_require);
    damage_function(lua, 1, "getZoomScale", mouse_zoom);
    damage_lua.set_field(lua, -10002, "cameraSystem");
    damage_lua.create_table(lua, 0, 2);
    damage_function(lua, 1, "handleTouches", mouse_behavior);
    damage_function(lua, 1, "cancelTouches", mouse_cancel_behavior);
    damage_lua.set_field(lua, -10002, "BehaviorTouchHandler");
    damage_lua.create_table(lua, 0, 0);
    damage_lua.set_field(lua, -10002, "g_level_scripts");
    damage_lua.create_table(lua, 0, 10);
    damage_function(lua, 1, "isSlingshotDisabledForCurrentLevel", mouse_disabled);
    damage_function(lua, 1, "getShootRange", mouse_range);
    damage_function(lua, 1, "setDraggedBird", mouse_set_dragged);
    damage_function(lua, 1, "isBirdDragged", mouse_is_dragged);
    damage_function(lua, 1, "getLevelStartPosition", mouse_rest);
    damage_function(lua, 1, "isSlingshotVisible", mouse_visible);
    damage_function(lua, 1, "stretchRubberBand", mouse_stretch);
    damage_function(lua, 1, "releaseRubberBandToLaunchBird", mouse_release);
    damage_function(lua, 1, "gameUpdate", mouse_end_phase);
    damage_lua.set_field(lua, -10002, "_ab_mouse_sling");
    damage_lua.get_field(lua, -10002, "_ab_mouse_update");
    damage_lua.get_field(lua, -10002, "_ab_mouse_sling");
    const char *(*setup)(void *, int, int) =
        (const char *(*)(void *, int, int))find_function(
            "_Z14lua_setupvalueP9lua_Stateii");
    if (!setup(lua, 1, 1)) {
        fail("Cannot resolve original game update sling upvalue\n");
    }
    damage_lua.set_top(lua, 0);
    mouse_boolean(lua, -10002, "g_usingGestureControls", false);
    mouse_boolean(lua, -10002, "g_deltaBasedSlingshot", false);
    mouse_boolean(lua, -10002, "g_isAccelerometerControl", false);
    mouse_boolean(lua, -10002, "cheatsEnabled", false);
    mouse_boolean(lua, -10002, "releaseBuild", true);
    damage_lua.push_string(lua, "Level1");
    damage_lua.set_field(lua, -10002, "levelName");
}

static void mouse_prepare(void *lua) {
    const uint32_t zero[2] = {0};
    const char *vectors[] = {"cursor", "oldCursor", "draggingStartPosPhysics",
                             "draggingStartPosScreen"};
    const size_t offsets[] = {1, 3, 5, 7};
    for (size_t index = 0; index < 4; ++index) {
        mouse_vector(lua, -10002, vectors[index], mouse_input + offsets[index]);
    }
    const char *empty[] = {"cursorPhysics", "cursorWorld", "oldCursorWorld"};
    for (size_t index = 0; index < 3; ++index) {
        mouse_vector(lua, -10002, empty[index], zero);
    }
    const char *keys[] = {"keyPressed", "keyHold", "keyReleased"};
    for (size_t index = 0; index < 3; ++index) {
        damage_lua.create_table(lua, 0, 1);
        mouse_boolean(lua, 1, "LBUTTON", mouse_flag(1u << index));
        damage_lua.set_field(lua, -10002, keys[index]);
    }
    mouse_boolean(lua, -10002, "skipInput", mouse_flag(8));
    mouse_boolean(lua, -10002, "levelCompleted", mouse_flag(16));
    mouse_boolean(lua, -10002, "birdReady", mouse_flag(128));
    mouse_boolean(lua, -10002, "dragStarted", mouse_flag(1024));
    mouse_boolean(lua, -10002, "inGamePressed", mouse_flag(2048));
    mouse_dragged = mouse_flag(512);
    if (mouse_flag(32)) {
        damage_lua.push_string(lua, "first");
    } else {
        damage_lua.push_nil(lua);
    }
    damage_lua.set_field(lua, -10002, "currentBirdName");
    damage_number(lua, -10002, "screenWidth", mouse_input[15]);
    damage_number(lua, -10002, "touchcount", mouse_input[16]);
    damage_number(lua, -10002, "dragCursorIndex", mouse_input[18]);
    const char *timers[] = {"quadClickTimer", "gameTimer", "levelCompleteTimer",
                            "g_time_spent_in_current_level"};
    for (size_t index = 0; index < 4; ++index) {
        damage_number(lua, -10002, timers[index], 0);
    }
    damage_number(lua, -10002, "levelStartTimer", UINT32_C(0x41200000));
    damage_lua.create_table(lua, 0, 2);
    mouse_boolean(lua, 1, "doNotTimeout", true);
    damage_lua.create_table(lua, 0, 1);
    mouse_vector(lua, 2, "first", mouse_input + 11);
    damage_lua.set_field(lua, 1, "world");
    damage_lua.set_field(lua, -10002, "objects");
    damage_lua.create_table(lua, 5, 0);
    for (size_t index = 0; index < 5; ++index) {
        damage_lua.create_table(lua, 0, 3);
        damage_number(lua, 2, "dx", mouse_input[19 + index * 3]);
        damage_number(lua, 2, "dy", mouse_input[20 + index * 3]);
        damage_number(lua, 2, "dt", mouse_input[21 + index * 3]);
        mouse_raw_set(lua, 1, (int)index + 1);
    }
    damage_lua.set_field(lua, -10002, "dragCursorTable");
}

#if defined(AB_ORACLE_GAME_SLING_MOUSE_ONLY)
static void mouse_snapshot(void *lua, uint32_t output[29]) {
    for (size_t index = 0; index < 29; ++index) {
        output[index] = 0;
    }
    const char *vectors[] = {"cursorPhysics", "oldCursor", "draggingStartPosPhysics",
                             "draggingStartPosScreen"};
    for (size_t index = 0; index < 4; ++index) {
        mouse_global_vector(lua, vectors[index], output + index * 2);
    }
    damage_lua.get_field(lua, -10002, "dragStarted");
    output[8] = (uint32_t)damage_lua.to_boolean(lua, -1);
    damage_lua.get_field(lua, -10002, "inGamePressed");
    output[9] = (uint32_t)damage_lua.to_boolean(lua, -1);
    damage_lua.set_top(lua, 0);
    output[10] = mouse_dragged;
    output[11] = mouse_field(lua, -10002, "dragCursorIndex");
    damage_lua.get_field(lua, -10002, "dragCursorTable");
    for (size_t index = 0; index < 5; ++index) {
        mouse_raw_get(lua, 1, (int)index + 1);
        int kind = damage_lua.type(lua, -1);
        if (kind == 5) {
            output[12] |= 1u << index;
            output[13 + index * 3] = mouse_field(lua, 2, "dx");
            output[14 + index * 3] = mouse_field(lua, 2, "dy");
            output[15 + index * 3] = mouse_field(lua, 2, "dt");
        } else if (kind != 0) {
            fail("Unexpected original mouse sample value\n");
        }
        damage_lua.set_top(lua, 1);
    }
    damage_lua.set_top(lua, 0);
    output[28] = mouse_event_count;
}
#endif

static void *mouse_open_reference(void) {
    void *lua = open_sling_reference();
    mouse_error = (int (*)(void *))find_function("_Z9lua_errorP9lua_State");
    mouse_raw_get =
        (void (*)(void *, int, int))find_function("_Z11lua_rawgetiP9lua_Stateii");
    mouse_raw_set =
        (void (*)(void *, int, int))find_function("_Z11lua_rawsetiP9lua_Stateii");
    mouse_physics = (void (*)(FloatBits[2], const void *, float, float))find_function(
        "_ZNK20CoordinateTransforms24screenToPhysicsTransformEff");
    mouse_world = (void (*)(FloatBits[2], const void *, float, float))find_function(
        "_ZNK20CoordinateTransforms22screenToWorldTransformEff");
    mouse_coordinates[3] = (uint32_t)(uintptr_t)mouse_camera;
    mouse_load_update(lua);
    mouse_globals(lua);
    return lua;
}

#if defined(AB_ORACLE_GAME_SLING_MOUSE_ONLY)
static void probe_game_sling_mouse(void) {
    void *lua = mouse_open_reference();
    uint32_t random = UINT32_C(0x5201ab37);
    semihost(4, "AB_ANDROID_GAME_SLING_MOUSE_V1 4160\n");
    for (size_t item = 0; item < 4160; ++item) {
        for (size_t field = 0; field < 36; ++field) {
            FloatBits value = {
                .number = (float)((int32_t)(random_word(&random) % 4097) - 2048) / 8};
            mouse_input[field] = value.bits;
        }
        const float times[] = {0, 0.001f, 0.016f, 0.017f, 0.033333335f, 0.1f};
        FloatBits seconds = {.number = times[item % 6]};
        mouse_input[0] = seconds.bits;
        FloatBits range = {.number = (float)(random_word(&random) % 129) / 16};
        mouse_input[13] = range.bits;
        const float zooms[] = {0.25f, 0.5f, 0.7f, 1, 1.5f, 2, 3, 4};
        FloatBits zoom = {.number = zooms[item % 8]};
        mouse_input[14] = zoom.bits;
        FloatBits width = {.number = (float)(240 + (item % 4) * 240)};
        mouse_input[15] = width.bits;
        FloatBits touches = {.number = (float)(item % 3)};
        mouse_input[16] = touches.bits;
        mouse_input[17] = (uint32_t)item;
        FloatBits sample_index = {.number = (float)(item % 5 + 1)};
        mouse_input[18] = sample_index.bits;
        for (size_t index = 0; index < 5; ++index) {
            FloatBits elapsed = {.number = times[(item + index) % 6]};
            mouse_input[21 + index * 3] = elapsed.bits;
        }
        mouse_camera[26] = mouse_input[34];
        mouse_camera[27] = mouse_input[35];
        mouse_camera[28] = mouse_camera[29] = mouse_input[14];
        if (item % 4 == 1 || item % 8 == 3) {
            FloatBits position[2];
            FloatBits x = {.bits = mouse_input[1]}, y = {.bits = mouse_input[2]};
            mouse_physics(position, mouse_coordinates, x.number, y.number);
            mouse_input[11] = position[0].bits;
            mouse_input[12] = position[1].bits;
        }
        if (item >= 4096) {
            /* Exact selection-radius boundaries and the ten-call path with
             * simultaneous press/hold/release. Cursor physics is positive zero. */
            static const uint32_t offsets[] = {0,
                                               UINT32_C(0x407fffff),
                                               UINT32_C(0x40800000),
                                               UINT32_C(0x40800001),
                                               UINT32_C(0xc07fffff),
                                               UINT32_C(0xc0800000),
                                               UINT32_C(0xc0800001),
                                               UINT32_C(0x80000000)};
            mouse_input[1] = mouse_input[2] = 0;
            mouse_input[11] = offsets[item % 8];
            mouse_input[12] = 0;
            mouse_input[13] = UINT32_C(0x40800000);
            mouse_input[14] = UINT32_C(0x3f800000);
            mouse_input[15] = UINT32_C(0x43f00000);
            mouse_input[16] = UINT32_C(0x3f800000);
            mouse_input[17] = 1 | 2 | 4 | 32 | 128 | 256 | 1024 | 2048;
            mouse_input[34] = mouse_input[35] = 0;
            mouse_camera[26] = mouse_camera[27] = 0;
            mouse_camera[28] = mouse_camera[29] = mouse_input[14];
        }
        mouse_prepare(lua);
        mouse_phase = false;
        mouse_event_count = 0;
        damage_lua.get_field(lua, -10002, "_ab_mouse_update");
        damage_lua.push_number(lua, seconds.number);
        damage_lua.push_number(lua, 0);
        int status = damage_lua.call(lua, 2, 0, 0);
        if (!status || !damage_named(lua, -1, "AB_MOUSE_INPUT_PHASE_END")) {
            const char *error = damage_lua.to_string(lua, -1, NULL);
            fail(error ? error : "Original mouse phase did not reach its boundary\n");
        }
        damage_lua.set_top(lua, 0);
        uint32_t output[29];
        mouse_snapshot(lua, output);
        print_words(mouse_input, 36);
        print_words(output, 29);
        for (size_t index = 0; index < mouse_event_count; ++index) {
            print_words(mouse_events[index], 6);
        }
    }
    damage_lua.close_state(lua);
    semihost(4, "AB game sling mouse reference complete\n");
}
#endif

#endif
