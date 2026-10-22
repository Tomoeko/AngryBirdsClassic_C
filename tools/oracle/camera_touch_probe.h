#ifndef AB_ORACLE_CAMERA_TOUCH_H
#define AB_ORACLE_CAMERA_TOUCH_H

static const SystemOriginal touch_originals[] = {
    {"touch-system-getScreenPosition", "getScreenPosition", 0},
    {"touch-system-getClosestCameraSnapPoint", "getClosestCameraSnapPoint", 0},
    {"touch-system-getSnapPointInDirection", "getSnapPointInDirection", 0},
    {"touch-system-getDistanceToSnapPoint", "getDistanceToSnapPoint", 0},
    {"touch-system-getRelativeZoomLevel", "getRelativeZoomLevel", 0},
    {"touch-system-snapZoomLevel", "snapZoomLevel", 0},
    {"touch-system-toggleBetweenCloseAndFarZoom", "toggleBetweenCloseAndFarZoom", 0},
    {"touch-system-init", "init", 2},
    {"touch-system-isInputEnabled", "isInputEnabled", 2},
    {"touch-system-setInputEnabled", "setInputEnabled", 2},
    {"touch-limits-getZoomLevelLimits", "getZoomLevelLimits", 1},
    {"touch-utils-length", "length", 3},
    {"touch-utils-vectorLength", "vectorLength", 3},
    {"touch-utils-vectorDistance", "vectorDistance", 3},
    {"touch-utils-vLength", "vLength", 3},
    {"touch-utils-vectorSubtract", "vectorSubtract", 3},
    {"touch-utils-vectorDot", "vectorDot", 3},
    {"touch-utils-tweenEaseCubicOut", "tweenEaseCubicOut", 3},
    {"touch-utils-easeCosineInOut", "easeCosineInOut", 3},
};

static uint32_t touch_flags, touch_payload[12], touch_conversion_count;
static uint32_t touch_camera[30], touch_coordinates[4];
static void (*touch_transform)(FloatBits[2], const void *, float, float);
static int (*touch_get_metatable)(void *, int);
static int (*touch_next)(void *, int);
static void (*touch_raw_get)(void *, int, int);
static unsigned (*touch_length)(void *, int);
static const char *(*touch_get_upvalue)(void *, int, int);
#if defined(AB_ORACLE_CAMERA_TOUCH_FILTERED)
#include "camera_touch_filter_probe.h"
#endif

/* Allocation/inheritance is a controlled oracle boundary. Original methods,
 * constructors, state updates and transition callbacks execute unmodified. */
static int touch_new(void *lua) {
    int arguments = damage_lua.get_top(lua);
    int object = arguments + 1;
    damage_lua.create_table(lua, 0, 20);
    system_push_value(lua, 1);
    (void)system_set_metatable(lua, object);
    damage_lua.get_field(lua, object, "init");
    if (damage_lua.type(lua, -1) == 6) {
        system_push_value(lua, object);
        for (int index = 2; index <= arguments; ++index) {
            system_push_value(lua, index);
        }
        system_call_checked(lua, arguments, 0);
    } else {
        damage_lua.set_top(lua, object);
    }
    return 1;
}

static int touch_class(void *lua) {
    int arguments = damage_lua.get_top(lua);
    int table = arguments + 1;
    damage_lua.create_table(lua, 0, 10);
    damage_function(lua, table, "new", touch_new);
    system_push_value(lua, table);
    damage_lua.set_field(lua, table, "__index");
    if (arguments && damage_lua.type(lua, 1) == 5) {
        damage_lua.create_table(lua, 0, 1);
        system_push_value(lua, 1);
        damage_lua.set_field(lua, table + 1, "__index");
        (void)system_set_metatable(lua, table);
    }
    return 1;
}

static void touch_methods(void *lua, int table, unsigned group) {
    for (size_t index = 0; index < sizeof(touch_originals) / sizeof(touch_originals[0]);
         ++index) {
        const SystemOriginal *original = touch_originals + index;
        if (original->group == group) {
            system_method(lua, table, original->method, original->name);
        }
    }
}

