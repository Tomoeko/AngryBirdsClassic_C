#ifndef AB_ORACLE_SLING_IMPULSE_H
#define AB_ORACLE_SLING_IMPULSE_H

static uint32_t impulse_result[4], impulse_calls[4];

#if defined(AB_ORACLE_SLING_ROUTING_ONLY)
static uint32_t routing_input[4], routing_release[2];
static uint32_t *routing_threshold;

static int routing_distance(void *lua) {
    FloatBits distance = {.bits = routing_input[2]};
    damage_lua.push_number(lua, distance.number);
    return 1;
}

static int routing_launch(void *lua) {
    (void)lua;
    ++routing_release[0];
    return 0;
}

static int routing_cancel(void *lua) {
    (void)lua;
    ++routing_release[1];
    return 0;
}

static void routing_value(void *lua, const char *name, uint32_t kind) {
    switch (kind) {
        case 0:
            damage_lua.push_nil(lua);
            break;
        case 1:
        case 2:
            damage_lua.push_boolean(lua, kind == 2);
            break;
        case 3:
            damage_lua.push_number(lua, 0);
            break;
        default:
            damage_lua.push_string(lua, "mode");
            break;
    }
    damage_lua.set_field(lua, -10002, name);
}

static void prepare_routing(void *lua) {
    damage_lua.get_field(lua, -10002, "SlingshotSystem");
    damage_lua.get_field(lua, 1, "isStretchedEnoughToShootBird");
    const uint32_t *closure = damage_lua.to_pointer(lua, -1);
    const uint8_t *header = (const uint8_t *)closure;
    if (!closure || header[6] || header[7] != 1) {
        fail("Original release threshold closure mismatch\n");
    }
    routing_threshold = (uint32_t *)(uintptr_t)((uint32_t *)(uintptr_t)closure[5])[2];
    damage_lua.create_table(lua, 0, 4);
    damage_lua.get_field(lua, 1, "isStretchedEnoughToShootBird");
    damage_lua.set_field(lua, 3, "isStretchedEnoughToShootBird");
    damage_function(lua, 3, "getDistanceToRestPosition", routing_distance);
    damage_function(lua, 3, "launchBird", routing_launch);
    damage_function(lua, 3, "cancelBirdDrag", routing_cancel);
    damage_lua.set_field(lua, -10002, "_ab_release_self");
    damage_lua.set_top(lua, 0);
}
#endif

static void impulse_call(void *lua, int arguments, int results) {
    if (damage_lua.call(lua, arguments, results, 0)) {
        const char *error = damage_lua.to_string(lua, -1, NULL);
        fail(error ? error : "Original launch impulse call failed\n");
    }
}

static uint32_t impulse_coordinate(void *lua, int table, const char *field) {
    damage_lua.get_field(lua, table, field);
    if (damage_lua.type(lua, -1) != 3) {
        fail("Original impulse boundary coordinate is not numeric\n");
    }
    FloatBits value = {.number = damage_lua.to_number(lua, -1)};
    damage_lua.set_top(lua, damage_lua.get_top(lua) - 1);
    return value.bits;
}

static int impulse_boundary(void *lua) {
    if (++impulse_calls[0] != 1 || damage_lua.get_top(lua) != 3) {
        fail("Unexpected original launch impulse boundary count\n");
    }
    impulse_result[0] = impulse_coordinate(lua, 2, "x");
    impulse_result[1] = impulse_coordinate(lua, 2, "y");
    impulse_result[2] = impulse_coordinate(lua, 3, "x");
    impulse_result[3] = impulse_coordinate(lua, 3, "y");
    return 0;
}

static int impulse_statistics(void *lua) {
    if (damage_lua.get_top(lua) != 1 || ++impulse_calls[1] != 1) {
        fail("Unexpected original shots statistic boundary\n");
    }
    impulse_calls[3] = (impulse_calls[3] << 4) | 1;
    return 0;
}

