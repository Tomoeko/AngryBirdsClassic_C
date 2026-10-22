#ifndef AB_ORACLE_CAMERA_FOLLOW_END_SEQUENCE_H
#define AB_ORACLE_CAMERA_FOLLOW_END_SEQUENCE_H

static int owner_end_native_load(void *lua) {
    uint32_t resource = 1;
    const char *path = "/data/local/tmp/ab520/camera/follow-shot-end.lua";
    int valid_path = damage_named(
        lua, 1, "scripts/cameraSequences/FollowShotBirdEndCameraSequence.lua");
#if defined(AB_ORACLE_CAMERA_SHOT_SEQUENCE)
    if (damage_named(lua, 1,
                     "scripts/cameraSequences/FollowShotBirdCameraSequence.lua")) {
        resource = 2;
        path = "/data/local/tmp/ab520/camera/follow-shot-start.lua";
        valid_path = 1;
    }
#endif
    if (damage_lua.get_top(lua) != 3 || !valid_path || damage_lua.type(lua, 2) != 5 ||
        !damage_named(lua, 3, "")) {
        fail("Unexpected original end-camera resource request\n");
    }
    uint32_t *event = owner_native_event(lua, 4, 9);
    event[4] = resource;
    event[6] = 1;
#if defined(AB_ORACLE_CLASSIC_CAMERA_LIVE)
    damage_lua.get_field(lua, -10002, "flyingBird");
    if (damage_lua.type(lua, 4) != 5)
        fail("Missing current flying bird at original sequence load\n");
    event[8] = (uint32_t)owner_follow_number(owner_native_number(lua, 4, "_id"));
    damage_lua.get_field(lua, 4, "launchVelocity");
    event[7] = (uint32_t)damage_lua.type(lua, 5);
    if (event[7] != 0 && event[7] != 5)
        fail("Unexpected launch value at original sequence load\n");
    damage_lua.set_top(lua, 3);
#endif
    char bytes[1024];
    int file = system_call(5, (int)(uintptr_t)path, NULL, 0);
    int size = file < 0 ? -1 : system_call(3, file, bytes, sizeof(bytes));
    if (size <= 0 || size == sizeof(bytes) || system_call(6, file, NULL, 0) ||
        damage_lua.load(lua, bytes, (unsigned)size, "original-end-camera-sequence")) {
        fail("Cannot load original end-camera sequence\n");
    }
    void (*push_value)(void *, int) =
        (void (*)(void *, int))find_function("_Z13lua_pushvalueP9lua_Statei");
    int (*set_environment)(void *, int) =
        (int (*)(void *, int))find_function("_Z11lua_setfenvP9lua_Statei");
    push_value(lua, 2);
    if (!set_environment(lua, 4)) {
        fail("Cannot bind original sequence resource environment\n");
    }
    owner_native_checked(lua, 0, 0);
    damage_lua.get_field(lua, 2, "sequence");
    if (damage_lua.type(lua, -1) != 5) {
        fail("Missing original loaded end-camera sequence\n");
    }
    damage_lua.set_field(lua, -10002, "_ab_owner_loaded_sequence");
    return 0;
}

static int owner_end_native_print(void *lua) {
    if (damage_lua.get_top(lua) != 1 || damage_lua.type(lua, 1) != 4) {
        fail("Unexpected original camera sequence print arguments\n");
    }
    uint32_t *event = owner_native_event(lua, 7, UINT32_MAX);
    event[4] = event[3] == 19 ? 1u : 2u;
    const char *message = event[4] == 1
                              ? "<CameraSequenceManager> loading sequence from file: "
                                "FollowShotBirdEndCameraSequence"
                              : "<CameraSequenceManager> starting new camera sequence";
#if defined(AB_ORACLE_CAMERA_SHOT_SEQUENCE)
    if (event[4] == 1 &&
        damage_named(lua, 1,
                     "<CameraSequenceManager> loading sequence from file: "
                     "FollowShotBirdCameraSequence")) {
        event[4] = 3;
        return 0;
    }
#endif
    if (!damage_named(lua, 1, message)) {
        fail("Unexpected original camera sequence print text\n");
    }
    return 0;
}