static int touch_filtered(void *lua) {
    damage_lua.get_field(lua, -10002, "_ab_touch_filtered");
    return 1;
}

static int touch_sling_instance(void *lua) {
    damage_lua.get_field(lua, -10002, "SlingshotSystem");
    return 1;
}

static int touch_bird_dragged(void *lua) {
    damage_lua.push_boolean(lua, (int)(touch_flags & 64));
    return 1;
}

static int touch_ingame(void *lua) {
    damage_lua.push_boolean(lua, (int)(touch_flags & 32));
    return 1;
}

static int touch_distance(void *lua) {
    for (size_t field = 0; field < 3; ++field) {
        touch_camera[26 + field] = system_native[field];
    }
    touch_camera[29] = system_native[2];
    FloatBits output[2];
    touch_transform(output, touch_coordinates, damage_lua.to_number(lua, 1),
                    damage_lua.to_number(lua, 2));
    damage_lua.push_number(lua, output[0].number);
    damage_lua.push_number(lua, output[1].number);
    ++touch_conversion_count;
    return 2;
}

static uint32_t touch_tween(void *lua, int index, int allow_end) {
    static const char *const names[] = {"utils-tweenEaseCubicInOut",
                                        "touch-utils-tweenEaseCubicOut", "",
                                        "touch-utils-easeCosineInOut"};
    int top = damage_lua.get_top(lua);
    const void *pointer = damage_lua.to_pointer(lua, index);
    for (uint32_t kind = 0; kind < 4; ++kind) {
        if (kind == 2) {
            continue;
        }
        damage_lua.get_field(lua, -10002, names[kind]);
        int equal = pointer == damage_lua.to_pointer(lua, -1);
        damage_lua.set_top(lua, top);
        if (equal) {
            return kind;
        }
    }
    if (!allow_end) {
        fail("Unknown original camera touch tween\n");
    }
    /* The original pinch closure is pure b+c. Two nonmutating calls identify
     * that known anonymous function after excluding named builtin identities. */
    for (unsigned sample = 0; sample < 2; ++sample) {
        system_push_value(lua, index);
        damage_lua.push_number(lua, sample ? 0.25f : 0);
        damage_lua.push_number(lua, 2);
        damage_lua.push_number(lua, 8);
        damage_lua.push_number(lua, 1);
        system_call_checked(lua, 4, 1);
        if (damage_lua.to_number(lua, -1) != 10) {
            fail("Unexpected original anonymous pinch tween\n");
        }
        damage_lua.set_top(lua, top);
    }
    return 2;
}

static int touch_observe_update(void *lua) {
    damage_lua.get_field(lua, -10002, "_ab_touch_original_update");
    system_push_value(lua, 1);
    system_push_value(lua, 2);
    system_call_checked(lua, 2, 2);
    for (size_t index = 0; index < 12; ++index) {
        touch_payload[index] = 0;
    }
    touch_payload[0] = (uint32_t)(damage_lua.type(lua, 3) == 5);
    touch_payload[11] = (uint32_t)damage_lua.to_boolean(lua, 4);
    if (touch_payload[0]) {
        const char *fields[] = {"x", "y", "zoom", "movementDuration", "zoomDuration"};
        for (size_t field = 0; field < 5; ++field) {
            damage_lua.get_field(lua, 3, fields[field]);
            if (damage_lua.type(lua, 5) == 3) {
                if (field < 3) {
                    touch_payload[1] |= 1u << field;
                }
                FloatBits value = {.number = damage_lua.to_number(lua, 5)};
                touch_payload[2 + field] = value.bits;
            }
            damage_lua.set_top(lua, 4);
        }
        const char *tweens[] = {"tweenFunction", "zoomTweenFunction"};
        for (size_t field = 0; field < 2; ++field) {
            damage_lua.get_field(lua, 3, tweens[field]);
            if (damage_lua.type(lua, 5) == 6) {
                touch_payload[7 + field * 2] = 1;
                touch_payload[8 + field * 2] = touch_tween(lua, 5, field == 1);
            }
            damage_lua.set_top(lua, 4);
        }
    }
    return 2;
}

