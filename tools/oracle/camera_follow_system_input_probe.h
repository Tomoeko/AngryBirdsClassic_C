#ifndef AB_ORACLE_CAMERA_FOLLOW_SYSTEM_INPUT_H
#define AB_ORACLE_CAMERA_FOLLOW_SYSTEM_INPUT_H

static void owner_native_prepare(void *lua) {
    void (*raw_set)(void *, int, int) =
        (void (*)(void *, int, int))find_function("_Z11lua_rawsetiP9lua_Stateii");
    damage_number(lua, -10002, "screenWidth", owner_native_config[1]);
    damage_number(lua, -10002, "screenHeight", owner_native_config[2]);
    damage_lua.create_table(lua, 0, 1);
    damage_number(lua, 1, "worldGravity", owner_native_config[16]);
#if defined(AB_ORACLE_CAMERA_FOLLOW_PROPERTIES)
    damage_lua.create_table(lua, 0, 2);
    damage_lua.set_field(lua, 1, "world");
#endif
    damage_lua.set_field(lua, -10002, "objects");
    damage_lua.create_table(lua, 0, 2);
    damage_function(lua, 1, "setCameraTopLeft", owner_native_top_left);
    damage_function(lua, 1, "setCameraZoomScale", owner_native_zoom);
    damage_lua.set_field(lua, -10002, "CameraNative");
    damage_lua.get_field(lua, -10002, "PhysicsQueries");
    damage_function(lua, 1, "predictBirdImpactPosition", owner_native_predict);
    damage_lua.set_top(lua, 0);
    damage_function(lua, -10002, "physicsToWorldTransform", owner_native_transform);
    damage_lua.get_field(lua, -10002, "CameraSystem");
    damage_lua.get_field(lua, 1, "new");
    damage_lua.get_field(lua, -10002, "CameraSystem");
    owner_native_checked(lua, 1, 1);
    damage_lua.set_field(lua, -10002, "cameraSystem");
    damage_lua.set_top(lua, 0);
    damage_lua.create_table(lua, 0, 2);
    damage_function(lua, 1, "update", owner_native_input);
    damage_lua.push_number(lua, 501);
    damage_lua.set_field(lua, 1, "_id");
    damage_lua.set_field(lua, -10002, "_ab_owner_handler");
    damage_lua.get_field(lua, -10002, "cameraSystem");
    damage_lua.get_field(lua, -10002, "_ab_owner_handler");
    damage_lua.set_field(lua, 1, "cameraInputHandler");
    damage_lua.create_table(lua, 2, 0);
    for (int index = 1; index <= 2; ++index) {
        uint32_t first = index == 1 ? 3u : 6u;
        damage_lua.create_table(lua, 0, 4);
        damage_number(lua, 3, "x", owner_native_config[first]);
        damage_number(lua, 3, "y", owner_native_config[first + 1u]);
        damage_number(lua, 3, "zoom", owner_native_config[first + 2u]);
        damage_lua.push_number(lua, (float)index);
        damage_lua.set_field(lua, 3, "id");
        raw_set(lua, 2, index);
    }
    damage_lua.set_field(lua, 1, "snapPoints");
#if !defined(AB_ORACLE_CAMERA_FOLLOW_END_SEQUENCE)
    damage_function(lua, 1, "startCameraSequenceFromFile", owner_native_sequence);
#else
    damage_lua.push_nil(lua);
    damage_lua.set_field(lua, -10002, "_ab_owner_loaded_sequence");
#endif
    damage_lua.get_field(lua, 1, "setCameraLimitEnforcer");
    damage_lua.get_field(lua, -10002, "cameraSystem");
    damage_lua.push_nil(lua);
    damage_lua.push_nil(lua);
    owner_native_checked(lua, 3, 0);
    damage_lua.set_top(lua, 0);
    for (uint32_t index = 0; index < 2; ++index) {
        uint32_t first = index ? 19u : 9u;
        damage_lua.create_table(lua, 0, 4);
        damage_number(lua, 1, "x", owner_native_config[first]);
        damage_number(lua, 1, "y", owner_native_config[first + 1u]);
        damage_lua.push_number(lua, (float)(index + 1u));
        damage_lua.set_field(lua, 1, "_id");
#if !defined(AB_ORACLE_CLASSIC_CAMERA_LIVE)
        if (index || !(owner_native_config[18] & 2u)) {
            owner_native_vector(lua,
                                owner_follow_number(owner_native_config[first + 2u]),
                                owner_follow_number(owner_native_config[first + 3u]));
            damage_lua.set_field(lua, 1, "launchVelocity");
        }
#endif
#if defined(AB_ORACLE_CAMERA_FOLLOW_WORLD)
        owner_world_native_script(lua, 1, index + 1u);
#endif
        damage_lua.set_field(lua, -10002,
                             index ? "_ab_owner_bird2" : "_ab_owner_bird1");
    }
}