static uint32_t owner_end_native_condition(void *lua, int stage) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, stage, "condition");
    int type = damage_lua.type(lua, -1);
    uint32_t kind = 0;
    if (type == 4 && damage_named(lua, -1, "noMovingObjects")) {
        kind = 3;
    } else if (type == 6) {
        const uint8_t *closure = damage_lua.to_pointer(lua, -1);
        const uint32_t *prototype =
            (const uint32_t *)(uintptr_t)((const uint32_t *)closure)[4];
        if (closure[6] || !prototype) {
            fail("Invalid original end-camera condition closure\n");
        }
        uint32_t size = prototype[11];
        kind = size == 3 ? 1u : size == 9 ? 6u : size == 10 ? 2u : size == 15 ? 3u : 0;
        if (!kind || closure[7] != (size == 3 ? 0u : 2u)) {
            fail("Unknown original end-camera condition closure\n");
        }
    } else if (type != 0) {
        fail("Unexpected original end-camera condition type\n");
    }
    damage_lua.set_top(lua, top);
    return kind;
}

static void owner_end_native_snapshot(void *lua, uint32_t output[80]) {
    damage_lua.get_field(lua, -10002, "cameraSystem");
    damage_lua.get_field(lua, 1, "cameraSequenceManager");
    damage_lua.get_field(lua, 2, "currentSequence");
    output[64] = damage_lua.type(lua, 3) != 0;
    damage_lua.set_top(lua, 2);
    output[65] = owner_native_number(lua, 2, "sequenceStageIndex");
    output[66] = owner_native_number(lua, 2, "timer");
    output[68] = owner_native_optional(lua, 2, "stageTimer", output + 67);
    damage_lua.get_field(lua, 2, "doneCallback");
    output[69] = damage_lua.to_boolean(lua, 3) != 0;
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "_ab_owner_loaded_sequence");
    if (damage_lua.type(lua, 1) == 5) {
        void (*raw_get)(void *, int, int) =
            (void (*)(void *, int, int))find_function("_Z11lua_rawgetiP9lua_Stateii");
        unsigned (*length)(void *, int) =
            (unsigned (*)(void *, int))find_function("_Z10lua_objlenP9lua_Statei");
        unsigned count = length(lua, 1);
        if (count != 1 && count != 3)
            fail("Unexpected original loaded camera sequence length\n");
#if defined(AB_ORACLE_CAMERA_SHOT_SEQUENCE)
        output[62] = count;
#endif
        for (int index = 0; index < (int)count; ++index) {
            raw_get(lua, 1, index + 1);
            uint32_t first = 70u + (uint32_t)index * 3u;
            output[first + 1] =
                owner_native_optional(lua, 2, "duration", output + first);
            output[first + 2] = owner_end_native_condition(lua, 2);
            damage_lua.set_top(lua, 1);
        }
    } else if (damage_lua.type(lua, 1) != 0) {
        fail("Invalid retained original end-camera sequence\n");
    }
    damage_lua.set_top(lua, 0);
    output[79] = owner_native_counts[7];
}

#if defined(AB_ORACLE_CAMERA_SHOT_SEQUENCE)
static int owner_shot_native_object(void *lua) {
    if (damage_lua.get_top(lua) != 1 || !damage_named(lua, 1, "flyingBird"))
        fail("Unexpected original camera follow-target lookup\n");
    uint32_t *event = owner_native_event(lua, 8, 10);
    event[6] = 1;
#if defined(AB_ORACLE_CAMERA_FOLLOW_PROPERTIES)
    void (*push_value)(void *, int) =
        (void (*)(void *, int))find_function("_Z13lua_pushvalueP9lua_Statei");
    damage_lua.get_field(lua, -10002, "_ab_level_object_lookup");
    push_value(lua, 1);
    owner_native_checked(lua, 1, 1);
#else
    damage_lua.push_nil(lua);
#endif
    return 1;
}
#endif

#endif