static uint32_t touch_optional_number(void *lua, int table, const char *name) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, table, name);
    int type = damage_lua.type(lua, -1);
    if (type != 0 && type != 3) {
        fail("Unexpected touch numeric state\n");
    }
    FloatBits value = {.number = type == 3 ? damage_lua.to_number(lua, -1) : 0};
    damage_lua.set_top(lua, top);
    return value.bits;
}

static void touch_drag_snapshot(void *lua, int table, uint32_t output[21]) {
    int top = damage_lua.get_top(lua);
    const char *fields[] = {"startX",     "startY",         "anchorX",
                            "anchorY",    "currentScreenX", "currentScreenY",
                            "oldScreenX", "oldScreenY"};
    for (size_t field = 0; field < 8; ++field) {
        damage_lua.get_field(lua, table, fields[field]);
        if (damage_lua.type(lua, -1) == 3) {
            output[0] |= 1u << field;
            FloatBits value = {.number = damage_lua.to_number(lua, -1)};
            output[field + 1] = value.bits;
        }
        damage_lua.set_top(lua, top);
    }
    damage_lua.get_field(lua, table, "velocityBuffer");
    int buffer = top + 1;
    if (damage_lua.type(lua, buffer) == 5) {
        output[9] = touch_length(lua, buffer);
        if (output[9] > 5) {
            fail("Original touch velocity buffer overflow\n");
        }
        for (unsigned index = 0; index < output[9]; ++index) {
            touch_raw_get(lua, buffer, (int)index + 1);
            output[10 + index * 2] = touch_optional_number(lua, buffer + 1, "x");
            output[11 + index * 2] = touch_optional_number(lua, buffer + 1, "y");
            damage_lua.set_top(lua, buffer);
        }
        output[20] = touch_optional_number(lua, table, "velocityBufferSize");
    }
    damage_lua.set_top(lua, top);
}

static void touch_snapshot(void *lua) {
    uint32_t output[53] = {0};
    damage_lua.get_field(lua, -10002, "TouchInputIdleState");
    damage_lua.get_field(lua, 1, "init");
    if (!touch_get_upvalue(lua, 2, 1)) {
        fail("Cannot observe original shared tap counter\n");
    }
    FloatBits taps = {.number = damage_lua.to_number(lua, 3)};
    output[1] = taps.bits;
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "_ab_touch_handler");
    damage_lua.get_field(lua, 1, "touchStateMachine");
    damage_lua.get_field(lua, 2, "currentState");
    if (!touch_get_metatable(lua, 3)) {
        fail("Missing original touch state class\n");
    }
    const void *class_pointer = damage_lua.to_pointer(lua, 4);
    damage_lua.set_top(lua, 3);
    const char *classes[] = {"TouchInputIdleState", "TouchInputDragState",
                             "TouchInputZoomState"};
    int selected = -1;
    for (int kind = 0; kind < 3; ++kind) {
        damage_lua.get_field(lua, -10002, classes[kind]);
        if (class_pointer == damage_lua.to_pointer(lua, 4)) {
            selected = kind;
        }
        damage_lua.set_top(lua, 3);
    }
    if (selected < 0) {
        fail("Unknown original touch state class\n");
    }
    output[0] = (uint32_t)selected;
    output[2] =
        touch_optional_number(lua, 3, selected == 0 ? "tapResetTimer" : "tapTimer");
    if (selected == 1) {
        output[3] = (uint32_t)(int32_t)system_number(lua, 3, "touchId");
        damage_lua.get_field(lua, 3, "detectTap");
        output[5] = (uint32_t)damage_lua.to_boolean(lua, 4);
        damage_lua.set_top(lua, 3);
    } else if (selected == 2) {
        output[3] = (uint32_t)(int32_t)system_number(lua, 3, "firstTouchId");
        output[4] = (uint32_t)(int32_t)system_number(lua, 3, "secondTouchId");
        const char *fields[] = {"startZoom", "startDistance", "previousDistance",
                                "previousDiff", "zoomVelocity"};
        for (size_t field = 0; field < 5; ++field) {
            output[9 + field] = touch_optional_number(lua, 3, fields[field]);
        }
    }
    damage_lua.get_field(lua, 3, "latestTapPosition");
    if (damage_lua.type(lua, 4) == 5) {
        output[6] = 1;
        output[7] = touch_optional_number(lua, 4, "x");
        output[8] = touch_optional_number(lua, 4, "y");
    }
    damage_lua.set_top(lua, 3);
    damage_lua.get_field(lua, 3, "dragHandler");
    if (damage_lua.type(lua, 4) == 5) {
        touch_drag_snapshot(lua, 4, output + 14);
    }
    damage_lua.set_top(lua, 0);
    for (size_t field = 0; field < 12; ++field) {
        output[35 + field] = touch_payload[field];
    }
    damage_lua.get_field(lua, -10002, "_ab_system_mover");
    const char *tweens[] = {"movementTweenFunction", "zoomTweenFunction"};
    for (size_t field = 0; field < 2; ++field) {
        damage_lua.get_field(lua, 1, tweens[field]);
        output[47 + field] = touch_tween(lua, 2, 1);
        damage_lua.set_top(lua, 1);
    }
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "cameraSystem");
    damage_lua.get_field(lua, 1, "savedPositionForToggle");
    if (damage_lua.type(lua, 2) == 5) {
        output[49] = 1;
        output[50] = touch_optional_number(lua, 2, "x");
        output[51] = touch_optional_number(lua, 2, "y");
    }
    output[52] = touch_conversion_count;
    damage_lua.set_top(lua, 0);
    print_words(output, 53);
}

