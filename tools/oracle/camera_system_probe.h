#ifndef AB_ORACLE_CAMERA_SYSTEM_H
#define AB_ORACLE_CAMERA_SYSTEM_H

typedef struct {
    const char *name;
    const char *method;
    unsigned group;
} SystemOriginal;

static const SystemOriginal system_originals[] = {
    {"system-lockCameraLimits", "lockCameraLimits", 0},
    {"system-unlockCameraLimits", "unlockCameraLimits", 0},
    {"system-getScreenX", "getScreenX", 0},
    {"system-getScreenY", "getScreenY", 0},
    {"system-getScreenLeft", "getScreenLeft", 0},
    {"system-getScreenTop", "getScreenTop", 0},
    {"system-getScreenExtentsForPositionAndZoom", "getScreenExtentsForPositionAndZoom",
     0},
    {"system-getScreenLeftForRendering", "getScreenLeftForRendering", 0},
    {"system-getScreenTopForRendering", "getScreenTopForRendering", 0},
    {"system-getZoomScale", "getZoomScale", 0},
    {"system-gotoTargetPosition", "gotoTargetPosition", 0},
    {"system-gotoSnapPoint", "gotoSnapPoint", 0},
    {"system-updateCamera", "updateCamera", 0},
    {"system-getSnapPoint", "getSnapPoint", 0},
    {"system-setCameraInputEnabled", "setCameraInputEnabled", 0},
    {"system-getClosestValidCameraValues", "getClosestValidCameraValues", 0},
    {"mover-reset", "reset", 1},
    {"mover-update", "update", 1},
    {"mover-updatePosition", "updatePosition", 1},
    {"mover-updateZoom", "updateZoom", 1},
    {"mover-setTargetValues", "setTargetValues", 1},
    {"mover-getZoomScale", "getZoomScale", 1},
    {"mover-setTargetZoomScale", "setTargetZoomScale", 1},
    {"mover-setTargetPosition", "setTargetPosition", 1},
    {"mover-setTweenFunctions", "setTweenFunctions", 1},
    {"limits-initializeLimits", "initializeLimits", 2},
    {"limits-lock", "lock", 2},
    {"limits-unlock", "unlock", 2},
    {"limits-enforceUserInputValues", "enforceUserInputValues", 2},
    {"limits-enforceUserInputZoom", "enforceUserInputZoom", 2},
    {"limits-enforceUserInputPosition", "enforceUserInputPosition", 2},
    {"limits-projectWantedValuesDown", "projectWantedValuesDown", 2},
    {"limits-getClosestValidValues", "getClosestValidValues", 2},
    {"limits-checkPosition", "checkPosition", 2},
    {"limits-checkZoom", "checkZoom", 2},
    {"limits-checkForBrokenConstraints", "checkForBrokenConstraints", 2},
    {"sequence-reset", "reset", 3},
    {"sequence-startSequence", "startSequence", 3},
    {"sequence-update", "update", 3},
    {"sequence-moveToNextStage", "moveToNextStage", 3},
    {"sequence-setCondition", "setCondition", 3},
    {"sequence-delegateSequenceAction", "delegateSequenceAction", 3},
    {"sequence-endSequence", "endSequence", 3},
    {"automatic-baseInit", "baseInit", 4},
    {"automatic-setAllowCancel", "setAllowCancel", 4},
    {"automatic-init", "init", 4},
    {"automatic-update", "update", 4},
    {"utils-tweenEaseCubicInOut", "tweenEaseCubicInOut", 5},
    {"utils-clamp", "clamp", 5},
    {"utils-vectorProjection", "vectorProjection", 5},
    {"utils-vectorLengthSquared", "vectorLengthSquared", 5},
    {"utils-vLengthsq", "vLengthsq", 5},
    {"default-source", "source", 6},
    {"restart-source", "source", 7},
};
static void (*system_push_value)(void *, int);
static void (*system_raw_set)(void *, int, int);
static int (*system_set_metatable)(void *, int);
static uint32_t system_input[12], system_native[3], system_publications;
static uint32_t system_done_count, system_done_state[6];
static int system_input_enabled;

