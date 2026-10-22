#ifndef AB_ORACLE_GAME_UPDATE_JOINTS_H
#define AB_ORACLE_GAME_UPDATE_JOINTS_H

static bool tail_joint_append_pending;

static const char *const tail_joint_names[] = {"first", "second", "third", "missing",
                                               "absent"};

static uint32_t tail_joint_id(void *lua, int argument) {
    for (size_t index = 0; index < 5; ++index) {
        if (damage_named(lua, argument, tail_joint_names[index])) {
            return (uint32_t)index + 1;
        }
    }
    fail("Unexpected original game joint phase name\n");
    return 0;
}

static int tail_joint_limit(void *lua) {
    tail_record(lua, 23, tail_joint_id(lua, 1), 1, 0, 0);
    return 0;
}

static int tail_joint_destroy(void *lua) {
    tail_record(lua, 24, tail_joint_id(lua, 1), 1, 0, 0);
    if (tail_joint_append_pending) {
        tail_joint_append_pending = false;
        unsigned (*length)(void *, int) =
            (unsigned (*)(void *, int))find_function("_Z10lua_objlenP9lua_Statei");
        void (*set_index)(void *, int, int) =
            (void (*)(void *, int, int))find_function("_Z11lua_rawsetiP9lua_Stateii");
        damage_lua.get_field(lua, -10002, "g_jointsToDestroy");
        unsigned count = length(lua, 2);
        if (count > 4) {
            fail("Unexpected original joint queue during callback append\n");
        }
        damage_lua.push_string(lua, "second");
        set_index(lua, 2, (int)count + 1);
        damage_lua.set_top(lua, 1);
    }
    return 0;
}

static void tail_prepare_joints(void *lua, uint32_t sample) {
    tail_input[15] = sample & 15u;
    tail_input[16] = (sample >> 4) & 15u;
    tail_input[17] = (sample / 64) % 5;
    tail_joint_append_pending = (sample & 2048u) != 0;
    damage_function(lua, -10002, "checkJointLimits", tail_joint_limit);
    damage_function(lua, -10002, "destroyJoint", tail_joint_destroy);
    damage_lua.get_field(lua, -10002, "objects");
    damage_lua.get_field(lua, 1, "joints");
    for (size_t index = 0; index < 4; ++index) {
        tail_input[18 + index] = 0;
        if (tail_input[15] & (1u << index)) {
            damage_lua.create_table(lua, 0, 2);
            damage_lua.push_string(lua, tail_joint_names[index]);
            damage_lua.set_field(lua, -2, "name");
            damage_lua.push_boolean(lua, (tail_input[16] & (1u << index)) != 0);
            damage_lua.set_field(lua, -2, "backAndForth");
            damage_lua.set_field(lua, 2, tail_joint_names[index]);
        }
    }
    damage_lua.push_nil(lua);
    size_t count = 0;
    while (tail_next(lua, 2)) {
        if (count == 4) {
            fail("Original joint metadata traversal exceeded its bound\n");
        }
        tail_input[18 + count++] = tail_joint_id(lua, 3);
        damage_lua.set_top(lua, 3);
    }
    damage_lua.set_top(lua, 0);
    damage_lua.create_table(lua, 0, 0);
    void (*set_index)(void *, int, int) =
        (void (*)(void *, int, int))find_function("_Z11lua_rawsetiP9lua_Stateii");
    for (uint32_t index = 0; index < tail_input[17]; ++index) {
        uint32_t id = ((sample >> 8) + index) % 5;
        damage_lua.push_string(lua, tail_joint_names[id]);
        set_index(lua, 1, (int)index + 1);
    }
    damage_lua.set_field(lua, -10002, "g_jointsToDestroy");
    damage_lua.set_top(lua, 0);
}

static uint32_t tail_joint_queue_count(void *lua) {
    unsigned (*length)(void *, int) =
        (unsigned (*)(void *, int))find_function("_Z10lua_objlenP9lua_Statei");
    damage_lua.get_field(lua, -10002, "g_jointsToDestroy");
    if (damage_lua.type(lua, 1) != 5) {
        fail("Original joint destruction queue is no longer a table\n");
    }
    uint32_t result = length(lua, 1);
    damage_lua.set_top(lua, 0);
    return result;
}

#endif