static void touch_initialize(void *lua) {
    int (*open_table)(void *) =
        (int (*)(void *))find_function("_Z13luaopen_tableP9lua_State");
    (void)open_table(lua);
    damage_lua.set_top(lua, 0);
    for (size_t index = 0; index < sizeof(touch_originals) / sizeof(touch_originals[0]);
         ++index) {
        char path[160] = "/data/local/tmp/ab520/camera/";
        size_t cursor = 0;
        while (path[cursor]) {
            ++cursor;
        }
        for (const char *name = touch_originals[index].name; *name; ++name) {
            path[cursor++] = *name;
        }
        const char suffix[] = "-oracle.lua";
        for (size_t byte = 0; byte < sizeof(suffix); ++byte) {
            path[cursor++] = suffix[byte];
        }
        load_selected_function(lua, path, touch_originals[index].name);
    }
    damage_lua.get_field(lua, -10002, "lua");
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 2, "create", touch_class);
    damage_lua.set_field(lua, 1, "Class");
    damage_lua.set_top(lua, 0);
    damage_lua.create_table(lua, 0, 5);
    touch_methods(lua, 1, 2);
    damage_function(lua, 1, "new", touch_new);
    system_push_value(lua, 1);
    damage_lua.set_field(lua, 1, "__index");
    damage_lua.set_field(lua, -10002, "CameraInputHandler");
    damage_function(lua, -10002, "requireFile", damage_require);
    load_damage_chunk(
        lua, "/data/local/tmp/ab520/camera/touch-FiniteStateMachine-oracle.lua");
    load_damage_chunk(lua, "/data/local/tmp/ab520/camera/touch-DragHandler-oracle.lua");
    load_damage_chunk(
        lua, "/data/local/tmp/ab520/camera/touch-TouchCameraInputHandler-oracle.lua");
    damage_lua.get_field(lua, -10002, "TouchCameraInputHandler");
    damage_lua.get_field(lua, 1, "update");
    damage_lua.set_field(lua, -10002, "_ab_touch_original_update");
    damage_lua.set_top(lua, 0);
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 1, "getFilteredTouches", touch_filtered);
    damage_lua.set_field(lua, -10002, "menuManager");
    damage_lua.create_table(lua, 0, 2);
    damage_function(lua, 1, "instance", touch_sling_instance);
    damage_function(lua, 1, "isBirdDragged", touch_bird_dragged);
    damage_lua.set_field(lua, -10002, "SlingshotSystem");
    damage_function(lua, -10002, "isIngame", touch_ingame);
    damage_function(lua, -10002, "screenToWorldDistance", touch_distance);
    touch_methods(lua, -10002, 3);