static void system_call_checked(void *lua, int arguments, int results) {
    if (damage_lua.call(lua, arguments, results, 0)) {
        const char *message = damage_lua.to_string(lua, -1, NULL);
        fail(message ? message : "Original camera graph call failed\n");
    }
}

static void system_method(void *lua, int table, const char *field, const char *name) {
    damage_lua.get_field(lua, -10002, name);
    if (damage_lua.type(lua, -1) != 6) {
        fail("Missing original camera graph method\n");
    }
    damage_lua.set_field(lua, table, field);
}

static void system_methods(void *lua, int table, unsigned group) {
    for (size_t index = 0;
         index < sizeof(system_originals) / sizeof(system_originals[0]); ++index) {
        const SystemOriginal *original = system_originals + index;
        if (original->group == group) {
            system_method(lua, table, original->method, original->name);
        }
    }
}

static float system_number(void *lua, int table, const char *name) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, table, name);
    if (damage_lua.type(lua, -1) != 3) {
        fail("Missing original camera graph number\n");
    }
    float result = damage_lua.to_number(lua, -1);
    damage_lua.set_top(lua, top);
    return result;
}

static void system_vector(void *lua, float x, float y) {
    int table = damage_lua.get_top(lua) + 1;
    damage_lua.create_table(lua, 0, 2);
    damage_lua.push_number(lua, x);
    damage_lua.set_field(lua, table, "x");
    damage_lua.push_number(lua, y);
    damage_lua.set_field(lua, table, "y");
    damage_lua.get_field(lua, -10002, "_ab_system_vector_meta");
    (void)system_set_metatable(lua, table);
}

/* Vector construction and multiplication by one are controlled boundaries.
 * These do not substitute camera movement, limit or tween arithmetic. */
static int system_vector_new(void *lua) {
    int count = damage_lua.get_top(lua);
    if ((count != 1 && count != 3) || damage_lua.type(lua, 1) != 5) {
        fail("Unexpected controlled camera graph vector constructor\n");
    }
    system_vector(lua, count == 3 ? damage_lua.to_number(lua, 2) : 0,
                  count == 3 ? damage_lua.to_number(lua, 3) : 0);
    return 1;
}

static int system_vector_multiply(void *lua) {
    if (damage_lua.get_top(lua) != 2 || damage_lua.type(lua, 1) != 5 ||
        damage_lua.type(lua, 2) != 3 || damage_lua.to_number(lua, 2) != 1) {
        fail("Unexpected controlled camera graph vector identity\n");
    }
    system_vector(lua, system_number(lua, 1, "x"), system_number(lua, 1, "y"));
    return 1;
}

/* All copied oracle tables are flat numeric camera values/snap records. Reject
 * nested data rather than pretending this boundary implements deepCopy. */
static int system_copy(void *lua) {
#if defined(AB_ORACLE_CAMERA_TOUCH_ONLY)
    if (damage_lua.get_top(lua) == 1 && damage_lua.type(lua, 1) == 0) {
        damage_lua.push_nil(lua);
        return 1;
    }
#endif
    static const char *const fields[] = {"x",
                                         "y",
                                         "zoom",
                                         "movementDuration",
                                         "zoomDuration",
                                         "tweenFunction",
                                         "zoomTweenFunction"};
    if (damage_lua.get_top(lua) != 1 || damage_lua.type(lua, 1) != 5) {
        fail("Unexpected controlled camera graph flat copy\n");
    }
    damage_lua.create_table(lua, 0, 7);
    for (size_t index = 0; index < sizeof(fields) / sizeof(fields[0]); ++index) {
        damage_lua.get_field(lua, 1, fields[index]);
        if (damage_lua.type(lua, -1) == 5) {
            fail("Nested data at controlled flat camera copy\n");
        }
        damage_lua.set_field(lua, 2, fields[index]);
    }
    return 1;
}