static int impulse_backwards(void *lua) {
    if (damage_lua.get_top(lua) != 1 || ++impulse_calls[2] != 1) {
        fail("Unexpected original backwards statistic boundary\n");
    }
    impulse_calls[3] = (impulse_calls[3] << 4) | 2;
    return 0;
}

static void impulse_root_vector(void *lua, const char *getter, const uint32_t bits[2]) {
    damage_lua.get_field(lua, -10002, "SlingshotSystem");
    damage_lua.get_field(lua, 1, getter);
    damage_lua.get_field(lua, -10002, "_ab_sling_self");
    impulse_call(lua, 1, 1);
    damage_number(lua, 2, "x", bits[0]);
    damage_number(lua, 2, "y", bits[1]);
    damage_lua.set_top(lua, 0);
}

static void prepare_impulse_case(void *lua) {
    prepare_sling_launch(lua);
    damage_lua.get_field(lua, -10002, "_ab_sling_self");
    damage_lua.get_field(lua, -10002, "SlingshotSystem");
    damage_lua.get_field(lua, 2, "getVectorToRestPosition");
    damage_lua.set_field(lua, 1, "getVectorToRestPosition");
    damage_lua.get_field(lua, 2, "getDistanceToRestPosition");
    damage_lua.set_field(lua, 1, "getDistanceToRestPosition");
    damage_lua.set_top(lua, 1);
    damage_function(lua, 1, "launchBirdWithImpulse", impulse_boundary);
    damage_lua.set_top(lua, 0);
    impulse_root_vector(lua, "getLevelStartPosition", sling_input);
    impulse_root_vector(lua, "getRubberBandPosition", sling_input + 2);

    damage_lua.get_field(lua, -10002, "SlingshotSystem");
    damage_lua.get_field(lua, 1, "setDraggedBird");
    damage_lua.get_field(lua, -10002, "_ab_sling_self");
    damage_lua.create_table(lua, 0, 2);
    damage_lua.push_string(lua, "first");
    damage_lua.set_field(lua, -2, "name");
    damage_number(lua, -2, "mass", sling_input[8]);
    impulse_call(lua, 2, 0);
    damage_lua.set_top(lua, 0);
}

static void probe_sling_impulse(void) {
    void *lua = open_sling_reference();
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, -2, "stopMovement", damage_require);
    damage_lua.set_field(lua, -10002, "cameraSystem");
    damage_lua.create_table(lua, 0, 2);
    damage_function(lua, -2, "incrementBirdsShot", impulse_statistics);
    damage_function(lua, -2, "incrementBackwardsBirdCount", impulse_backwards);
    damage_lua.set_field(lua, -10002, "settingsWrapper");
    /* Keep statistic eligibility outside this arithmetic corpus. The original
     * launch function still executes its unconditional shots update. */
    damage_lua.push_boolean(lua, 1);
    damage_lua.set_field(lua, -10002, "eagleBaitLaunched");
    damage_lua.push_boolean(lua, 0);
    damage_lua.set_field(lua, -10002, "g_deltaBasedSlingshot");
#if defined(AB_ORACLE_SLING_ROUTING_ONLY)
    prepare_routing(lua);
#endif
    uint32_t state = UINT32_C(0x52051ae1);
#if defined(AB_ORACLE_SLING_ROUTING_ONLY)
    semihost(4, "AB_ANDROID_SLING_ROUTING_V1 4096\n");
#else
    semihost(4, "AB_ANDROID_SLING_IMPULSE_V1 4096\n");
