#ifndef AB_ORACLE_CAMERA_FOLLOW_SYSTEM_STATE_H
#define AB_ORACLE_CAMERA_FOLLOW_SYSTEM_STATE_H

static uint32_t owner_native_optional(void *lua, int table, const char *field,
                                      uint32_t *present) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, table, field);
    int type = damage_lua.type(lua, -1);
    if (type != 0 && type != 3) {
        fail("Unexpected original camera owner numeric state type\n");
    }
    *present = type == 3;
    FloatBits value = {.number = type == 3 ? damage_lua.to_number(lua, -1) : 0};
    damage_lua.set_top(lua, top);
    return value.bits;
}

static uint32_t owner_native_tween(void *lua, int mover, const char *field,
                                   int movement) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, mover, field);
    int type = damage_lua.type(lua, -1);
    if (type == 5 && movement) {
        damage_lua.set_top(lua, top);
        return 5;
    }
    if (type != 6) {
        fail("Unexpected original owner mover tween type\n");
    }
    const void *selected = damage_lua.to_pointer(lua, -1);
    damage_lua.get_field(lua, -10002, "tweenEaseCubicInOut");
    uint32_t kind = selected == damage_lua.to_pointer(lua, -1) ? 0u : 5u;
    damage_lua.set_top(lua, top + 1);
    damage_lua.get_field(lua, -10002, "easeSmoothStep");
    if (selected == damage_lua.to_pointer(lua, -1)) {
        kind = 4;
    }
    damage_lua.set_top(lua, top);
    if (kind == 5) {
        const uint8_t *closure = selected;
        const uint32_t *prototype =
            (const uint32_t *)(uintptr_t)((const uint32_t *)closure)[4];
        if (movement || closure[6] || closure[7] != 6 || !prototype ||
            prototype[11] != 40) {
            fail("Unexpected original owner captured zoom closure\n");
        }
    }
    return kind;
}