static int system_default(void *lua) {
    if (damage_lua.get_top(lua) != 2) {
        fail("Unexpected controlled camera graph default boundary\n");
    }
    system_push_value(lua, damage_lua.type(lua, 1) == 0 ? 2 : 1);
    return 1;
}

static int system_new_behavior(void *lua) {
    if (damage_lua.get_top(lua) != 2 || damage_lua.type(lua, 2) != 5) {
        fail("Unexpected controlled camera graph behavior construction\n");
    }
    damage_lua.get_field(lua, -10002, "automatic-init");
    damage_lua.create_table(lua, 0, 12);
    system_methods(lua, 4, 4);
    system_push_value(lua, 4);
    damage_lua.set_field(lua, -10002, "_ab_system_new_behavior");
    system_push_value(lua, 2);
    system_call_checked(lua, 2, 0);
    damage_lua.get_field(lua, -10002, "_ab_system_new_behavior");
    return 1;
}

static int system_input_update(void *lua) {
    if (system_input[2] & 8) {
        damage_lua.create_table(lua, 0, 5);
        const char *fields[] = {"x", "y", "zoom"};
        int table = damage_lua.get_top(lua);
        for (size_t field = 0; field < 3; ++field) {
            if (system_input[3] & (1u << field)) {
                damage_number(lua, table, fields[field], system_input[4 + field]);
            }
        }
        damage_number(lua, table, "movementDuration", system_input[7]);
        damage_number(lua, table, "zoomDuration", system_input[8]);
    } else {
        damage_lua.push_nil(lua);
    }
    damage_lua.push_boolean(lua, (int)(system_input[2] & 1));
    return 2;
}

static int system_input_enable(void *lua) {
    if (damage_lua.get_top(lua) != 2 || damage_lua.type(lua, 2) != 1) {
        fail("Unexpected controlled camera input enable boundary\n");
    }
    system_input_enabled = damage_lua.to_boolean(lua, 2);
    return 0;
}

static int system_native_position(void *lua) {
    if (damage_lua.get_top(lua) != 2 || damage_lua.type(lua, 1) != 3 ||
        damage_lua.type(lua, 2) != 3) {
        fail("Unexpected original camera native position boundary\n");
    }
    for (size_t axis = 0; axis < 2; ++axis) {
        FloatBits value = {.number = damage_lua.to_number(lua, (int)axis + 1)};
        system_native[axis] = value.bits;
    }
    return 0;
}

static int system_native_zoom(void *lua) {
    if (damage_lua.get_top(lua) != 1 || damage_lua.type(lua, 1) != 3) {
        fail("Unexpected original camera native zoom boundary\n");
    }
    FloatBits value = {.number = damage_lua.to_number(lua, 1)};
    system_native[2] = value.bits;
    ++system_publications;
    return 0;
}

static int system_no_print(void *lua) {
    (void)lua;
    return 0;
}

static void system_sequence_snapshot(void *lua, uint32_t output[6]) {
    int top = damage_lua.get_top(lua), table = top + 1;
    damage_lua.get_field(lua, -10002, "_ab_system_sequence");
    const char *fields[] = {"currentSequence", "sequenceStageIndex", "timer",
                            "stageTimer", "doneCallback"};
    for (size_t field = 0; field < 5; ++field) {
        damage_lua.get_field(lua, table, fields[field]);
        int type = damage_lua.type(lua, -1);
        if (field == 0 || field == 4) {
            output[field == 0 ? 0 : 5] = (uint32_t)(type != 0);
        } else if (field == 3) {
            output[3] = (uint32_t)type;
            FloatBits value = {.number = type == 3 ? damage_lua.to_number(lua, -1) : 0};
            output[4] = value.bits;
        } else {
            FloatBits value = {.number = damage_lua.to_number(lua, -1)};
            output[field] = value.bits;
        }
        damage_lua.set_top(lua, table);
    }
    damage_lua.set_top(lua, top);
}

