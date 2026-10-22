#ifndef AB_ORACLE_SLING_START_POSE_H
#define AB_ORACLE_SLING_START_POSE_H

static uint32_t sling_start_pose_input[7], sling_start_pose_calls;

static int sling_start_pose_sprite(void *lua, bool pivot) {
    if (damage_lua.get_top(lua) != 2 || !damage_named(lua, 1, "") ||
        !damage_named(lua, 2, "SLING_SHOT_01_BACK") ||
        sling_start_pose_calls != (pivot ? 0u : 1u)) {
        fail("Unexpected original sling start sprite query\n");
    }
    ++sling_start_pose_calls;
    FloatBits unused = {.bits = UINT32_C(0x7fc12345)};
    FloatBits value = {.bits = sling_start_pose_input[pivot ? 3 : 4]};
    damage_lua.push_number(lua, unused.number);
    damage_lua.push_number(lua, value.number);
    return 2;
}

static int sling_start_pose_pivot(void *lua) {
    return sling_start_pose_sprite(lua, true);
}

static int sling_start_pose_bounds(void *lua) {
    return sling_start_pose_sprite(lua, false);
}

static void sling_start_pose_call(void *lua, int arguments, int returns) {
    if (damage_lua.call(lua, arguments, returns, 0)) {
        const char *message = damage_lua.to_string(lua, -1, NULL);
        fail(message ? message : "Original sling start pose failed\n");
    }
}

static void probe_sling_start_pose(void) {
    void *lua = open_sling_reference();
    damage_lua.create_table(lua, 0, 2);
    damage_function(lua, 1, "getSpritePivot", sling_start_pose_pivot);
    damage_function(lua, 1, "getSpriteBounds", sling_start_pose_bounds);
    damage_lua.set_field(lua, -10002, "res");
    const char *getters[] = {"getLevelStartPosition", "getSlingshotBasePosition",
                             "getRubberBandPosition"};
    uint32_t random = UINT32_C(0x5205a271);
    semihost(4, "AB_ANDROID_SLING_START_POSE_V1 4096\n");
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t retained[6], output[6];
        for (size_t field = 0; field < 6; ++field) {
            sling_start_pose_input[field] = random_float(&random, 119, 14, 1);
            retained[field] = random_float(&random, 121, 10, 1);
        }
        sling_start_pose_input[6] = (uint32_t)item & 3;
        if (item % 17 == 0) {
            for (size_t field = 0; field < 6; ++field) {
                sling_start_pose_input[field] &= UINT32_C(0x80000000);
            }
        }
        for (size_t vector = 0; vector < 3; ++vector) {
            damage_lua.get_field(lua, -10002, "SlingshotSystem");
            damage_lua.get_field(lua, 1, getters[vector]);
            damage_lua.get_field(lua, -10002, "SlingshotSystem");
            sling_start_pose_call(lua, 1, 1);
            damage_number(lua, 2, "x", retained[vector * 2]);
            damage_number(lua, 2, "y", retained[vector * 2 + 1]);
            damage_lua.set_top(lua, 0);
        }
        damage_number(lua, -10002, "physicsScale", sling_start_pose_input[5]);
        damage_lua.get_field(lua, -10002, "SlingshotSystem");
        damage_lua.get_field(lua, 1, "setPositionByStartObject");
        damage_lua.get_field(lua, -10002, "SlingshotSystem");
        if (sling_start_pose_input[6] & 1) {
            damage_lua.create_table(lua, 0, 3);
            damage_number(lua, 4, "x", sling_start_pose_input[0]);
            damage_number(lua, 4, "y", sling_start_pose_input[1]);
            if (sling_start_pose_input[6] & 2) {
                damage_number(lua, 4, "radius", sling_start_pose_input[2]);
            }
        } else {
            damage_lua.push_nil(lua);
        }
        sling_start_pose_calls = 0;
        sling_start_pose_call(lua, 2, 0);
        damage_lua.set_top(lua, 0);
        for (size_t vector = 0; vector < 3; ++vector) {
            damage_lua.get_field(lua, -10002, "SlingshotSystem");
            damage_lua.get_field(lua, 1, getters[vector]);
            damage_lua.get_field(lua, -10002, "SlingshotSystem");
            sling_start_pose_call(lua, 1, 1);
            const char *axes[] = {"x", "y"};
            for (size_t axis = 0; axis < 2; ++axis) {
                damage_lua.get_field(lua, 2, axes[axis]);
                if (damage_lua.type(lua, -1) != 3) {
                    fail("Unexpected original sling start vector field\n");
                }
                FloatBits value = {.number = damage_lua.to_number(lua, -1)};
                output[vector * 2 + axis] = value.bits;
                damage_lua.set_top(lua, 2);
            }
            damage_lua.set_top(lua, 0);
        }
        print_words(sling_start_pose_input, 7);
        print_words(retained, 6);
        print_words(&sling_start_pose_calls, 1);
        print_words(output, 6);
    }
    damage_lua.close_state(lua);
    semihost(4, "AB sling start pose reference complete\n");
}

#endif