static void owner_native_prepare_input(void *lua) {
#if defined(AB_ORACLE_CAMERA_FOLLOW_END_SEQUENCE)
    damage_lua.push_boolean(lua, (owner_native_command[2] & 4u) != 0);
    damage_lua.set_field(lua, -10002, "hasMovingObjects");
#endif
    damage_number(lua, -10002, "cameraShakeX", owner_native_command[9]);
    damage_number(lua, -10002, "cameraShakeY", owner_native_command[10]);
    if (owner_native_command[2] & 2u) {
        damage_lua.create_table(lua, 0, 5);
        if (owner_native_command[3] & 1u) {
            damage_number(lua, 1, "x", owner_native_command[4]);
        }
        if (owner_native_command[3] & 2u) {
            damage_number(lua, 1, "y", owner_native_command[5]);
        }
        if (owner_native_command[3] & 4u) {
            damage_number(lua, 1, "zoom", owner_native_command[6]);
        }
        damage_number(lua, 1, "movementDuration", owner_native_command[7]);
        damage_number(lua, 1, "zoomDuration", owner_native_command[8]);
    } else {
        damage_lua.push_nil(lua);
    }
    damage_lua.set_field(lua, -10002, "_ab_owner_input");
}

static void owner_native_self(void *lua, const char *method) {
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "cameraSystem");
    damage_lua.get_field(lua, 1, method);
    damage_lua.get_field(lua, -10002, "cameraSystem");
}

static void owner_native_remove(void *lua, uint32_t id) {
    owner_native_self(lua, "onRemoveBird");
    if (id) {
        damage_lua.get_field(lua, -10002,
                             id == 1 ? "_ab_owner_bird1" : "_ab_owner_bird2");
    } else {
        damage_lua.push_nil(lua);
    }
    owner_native_checked(lua, 2, 0);
    damage_lua.set_top(lua, 0);
}

static void owner_native_goto(void *lua) {
    owner_native_self(lua, "gotoTargetPosition");
    for (uint32_t index = 0; index < 3; ++index) {
        if (owner_native_command[3] & (1u << index)) {
            damage_lua.push_number(
                lua, owner_follow_number(owner_native_command[4 + index]));
        } else {
            damage_lua.push_nil(lua);
        }
    }
    damage_lua.push_number(lua, owner_follow_number(owner_native_command[7]));
    damage_lua.push_nil(lua);
    damage_lua.push_nil(lua);
    damage_lua.push_nil(lua);
    owner_native_checked(lua, 8, 0);
    damage_lua.set_top(lua, 0);
}

static void owner_native_action(void *lua) {
    uint32_t action = owner_native_command[0];
#if defined(AB_ORACLE_CAMERA_FOLLOW_END_SEQUENCE)
    if (action == 10) {
        damage_lua.get_field(lua, -10002, "cameraSystem");
        damage_lua.get_field(lua, 1, "startCameraSequenceFromFile");
        damage_lua.get_field(lua, -10002, "cameraSystem");
        damage_lua.push_string(lua, "FollowShotBirdEndCameraSequence");
        owner_native_checked(lua, 2, 0);
        damage_lua.set_top(lua, 0);
        return;
    }
#endif
#if defined(AB_ORACLE_CAMERA_SHOT_SEQUENCE)
    if (action == 1 || action == 2) {
        damage_lua.get_field(lua, -10002,
                             action == 1 ? "_ab_owner_bird1" : "_ab_owner_bird2");
        damage_lua.set_field(lua, -10002, "flyingBird");
        const char *const methods[] = {"_ab_bird_camera_launch",
                                       "_ab_museum_camera_launch",
                                       "_ab_blues_camera_init"};
        damage_lua.get_field(lua, -10002, methods[owner_native_config[0] % 3u]);
        damage_lua.push_nil(lua);
        owner_native_checked(lua, 1, 0);
        damage_lua.set_top(lua, 0);
        return;
    }
#endif
    if (action == 1 || action == 2) {
        owner_native_self(lua, "followBird");
        damage_lua.get_field(lua, -10002,
                             action == 1 ? "_ab_owner_bird1" : "_ab_owner_bird2");
        if (owner_native_config[18] & 4u) {
            damage_lua.push_nil(lua);
        } else {
            damage_lua.push_boolean(lua, 0);
        }
        if (owner_native_config[18] & 8u) {
            damage_lua.push_number(lua, 0);
        } else {
            damage_lua.push_nil(lua);
        }
        owner_native_checked(lua, 4, 0);
    } else if (action == 3 || action == 4 || action == 8) {
        owner_native_remove(lua, action == 8 ? 0u : action - 2u);
    } else if (action == 5) {
        owner_native_self(lua, "followBird");
        owner_native_checked(lua, 1, 0);
    } else if (action == 6 || action == 9) {
        owner_native_goto(lua);
        if (action == 9) {
            owner_native_remove(lua, 0);
        }
    } else if (action == 7) {
        owner_native_self(lua, "gotoSnapPoint");
        damage_lua.push_number(lua, 1);
        damage_lua.push_number(lua, owner_follow_number(owner_native_command[7]));
        damage_lua.push_boolean(lua, 1);
        damage_lua.push_nil(lua);
        damage_lua.push_nil(lua);
        damage_lua.push_boolean(lua, 1);
        owner_native_checked(lua, 7, 0);
    } else if (action) {
        fail("Invalid original owner fixture action\n");
    }
    damage_lua.set_top(lua, 0);
}

#endif