static int system_done(void *lua) {
    ++system_done_count;
    system_sequence_snapshot(lua, system_done_state);
    return 0;
}

static void system_bind_vector(void *lua, const char *name) {
    const char *(*setup_value)(void *, int, int) =
        (const char *(*)(void *, int, int))find_function(
            "_Z14lua_setupvalueP9lua_Stateii");
    damage_lua.get_field(lua, -10002, name);
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 2, "new", system_vector_new);
    if (!setup_value(lua, 1, 1)) {
        fail("Cannot bind original camera graph vector upvalue\n");
    }
    damage_lua.set_top(lua, 0);
}

static void system_initialize(void *lua) {
    (void)damage_lua.open_base(lua);
    (void)damage_lua.open_math(lua);
    damage_lua.set_top(lua, 0);
    for (size_t index = 0;
         index < sizeof(system_originals) / sizeof(system_originals[0]); ++index) {
        char path[160] = "/data/local/tmp/ab520/camera/system-";
        size_t cursor = 0;
        while (path[cursor]) {
            ++cursor;
        }
        for (const char *name = system_originals[index].name; *name; ++name) {
            path[cursor++] = *name;
        }
        const char suffix[] = "-oracle.lua";
        for (size_t byte = 0; byte < sizeof(suffix); ++byte) {
            path[cursor++] = suffix[byte];
        }
        load_selected_function(lua, path, system_originals[index].name);
    }
    damage_function(lua, -10002, "print", system_no_print);
    damage_function(lua, -10002, "deepCopy", system_copy);
    system_method(lua, -10002, "vLengthsq", "utils-vLengthsq");
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 1, "__mul", system_vector_multiply);
    damage_lua.set_field(lua, -10002, "_ab_system_vector_meta");
    system_bind_vector(lua, "mover-reset");
    system_bind_vector(lua, "mover-setTargetPosition");
    damage_lua.create_table(lua, 0, 2);
    damage_function(lua, 1, "default", system_default);
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 2, "new", system_vector_new);
    damage_lua.set_field(lua, 1, "Vector2");
    damage_lua.set_field(lua, -10002, "lua");
    damage_lua.create_table(lua, 0, 1);
    system_method(lua, 1, "init", "automatic-baseInit");
    damage_lua.set_field(lua, -10002, "AutomaticCameraMovementBehavior");
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 1, "new", system_new_behavior);
    damage_lua.set_field(lua, -10002, "GoToTargetCameraMovementBehavior");
    damage_lua.create_table(lua, 0, 2);
    damage_function(lua, 1, "setCameraTopLeft", system_native_position);
    damage_function(lua, 1, "setCameraZoomScale", system_native_zoom);
    damage_lua.set_field(lua, -10002, "CameraNative");
}