#if defined(AB_ORACLE_CAMERA_TOUCH_FILTERED)
    touch_filter_initialize(lua);
#endif
}

static void touch_reset(void *lua, const uint32_t setup[10]) {
    system_reset(lua, setup);
    damage_lua.get_field(lua, -10002, "cameraSystem");
    touch_methods(lua, 1, 0);
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "_ab_system_limits");
    touch_methods(lua, 1, 1);
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "gamelua");
    touch_methods(lua, 1, 3);
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "mover-setTweenFunctions");
    damage_lua.get_field(lua, -10002, "_ab_system_mover");
    damage_lua.push_nil(lua);
    damage_lua.push_nil(lua);
    system_call_checked(lua, 3, 0);
    damage_lua.get_field(lua, -10002, "TouchCameraInputHandler");
    damage_lua.get_field(lua, 1, "new");
    system_push_value(lua, 1);
    damage_lua.push_nil(lua);
    damage_lua.get_field(lua, -10002, "cameraSystem");
    system_call_checked(lua, 3, 1);
    damage_function(lua, 2, "update", touch_observe_update);
    system_push_value(lua, 2);
    damage_lua.set_field(lua, -10002, "_ab_touch_handler");
    damage_lua.get_field(lua, -10002, "cameraSystem");
    system_push_value(lua, 2);
    damage_lua.set_field(lua, 3, "cameraInputHandler");
    damage_lua.set_top(lua, 0);
    for (size_t index = 0; index < 12; ++index) {
        touch_payload[index] = 0;
    }
    touch_flags = touch_conversion_count = 0;
#if defined(AB_ORACLE_CAMERA_TOUCH_FILTERED)
    touch_filter_reset(lua);
#endif
}

static void touch_prepare(void *lua, uint32_t records[3][3], unsigned count) {
    damage_lua.create_table(lua, 0, (int)count);
    for (unsigned index = 0; index < count; ++index) {
        FloatBits id = {.bits = records[index][0]};
        damage_lua.push_number(lua, id.number);
        damage_lua.create_table(lua, 0, 2);
        damage_number(lua, 3, "x", records[index][1]);
        damage_number(lua, 3, "y", records[index][2]);
        /* Numeric IDs need not form a sequence; preserve actual native next order. */
        void (*raw_set)(void *, int) =
            (void (*)(void *, int))find_function("_Z10lua_rawsetP9lua_Statei");
        raw_set(lua, 1);
    }
    damage_lua.set_field(lua, -10002, "_ab_touch_filtered");
    damage_lua.get_field(lua, -10002, "_ab_touch_filtered");
    damage_lua.push_nil(lua);
    unsigned seen = 0;
    while (touch_next(lua, 1)) {
        if (seen >= count) {
            fail("Unexpected original filtered-touch count\n");
        }
        uint32_t ordered[3];
        FloatBits id = {.number = damage_lua.to_number(lua, 2)};
        ordered[0] = id.bits;
        ordered[1] = touch_optional_number(lua, 3, "x");
        ordered[2] = touch_optional_number(lua, 3, "y");
        print_words(ordered, 3);
        damage_lua.set_top(lua, 2);
        ++seen;
    }
    if (seen != count) {
        fail("Original filtered touches lost an ID\n");
    }
    damage_lua.set_top(lua, 0);
    damage_lua.push_boolean(lua, (int)(touch_flags & 16));
    damage_lua.set_field(lua, -10002, "g_touch_released_after_specialty");
    damage_lua.get_field(lua, -10002, "cameraSystem");
    damage_lua.push_boolean(lua, (int)(touch_flags & 256));
    damage_lua.set_field(lua, 1, "snapZoom");
    damage_lua.set_top(lua, 0);
    if (system_input[4]) {
        damage_lua.get_field(lua, -10002, "touch-system-setInputEnabled");
        damage_lua.get_field(lua, -10002, "_ab_touch_handler");
        damage_lua.push_boolean(lua, system_input[4] == 2);
        system_call_checked(lua, 2, 0);
    }
    touch_conversion_count = 0;
#if defined(AB_ORACLE_CAMERA_TOUCH_FILTERED)
    touch_filter_prepare(lua);
#endif
}