static void owner_native_snapshot(void *lua,
                                  uint32_t output[OWNER_FOLLOW_OUTPUT_WORDS]) {
    damage_lua.get_field(lua, -10002, "cameraSystem");
    damage_lua.get_field(lua, 1, "getTargetBird");
    damage_lua.get_field(lua, -10002, "cameraSystem");
    owner_native_checked(lua, 1, 1);
    if (damage_lua.type(lua, 2) == 5) {
        FloatBits id = {.bits = owner_native_number(lua, 2, "_id")};
        output[2] = (uint32_t)id.number;
    } else if (damage_lua.type(lua, 2) != 0) {
        fail("Unexpected original camera owner target query type\n");
    }
    damage_lua.set_top(lua, 1);
    damage_lua.get_field(lua, 1, "automaticBehavior");
    output[1] = damage_lua.to_boolean(lua, 2) != 0;
    if (output[1]) {
        damage_lua.get_field(lua, 2, "name");
        int following = damage_named(lua, 3, "FollowBirdCameraMovementBehavior");
        damage_lua.set_top(lua, 2);
        if (following) {
            output[3] = 1;
            damage_lua.get_field(lua, 2, "hitPosition");
            if (damage_lua.type(lua, 3) == 5) {
                output[4] = 1;
                output[5] = owner_native_number(lua, 3, "x");
                output[6] = owner_native_number(lua, 3, "y");
            } else if (damage_lua.type(lua, 3) != 0) {
                fail("Unexpected original owner follow hit type\n");
            }
            damage_lua.set_top(lua, 2);
            output[8] = owner_native_optional(lua, 2, "hitTime", output + 7);
            output[10] = owner_native_optional(lua, 2, "originalZoom", output + 9);
            output[12] = owner_native_optional(lua, 2, "timeToPeak", output + 11);
            output[14] = owner_native_optional(lua, 2, "scrollTimer", output + 13);
            damage_lua.get_field(lua, 2, "peakIsHitPosition");
            output[15] = damage_lua.type(lua, 3) != 0;
            output[16] = damage_lua.to_boolean(lua, 3) != 0;
            damage_lua.set_top(lua, 2);
            damage_lua.get_field(lua, 2, "allowCancel");
            output[17] = damage_lua.type(lua, 3) != 0;
            output[18] = damage_lua.to_boolean(lua, 3) != 0;
            damage_lua.set_top(lua, 2);
            output[19] = owner_native_number(lua, 2, "movementDurationScaler");
            output[20] = owner_native_number(lua, 2, "velocityCompensation");
            output[21] = owner_native_number(lua, 2, "minimumMovementDuration");
            output[22] = owner_native_number(lua, 2, "timer");
        }
    }
    damage_lua.set_top(lua, 1);
    damage_lua.get_field(lua, 1, "cameraMover");
    const char *const vectors[] = {"currentPosition", "targetPosition",
                                   "startPosition"};
    for (uint32_t index = 0; index < 3; ++index) {
        damage_lua.get_field(lua, 2, vectors[index]);
        if (damage_lua.type(lua, 3) == 5) {
            if (index)
                output[23] |= index == 1 ? 1u : 4u;
            output[24 + index * 2u] = owner_native_number(lua, 3, "x");
            output[25 + index * 2u] = owner_native_number(lua, 3, "y");
        } else if (damage_lua.type(lua, 3) != 0) {
            fail("Unexpected original owner mover vector type\n");
        }
        damage_lua.set_top(lua, 2);
    }
    const char *const scalars[] = {
        "currentZoomScale", "targetZoomScale",   "currentZoomVelocity",
        "startZoomScale",   "startZoom",         "movementTweenDuration",
        "movementTimer",    "zoomTweenDuration", "zoomTimer"};
    for (uint32_t index = 0; index < 9; ++index) {
        uint32_t present = 0;
        output[30 + index] = owner_native_optional(lua, 2, scalars[index], &present);
        if (present && (index == 1 || index == 3 || index == 4)) {
            output[23] |= index == 1 ? 2u : index == 3 ? 8u : 16u;
        }
    }
    output[39] = owner_native_tween(lua, 2, "movementTweenFunction", 1);
    output[40] = owner_native_tween(lua, 2, "zoomTweenFunction", 0);
    damage_lua.set_top(lua, 1);
    damage_lua.get_field(lua, 1, "screen");
    output[41] = owner_native_number(lua, 2, "x");
    output[42] = owner_native_number(lua, 2, "y");
    damage_lua.set_top(lua, 1);
    damage_lua.get_field(lua, 1, "cameraLimitEnforcer");
    damage_lua.get_field(lua, 2, "closeZoom");
    output[52] = (uint32_t)damage_lua.type(lua, 3);
    damage_lua.set_top(lua, 2);
    const char *const limits[] = {"closeZoomLevel", "farZoomLevel", "bottomBorder",
                                  "leftBorder",     "rightBorder",  "topBorder"};
    for (uint32_t index = 0; index < 6; ++index) {
        output[53 + index] = owner_native_number(lua, 2, limits[index]);
    }
    damage_lua.get_field(lua, 2, "enabled");
    output[59] = damage_lua.to_boolean(lua, 3) != 0;
    damage_lua.set_top(lua, 2);
    output[60] = owner_native_number(lua, 2, "maxHorizontalLeeway");
    output[61] = owner_native_number(lua, 2, "absoluteBottom");
    damage_lua.set_top(lua, 0);
    for (uint32_t index = 0; index < 4; ++index)
        output[43 + index] = owner_native_publication[index];
    output[47] = owner_native_counts[4];
    output[48] = owner_native_counts[2];
    output[49] = owner_native_counts[3];
    output[50] = owner_native_count;
    output[51] = owner_native_publication[3] != 0;
    output[63] = owner_native_command[11];
#if defined(AB_ORACLE_CAMERA_FOLLOW_END_SEQUENCE)
    owner_end_native_snapshot(lua, output);
#endif
}

#endif