static void system_reset(void *lua, const uint32_t setup[10]) {
    system_publications = system_done_count = 0;
    system_input_enabled = 1;
    for (size_t field = 0; field < 3; ++field) {
        system_native[field] = 0;
    }
    for (size_t field = 0; field < 6; ++field) {
        system_done_state[field] = 0;
    }
    damage_number(lua, -10002, "screenWidth", setup[0]);
    damage_number(lua, -10002, "screenHeight", setup[1]);
    damage_lua.create_table(lua, 0, 10);
    system_methods(lua, 1, 5);
    damage_function(lua, 1, "deepCopy", system_copy);
    damage_number(lua, 1, "screenWidth", setup[0]);
    damage_number(lua, 1, "screenHeight", setup[1]);
    damage_lua.set_field(lua, -10002, "gamelua");
    damage_lua.create_table(lua, 0, 24);
    system_methods(lua, 1, 1);
    damage_lua.set_field(lua, -10002, "_ab_system_mover");
    damage_lua.get_field(lua, -10002, "mover-reset");
    damage_lua.get_field(lua, -10002, "_ab_system_mover");
    system_call_checked(lua, 1, 0);
    damage_lua.create_table(lua, 0, 12);
    system_methods(lua, 1, 3);
    damage_lua.set_field(lua, -10002, "_ab_system_sequence");
    damage_lua.get_field(lua, -10002, "sequence-reset");
    damage_lua.get_field(lua, -10002, "_ab_system_sequence");
    system_call_checked(lua, 1, 0);
    damage_lua.create_table(lua, 2, 0);
    for (size_t index = 0; index < 2; ++index) {
        damage_lua.create_table(lua, 0, 3);
        const char *fields[] = {"x", "y", "zoom"};
        for (size_t field = 0; field < 3; ++field) {
            damage_number(lua, 2, fields[field], setup[4 + index * 3 + field]);
        }
        system_raw_set(lua, 1, (int)index + 1);
    }
    damage_lua.set_field(lua, -10002, "_ab_system_snaps");
    damage_lua.create_table(lua, 0, 24);
    system_methods(lua, 1, 0);
    damage_lua.get_field(lua, -10002, "_ab_system_snaps");
    damage_lua.set_field(lua, 1, "snapPoints");
    damage_lua.get_field(lua, -10002, "_ab_system_mover");
    damage_lua.set_field(lua, 1, "cameraMover");
    damage_lua.get_field(lua, -10002, "_ab_system_sequence");
    damage_lua.set_field(lua, 1, "cameraSequenceManager");
    damage_lua.create_table(lua, 0, 2);
    damage_number(lua, 2, "x", 0);
    damage_number(lua, 2, "y", 0);
    damage_lua.set_field(lua, 1, "screen");
    damage_lua.create_table(lua, 0, 2);
    damage_function(lua, 2, "update", system_input_update);
    damage_function(lua, 2, "setInputEnabled", system_input_enable);
    damage_lua.set_field(lua, 1, "cameraInputHandler");
    system_push_value(lua, 1);
    damage_lua.set_field(lua, -10002, "cameraSystem");
    damage_lua.get_field(lua, -10002, "gamelua");
    system_push_value(lua, 1);
    damage_lua.set_field(lua, 2, "cameraSystem");
    damage_lua.set_top(lua, 0);
    damage_lua.create_table(lua, 0, 24);
    system_methods(lua, 1, 2);
    damage_lua.get_field(lua, -10002, "cameraSystem");
    damage_lua.set_field(lua, 1, "cameraSystem");
    damage_lua.get_field(lua, -10002, "_ab_system_snaps");
    damage_lua.set_field(lua, 1, "snapPoints");
    damage_number(lua, 1, "absoluteBottom", setup[2]);
    damage_number(lua, 1, "maxHorizontalLeeway", setup[3]);
    damage_lua.push_boolean(lua, 1);
    damage_lua.set_field(lua, 1, "enabled");
    system_push_value(lua, 1);
    damage_lua.set_field(lua, -10002, "_ab_system_limits");
    damage_lua.get_field(lua, -10002, "cameraSystem");
    system_push_value(lua, 1);
    damage_lua.set_field(lua, 2, "cameraLimitEnforcer");
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "limits-initializeLimits");
    damage_lua.get_field(lua, -10002, "_ab_system_limits");
    system_call_checked(lua, 1, 0);
}

static void system_mover_snapshot(void *lua, uint32_t output[16]) {
    damage_lua.get_field(lua, -10002, "_ab_system_mover");
    const char *vectors[] = {"currentPosition", "targetPosition", "startPosition"};
    for (size_t index = 0; index < 3; ++index) {
        damage_lua.get_field(lua, 1, vectors[index]);
        if (damage_lua.type(lua, 2) == 5) {
            output[0] |= index == 1 ? 1u : index == 2 ? 4u : 0;
            FloatBits x = {.number = system_number(lua, 2, "x")};
            FloatBits y = {.number = system_number(lua, 2, "y")};
            output[1 + index * 2] = x.bits;
            output[2 + index * 2] = y.bits;
        }
        damage_lua.set_top(lua, 1);
    }
    const char *scalars[] = {
        "currentZoomScale", "targetZoomScale",   "currentZoomVelocity",
        "startZoomScale",   "startZoom",         "movementTweenDuration",
        "movementTimer",    "zoomTweenDuration", "zoomTimer"};
    for (size_t index = 0; index < 9; ++index) {
        damage_lua.get_field(lua, 1, scalars[index]);
        if (damage_lua.type(lua, 2) == 3) {
            output[0] |= index == 1 ? 2u : index == 3 ? 8u : index == 4 ? 16u : 0;
            FloatBits value = {.number = damage_lua.to_number(lua, 2)};
            output[7 + index] = value.bits;
        }
        damage_lua.set_top(lua, 1);
    }
    damage_lua.set_top(lua, 0);
}