static void probe_camera_touch(void) {
    initialize_damage_lua();
    system_push_value =
        (void (*)(void *, int))find_function("_Z13lua_pushvalueP9lua_Statei");
    system_raw_set =
        (void (*)(void *, int, int))find_function("_Z11lua_rawsetiP9lua_Stateii");
    system_set_metatable =
        (int (*)(void *, int))find_function("_Z16lua_setmetatableP9lua_Statei");
    touch_get_metatable =
        (int (*)(void *, int))find_function("_Z16lua_getmetatableP9lua_Statei");
    touch_next = (int (*)(void *, int))find_function("_Z8lua_nextP9lua_Statei");
    touch_raw_get =
        (void (*)(void *, int, int))find_function("_Z11lua_rawgetiP9lua_Stateii");
    touch_length =
        (unsigned (*)(void *, int))find_function("_Z10lua_objlenP9lua_Statei");
    touch_get_upvalue = (const char *(*)(void *, int, int))find_function(
        "_Z14lua_getupvalueP9lua_Stateii");
    touch_transform = (void (*)(FloatBits[2], const void *, float, float))find_function(
        "_ZNK20CoordinateTransforms21screenToWorldDistanceEff");
    touch_coordinates[3] = (uint32_t)(uintptr_t)touch_camera;
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original touch oracle state\n");
    }
    system_initialize(lua);
    touch_initialize(lua);
    uint32_t random = UINT32_C(0x5207ac11);
#if defined(AB_ORACLE_CAMERA_TOUCH_FILTERED)
    semihost(4, "AB_ANDROID_CAMERA_TOUCH_FILTERED_V1 64 128\n");
#else
    semihost(4, "AB_ANDROID_CAMERA_TOUCH_V1 64 128\n");
