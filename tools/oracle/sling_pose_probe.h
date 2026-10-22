#ifndef AB_ORACLE_SLING_POSE_H
#define AB_ORACLE_SLING_POSE_H

static uint32_t sling_pose_input[10], *sling_pose_angle, *sling_pose_distance;
static uint32_t sling_pose_coordinates[4], sling_pose_transforms[2][4];
static uint32_t sling_pose_transform_count;
static void (*sling_pose_transform)(FloatBits[2], const void *, float, float);

static int sling_pose_to_world(void *lua) {
    if (damage_lua.get_top(lua) != 2 || damage_lua.type(lua, 1) != 3 ||
        damage_lua.type(lua, 2) != 3 || sling_pose_transform_count >= 2) {
        fail("Unexpected original sling physics-to-world boundary\n");
    }
    FloatBits x = {.number = damage_lua.to_number(lua, 1)};
    FloatBits y = {.number = damage_lua.to_number(lua, 2)}, output[2];
    sling_pose_transform(output, sling_pose_coordinates, x.number, y.number);
    uint32_t *event = sling_pose_transforms[sling_pose_transform_count++];
    event[0] = x.bits;
    event[1] = y.bits;
    event[2] = output[0].bits;
    event[3] = output[1].bits;
    damage_lua.push_number(lua, output[0].number);
    damage_lua.push_number(lua, output[1].number);
    return 2;
}

static void sling_pose_call(void *lua, int arguments, int returns) {
    if (damage_lua.call(lua, arguments, returns, 0)) {
        const char *message = damage_lua.to_string(lua, -1, NULL);
        fail(message ? message : "Original sling pose call failed\n");
    }
}

static uint32_t sling_pose_field(void *lua, int table, const char *name) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, table, name);
    if (damage_lua.type(lua, -1) != 3) {
        fail("Unexpected original sling pose numeric field\n");
    }
    FloatBits value = {.number = damage_lua.to_number(lua, -1)};
    damage_lua.set_top(lua, top);
    return value.bits;
}

static void sling_pose_prepare(void *lua) {
    damage_function(lua, -10002, "physicsToWorldTransform", sling_pose_to_world);
    sling_pose_transform =
        (void (*)(FloatBits[2], const void *, float, float))find_function(
            "_ZNK20CoordinateTransforms23physicsToWorldTransformEff");
    damage_lua.get_field(lua, -10002, "SlingshotSystem");
    damage_lua.get_field(lua, 1, "calculateRubberBand");
    const uint32_t *closure = damage_lua.to_pointer(lua, -1);
    if (!closure || damage_lua.type(lua, -1) != 6 || ((const uint8_t *)closure)[6] ||
        ((const uint8_t *)closure)[7] != 4) {
        fail("Unexpected original sling pose closure layout\n");
    }
    sling_pose_angle = (uint32_t *)(uintptr_t)((uint32_t *)(uintptr_t)closure[7])[2];
    sling_pose_distance = (uint32_t *)(uintptr_t)((uint32_t *)(uintptr_t)closure[8])[2];
    if (sling_pose_angle[1] != 3 || sling_pose_distance[1] != 3) {
        fail("Unexpected original sling pose scalar upvalue types\n");
    }
    damage_lua.set_top(lua, 0);
}

static void sling_pose_case(void *lua) {
    sling_pose_angle[0] = sling_pose_input[4];
    sling_pose_distance[0] = sling_pose_input[5];
    const char *vectors[] = {"getLevelStartPosition", "getRubberBandPosition"};
    for (size_t index = 0; index < 2; ++index) {
        damage_lua.get_field(lua, -10002, "SlingshotSystem");
        damage_lua.get_field(lua, 1, vectors[index]);
        damage_lua.get_field(lua, -10002, "SlingshotSystem");
        sling_pose_call(lua, 1, 1);
        damage_number(lua, 2, "x", sling_pose_input[index * 2]);
        damage_number(lua, 2, "y", sling_pose_input[index * 2 + 1]);
        damage_lua.set_top(lua, 0);
    }
    damage_number(lua, -10002, "physicsToWorld", sling_pose_input[8]);
    if (sling_pose_input[9] & 1) {
        damage_lua.push_string(lua, "first");
    } else {
        damage_lua.push_nil(lua);
    }
    damage_lua.set_field(lua, -10002, "currentBirdName");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 3);
    if (sling_pose_input[9] & 2) {
        damage_number(lua, 3, "radius", sling_pose_input[6]);
    }
    damage_number(lua, 3, "width", sling_pose_input[7]);
    damage_lua.push_string(lua, sling_pose_input[9] & 4 ? "box" : "ball");
    damage_lua.set_field(lua, 3, "type");
    damage_lua.set_field(lua, 2, "first");
    damage_lua.set_field(lua, 1, "world");
    damage_lua.set_field(lua, -10002, "objects");
    damage_lua.create_table(lua, 0, 4);
    damage_lua.set_field(lua, -10002, "_ab_sling_pose");
}

static void probe_sling_pose(void) {
    void *lua = open_sling_reference();
    sling_pose_prepare(lua);
    uint32_t random = UINT32_C(0x520905e0);
    semihost(4, "AB_ANDROID_SLING_POSE_V1 4096\n");
    for (size_t item = 0; item < 4096; ++item) {
        for (size_t field = 0; field < 9; ++field) {
            sling_pose_input[field] = random_float(&random, 120, 11, 1);
        }
        FloatBits angle = {
            .number = (float)((int32_t)(random_word(&random) % 8193) - 4096) / 4};
        sling_pose_input[4] = angle.bits;
        sling_pose_input[9] = (uint32_t)item & 7;
        if (item % 7 == 0) {
            const float distances[] = {0, -0.0f, 2, 5, 2.00000024f, 4.99999952f};
            FloatBits distance = {.number = distances[(item / 7) % 6]};
            sling_pose_input[5] = distance.bits;
        }
        if (item % 17 == 0) {
            for (size_t field = 0; field < 9; ++field) {
                sling_pose_input[field] &= UINT32_C(0x80000000);
            }
        }
        sling_pose_case(lua);
        sling_pose_transform_count = 0;
        damage_lua.get_field(lua, -10002, "SlingshotSystem");
        damage_lua.get_field(lua, 1, "calculateRubberBand");
        damage_lua.get_field(lua, -10002, "_ab_sling_pose");
        sling_pose_call(lua, 1, 0);
        damage_lua.set_top(lua, 0);
        if (sling_pose_transform_count != 2) {
            fail("Incomplete original sling coordinate transforms\n");
        }
        uint32_t output[6];
        damage_lua.get_field(lua, -10002, "_ab_sling_pose");
        const char *vectors[] = {"pos", "stretchPos"};
        for (size_t index = 0; index < 2; ++index) {
            damage_lua.get_field(lua, 1, vectors[index]);
            output[index * 2] = sling_pose_field(lua, 2, "x");
            output[index * 2 + 1] = sling_pose_field(lua, 2, "y");
            damage_lua.set_top(lua, 1);
        }
        output[4] = sling_pose_field(lua, 1, "rubberBandAngle");
        output[5] = sling_pose_field(lua, 1, "rubberBandWidth");
        damage_lua.set_top(lua, 0);
        print_words(sling_pose_input, 10);
        print_words(sling_pose_transforms[0], 4);
        print_words(sling_pose_transforms[1], 4);
        print_words(output, 6);
    }
    damage_lua.close_state(lua);
    semihost(4, "AB sling pose reference complete\n");
}

#endif