static void system_snapshot(void *lua) {
    uint32_t output[46] = {0};
    system_mover_snapshot(lua, output);
    system_sequence_snapshot(lua, output + 16);
    damage_lua.get_field(lua, -10002, "_ab_system_limits");
    damage_lua.get_field(lua, 1, "enabled");
    output[22] = (uint32_t)damage_lua.to_boolean(lua, 2);
    damage_lua.set_top(lua, 1);
    const char *fields[] = {"leftBorder",     "rightBorder",        "topBorder",
                            "bottomBorder",   "closeZoomLevel",     "farZoomLevel",
                            "absoluteBottom", "maxHorizontalLeeway"};
    for (size_t field = 0; field < 8; ++field) {
        FloatBits value = {.number = system_number(lua, 1, fields[field])};
        output[29 + field] = value.bits;
    }
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "cameraSystem");
    damage_lua.get_field(lua, 1, "automaticBehavior");
    output[23] = (uint32_t)(damage_lua.type(lua, 2) != 0);
    damage_lua.set_top(lua, 1);
    damage_lua.get_field(lua, 1, "screen");
    FloatBits x = {.number = system_number(lua, 2, "x")};
    FloatBits y = {.number = system_number(lua, 2, "y")};
    output[44] = x.bits;
    output[45] = y.bits;
    damage_lua.set_top(lua, 0);
#if defined(AB_ORACLE_CAMERA_TOUCH_ONLY)
    damage_lua.get_field(lua, -10002, "_ab_touch_handler");
    damage_lua.get_field(lua, 1, "inputEnabled");
    system_input_enabled = damage_lua.to_boolean(lua, 2);
    damage_lua.set_top(lua, 0);
#endif
    output[24] = (uint32_t)system_input_enabled;
    for (size_t field = 0; field < 3; ++field) {
        output[25 + field] = system_native[field];
    }
    output[28] = system_publications;
    output[37] = system_done_count;
    for (size_t field = 0; field < 6; ++field) {
        output[38 + field] = system_done_state[field];
    }
    print_words(output, 46);
}

static void system_command(void *lua) {
    damage_lua.push_boolean(lua, (int)(system_input[2] & 2));
    damage_lua.set_field(lua, -10002, "hasMovingObjects");
    damage_lua.push_boolean(lua, (int)(system_input[2] & 4));
    damage_lua.set_field(lua, -10002, "birdSpecialtyAvailable");
    damage_number(lua, -10002, "cameraShakeX", system_input[9]);
    damage_number(lua, -10002, "cameraShakeY", system_input[10]);
    if (system_input[0] < 2) {
        damage_lua.get_field(lua, -10002,
                             system_input[0] ? "restart-source" : "default-source");
        system_call_checked(lua, 0, 0);
        damage_lua.get_field(lua, -10002, "sequence-startSequence");
        damage_lua.get_field(lua, -10002, "_ab_system_sequence");
        damage_lua.get_field(lua, -10002, "sequence");
        damage_lua.push_nil(lua);
        if (system_input[11]) {
            damage_lua.push_closure(lua, system_done, 0);
        } else {
            damage_lua.push_nil(lua);
        }
        system_call_checked(lua, 4, 0);
    } else {
        damage_lua.get_field(lua, -10002, "system-updateCamera");
        damage_lua.get_field(lua, -10002, "cameraSystem");
        FloatBits seconds = {.bits = system_input[1]};
        damage_lua.push_number(lua, seconds.number);
        system_call_checked(lua, 2, 0);
    }
    damage_lua.set_top(lua, 0);
    system_snapshot(lua);
}