#endif
    for (uint32_t flags = 0; flags < 8; ++flags) {
        touch_flags = flags & 1 ? 64u : 0u;
        damage_lua.push_boolean(lua, (int)(flags & 2));
        damage_lua.set_field(lua, -10002, "birdSpecialtyAvailable");
        damage_lua.push_boolean(lua, (int)(flags & 4));
        damage_lua.set_field(lua, -10002, "g_touch_released_after_specialty");
        damage_lua.get_field(lua, -10002, "_ab_touch_original_update");
        if (!touch_get_upvalue(lua, 1, 1)) {
            fail("Cannot inspect original touch gate\n");
        }
        system_call_checked(lua, 0, 1);
        uint32_t gate[2] = {flags, (uint32_t)damage_lua.to_boolean(lua, 2)};
        print_words(gate, 2);
        damage_lua.set_top(lua, 0);
    }
    for (size_t item = 0; item < 64; ++item) {
        uint32_t setup[10] = {UINT32_C(0x43f00000), UINT32_C(0x43a00000),
                              UINT32_C(0x43480000), UINT32_C(0x43480000),
                              UINT32_C(3253367420), UINT32_C(3281578908),
                              UINT32_C(1052074894), UINT32_C(1146044826),
                              UINT32_C(3281492696), UINT32_C(1052074893)};
        if (item) {
            FloatBits fields[10] = {{.number = 320.0f + (float)(item % 5) * 80.0f},
                                    {.number = 240.0f + (float)(item % 4) * 60.0f},
                                    {.number = 100.0f + (float)(item % 4) * 50.0f},
                                    {.number = 50.0f + (float)item * 3.0f},
                                    {.number = -250.0f + (float)item * 11.0f},
                                    {.number = -100.0f - (float)item * 9.0f},
                                    {.number = 0.25f + (float)(item % 7) * 0.125f},
                                    {.number = 700.0f + (float)item * 17.0f},
                                    {.number = -200.0f - (float)item * 3.0f},
                                    {.number = 0.375f + (float)(item % 5) * 0.125f}};
            for (size_t field = 0; field < 10; ++field) {
                setup[field] = fields[field].bits;
            }
        }
        print_words(setup, 10);
        touch_reset(lua, setup);
        for (size_t step = 0; step < 128; ++step) {
            for (size_t field = 0; field < 12; ++field) {
                system_input[field] = 0;
            }
            system_input[0] = 2;
            FloatBits seconds = {.number = 1.0f / 30.0f};
            if (step % 16 == 7) {
                seconds.number = 0.25f;
            }
            if (step % 16 == 15) {
                seconds.number = 0.03125f;
            }
            if (item % 4 == 3 && step % 16 == 10) {
                seconds.number = 0;
            }
            if (item % 10 == 9 && step % 16 == 14) {
                seconds.number = -0.03125f;
            }
            system_input[1] = seconds.bits;
            touch_flags = item % 2 ? 16u : 48u;
            if (item % 7 == 1 && step % 16 == 14) {
                touch_flags |= 64;
            }
            if (item % 7 == 2 && step % 16 >= 12) {
                touch_flags |= 4;
                if (step % 16 == 15) {
                    touch_flags |= 16;
                }
            }
            if (item % 7 == 3 && step % 16 == 9) {
                system_input[4] = 1;
            } else if (item % 7 == 3 && step % 16 == 11) {
                system_input[4] = 2;
            }
            if (item % 7 == 4) {
                touch_flags |= 256;
            }
            system_input[2] = touch_flags;
            unsigned phase = (unsigned)(step % 16);
            unsigned count =
                phase == 1 || phase == 2 || phase == 4 || phase == 11 || phase == 12
                    ? 1u
                : phase >= 8 && phase <= 10 ? 2u
                                            : 0u;
            if (item % 5 == 1 && phase == 2) {
                count = 3;
            }
#if defined(AB_ORACLE_CAMERA_TOUCH_FILTERED)
            system_input[5] = count ? 2u : 0u;
            if (phase == 1 || phase == 4 || phase == 8) {
                system_input[5] |= 1;
            }
            if (phase == 3 || phase == 5 || phase == 13) {
                system_input[5] |= 4;
            }
            if (phase == 14) {
                system_input[5] |= 8;
            }
            system_input[6] = !(item % 9 == 7 && phase == 9);
            system_input[7] = item % 9 == 8 && (phase == 2 || phase == 12) ? 2u : 0u;
#endif
            uint32_t touches[3][3] = {{0}};
            for (unsigned index = 0; index < count; ++index) {
                FloatBits id = {.number = (float)(index * 7 + 3)};
#if defined(AB_ORACLE_CAMERA_TOUCH_FILTERED)
                if (item % 3 == 1) {
                    id.number = (float)index;
                } else if (item % 3 == 2) {
                    id.number = index ? (float)(index * 6 - 5) : -3;
                }
#endif
                FloatBits x = {.number = 100.0f + (float)index * 140.0f};
                FloatBits y = {.number = 120.0f + (float)index * 50.0f};
                if (phase == 2) {
                    x.number += item % 3 ? 1.0f : 90.0f;
                }
                if (phase == 9) {
                    x.number += index ? 30.0f : -30.0f;
                }
                if (phase == 10) {
                    x.number += index ? 10.0f : -10.0f;
                }
                if (item % 8 == 5 && phase >= 8 && phase <= 10) {
                    x.number = 150;
                    y.number = 160;
                }
                if (item % 8 == 6 && phase == 11) {
                    id.number = 77;
                }
                if (step > 80) {
                    x.number += (float)(random_word(&random) % 33);
                    y.number += (float)(random_word(&random) % 17);
                }
                touches[index][0] = id.bits;
                touches[index][1] = x.bits;
                touches[index][2] = y.bits;
            }
            if (item >= 32 && step == 48) {
                system_input[0] = 1;
            }
            if (item % 16 == 0 && step == 80) {
                system_input[0] = 0;
            }
            system_input[3] = count;
            print_words(system_input, 12);
            touch_prepare(lua, touches, count);
            system_command(lua);
            touch_snapshot(lua);
        }
    }
    damage_lua.close_state(lua);
#if defined(AB_ORACLE_CAMERA_TOUCH_FILTERED)
    semihost(4, "AB camera touch filtered reference complete\n");
#else
    semihost(4, "AB camera touch reference complete\n");
#endif
}

#endif