#endif
    for (size_t index = 0; index < 4096; ++index) {
        uint32_t input[14];
        for (size_t field = 0; field < 4; ++field) {
            input[field] = random_float(&state, 122, 10, 1);
        }
        input[4] = random_float(&state, 122, 7, index % 13 == 0);
        input[5] = index % 19 ? random_float(&state, 124, 10, 0) : 0;
        input[6] = random_float(&state, 123, 9, 0);
        if (index & 16) {
            input[5] ^= UINT32_C(0x80000000);
            input[6] ^= UINT32_C(0x80000000);
        }
        input[7] = random_float(&state, 119, 3, 0);
        input[8] = random_float(&state, 124, 4, 0);
        input[9] = random_float(&state, 125, 3, index % 11 == 0);
        input[10] = 1 | ((uint32_t)index & 2);
        input[11] = random_float(&state, 122, 10, 1);
        input[12] = random_float(&state, 122, 10, 1);
        input[13] = (uint32_t)index & 1;
        if (index % 13 == 0) {
            input[11] = index & 2 ? UINT32_C(0x80000000) : 0;
            input[12] = index & 4 ? UINT32_C(0xbf800000) : UINT32_C(0x3f800000);
            input[13] = 1;
        }
        if (index >= 32 && index < 64) {
            FloatBits stretch = {.number = (float)(index - 32) / 31.0f};
            input[4] = stretch.bits;
            input[6] = UINT32_C(0xc4674000);
            input[7] = UINT32_C(0x3d4ccccd);
            input[8] = UINT32_C(0x4159e6a0);
            input[9] = UINT32_C(0x3f800000);
            input[10] = 1;
        }
        for (size_t field = 0; field < 11; ++field) {
            sling_input[field] = input[field];
        }
        prepare_impulse_case(lua);
#if defined(AB_ORACLE_SLING_ROUTING_ONLY)
        static const uint32_t boundaries[] = {
            UINT32_C(0xff800000), UINT32_C(0x80000000), 0,
            UINT32_C(0x3fffffff), UINT32_C(0x40000000), UINT32_C(0x40000001),
            UINT32_C(0x7f800000), UINT32_C(0x7fc00000)};
        routing_input[0] = (uint32_t)(index % 5);
        routing_input[1] = (uint32_t)((index / 5) % 5);
        routing_input[2] = index < 64 ? boundaries[index & 7]
                                      : random_float(&state, 122, 8, index & 1);
        routing_input[3] =
            index < 64 ? boundaries[index >> 3] : random_float(&state, 122, 8, 0);
        routing_value(lua, "eagleBaitLaunched", routing_input[0]);
        routing_value(lua, "g_special_game_mode", routing_input[1]);
#endif
        print_words(input, 14);
#if defined(AB_ORACLE_SLING_ROUTING_ONLY)
        print_words(routing_input, 4);
#endif
        for (size_t field = 0; field < 4; ++field) {
            impulse_calls[field] = 0;
        }
        damage_lua.get_field(lua, -10002, "SlingshotSystem");
        damage_lua.get_field(lua, 1, "launchBird");
        damage_lua.get_field(lua, -10002, "_ab_sling_self");
        if (input[13]) {
            damage_lua.create_table(lua, 0, 2);
            damage_number(lua, -2, "x", input[11]);
            damage_number(lua, -2, "y", input[12]);
        } else {
            damage_lua.push_nil(lua);
        }
        impulse_call(lua, 2, 0);
        damage_lua.set_top(lua, 0);
        print_words(impulse_result, 4);
#if defined(AB_ORACLE_SLING_ROUTING_ONLY)
        routing_threshold[0] = routing_input[3];
        routing_threshold[1] = 3;
        routing_release[0] = routing_release[1] = 0;
        damage_lua.get_field(lua, -10002, "SlingshotSystem");
        damage_lua.get_field(lua, 1, "releaseRubberBandToLaunchBird");
        damage_lua.get_field(lua, -10002, "_ab_release_self");
        impulse_call(lua, 1, 0);
        damage_lua.set_top(lua, 0);
        print_words(impulse_calls, 4);
        print_words(routing_release, 2);
#else
        print_words(impulse_calls, 2);
#endif
    }
    damage_lua.close_state(lua);
#if defined(AB_ORACLE_SLING_ROUTING_ONLY)
    semihost(4, "AB sling routing reference complete\n");
#else
    semihost(4, "AB sling impulse reference complete\n");
#endif
}

#endif