#if !defined(AB_ORACLE_CAMERA_TOUCH_ONLY)
static void probe_camera_system(void) {
    initialize_damage_lua();
    system_push_value =
        (void (*)(void *, int))find_function("_Z13lua_pushvalueP9lua_Statei");
    system_raw_set =
        (void (*)(void *, int, int))find_function("_Z11lua_rawsetiP9lua_Stateii");
    system_set_metatable =
        (int (*)(void *, int))find_function("_Z16lua_setmetatableP9lua_Statei");
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original integrated camera graph state\n");
    }
    system_initialize(lua);
    uint32_t random = UINT32_C(0x520ca5e1);
    semihost(4, "AB_ANDROID_CAMERA_SYSTEM_V1 64 128\n");
    for (size_t item = 0; item < 64; ++item) {
        uint32_t setup[10] = {UINT32_C(0x43f00000), UINT32_C(0x43a00000),
                              UINT32_C(0x43480000), UINT32_C(0x43480000),
                              UINT32_C(3253367420), UINT32_C(3281578908),
                              UINT32_C(1052074894), UINT32_C(1146044826),
                              UINT32_C(3281492696), UINT32_C(1052074893)};
        if (item) {
            FloatBits x = {.number = (float)((int)item * 11 - 500)};
            FloatBits y = {.number = -(float)(item * 7)};
            FloatBits zoom = {.number = 0.25f + (float)(item % 7) * 0.125f};
            setup[4] = x.bits;
            setup[5] = y.bits;
            setup[6] = zoom.bits;
            x.number += (float)(700 + item * 13);
            y.number -= (float)(item % 5) * 20;
            zoom.number += 0.125f;
            setup[7] = x.bits;
            setup[8] = y.bits;
            setup[9] = zoom.bits;
        }
        print_words(setup, 10);
        system_reset(lua, setup);
        for (size_t step = 0; step < 128; ++step) {
            FloatBits seconds = {.number = 0x1.111112p-5f};
            FloatBits x = {.number =
                               (float)((int)(random_word(&random) % 4000) - 2000)};
            FloatBits y = {.number = -(float)(random_word(&random) % 1000)};
            FloatBits zoom = {.number =
                                  0.125f + (float)(random_word(&random) % 8) * 0.25f};
            FloatBits duration = {.number = step % 2 ? 0.125f : 0};
            FloatBits shake_x = {.number = (float)((int)(step % 7) - 3) * 0.25f};
            FloatBits shake_y = {.number = (float)((int)(step % 5) - 2) * 0.25f};
            uint32_t flags = step > 118 || step == 1 || step == 113 ? 8u : 0;
            if (item % 3 == 1 && step == 20) {
                flags |= 1;
            }
            if (item % 3 == 2 && step == 83) {
                flags |= 1;
            }
            if (step % 11 == 0) {
                seconds.number = 0;
            }
            system_input[0] = step == 0 ? 0u : step == 112 ? 1u : 3u;
            system_input[1] = seconds.bits;
            system_input[2] = flags;
            system_input[3] = (uint32_t)(step % 8);
            system_input[4] = x.bits;
            system_input[5] = y.bits;
            system_input[6] = zoom.bits;
            system_input[7] = duration.bits;
            system_input[8] = duration.bits;
            system_input[9] = shake_x.bits;
            system_input[10] = shake_y.bits;
            system_input[11] = (uint32_t)(item % 2);
            print_words(system_input, 12);
            system_command(lua);
        }
    }
    damage_lua.close_state(lua);
    semihost(4, "AB camera system reference complete\n");
}

#endif
#endif
