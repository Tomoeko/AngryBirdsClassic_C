#ifndef AB_ORACLE_SLING_DELTA_H
#define AB_ORACLE_SLING_DELTA_H

static uint32_t delta_input[10], delta_events[3][6], delta_event_count;
static uint32_t delta_camera[30], delta_coordinates[4];
static float (*delta_convert)(const void *, float);
static uint32_t *delta_value;

static void delta_call(void *lua, int arguments, int results) {
    if (damage_lua.call(lua, arguments, results, 0)) {
        const char *error = damage_lua.to_string(lua, -1, NULL);
        fail(error ? error : "Original sling delta call failed\n");
    }
}

static void delta_push_vector(void *lua) {
    if (delta_value[1] != 5) {
        fail("Original sling delta upvalue is not a vector\n");
    }
    damage_lua.push_nil(lua);
    uint32_t *top = (uint32_t *)(uintptr_t)((const uint32_t *)lua)[2];
    top[-2] = delta_value[0];
    top[-1] = delta_value[1];
}

static uint32_t delta_field(void *lua, int table, const char *name) {
    damage_lua.get_field(lua, table, name);
    if (damage_lua.type(lua, -1) != 3) {
        fail("Original sling delta field is not numeric\n");
    }
    FloatBits value = {.number = damage_lua.to_number(lua, -1)};
    damage_lua.set_top(lua, damage_lua.get_top(lua) - 1);
    return value.bits;
}

static void delta_snapshot(void *lua, uint32_t output[2]) {
    int count = damage_lua.get_top(lua);
    delta_push_vector(lua);
    output[0] = delta_field(lua, -1, "x");
    output[1] = delta_field(lua, -1, "y");
    damage_lua.set_top(lua, count);
}

static uint32_t *delta_event(void *lua, uint32_t kind) {
    if (delta_event_count >= 3) {
        fail("Original sling delta boundary count exceeds storage\n");
    }
    uint32_t *event = delta_events[delta_event_count++];
    for (size_t index = 0; index < 6; ++index) {
        event[index] = 0;
    }
    event[0] = kind;
    event[5] = (uint32_t)damage_lua.get_top(lua);
    delta_snapshot(lua, event + 3);
    return event;
}

static int delta_screen_scale(void *lua) {
    if (damage_lua.get_top(lua) != 1 || damage_lua.type(lua, 1) != 3 ||
        delta_event_count >= 2) {
        fail("Unexpected original sling screen-scale boundary\n");
    }
    uint32_t *event = delta_event(lua, delta_event_count + 1);
    FloatBits input = {.number = damage_lua.to_number(lua, 1)};
    FloatBits output = {.number = delta_convert(delta_coordinates, input.number)};
    event[1] = input.bits;
    event[2] = output.bits;
    damage_lua.push_number(lua, output.number);
    return 1;
}

static int delta_shoot_range(void *lua) {
    if (damage_lua.get_top(lua) != 1 || damage_lua.type(lua, 1) != 5 ||
        delta_event_count != 2) {
        fail("Unexpected original sling shoot-range boundary\n");
    }
    uint32_t *event = delta_event(lua, 3);
    FloatBits output = {.bits = delta_input[7]};
    event[2] = output.bits;
    damage_lua.push_number(lua, output.number);
    return 1;
}

static void probe_sling_delta(void) {
    void *lua = open_sling_reference();
    delta_convert = (float (*)(const void *, float))find_function(
        "_ZN20CoordinateTransforms28screenDistanceToPhysicsScaleEf");
    delta_coordinates[3] = (uint32_t)(uintptr_t)delta_camera;
    damage_function(lua, -10002, "screenDistanceToPhysicsScale", delta_screen_scale);
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 1, "getShootRange", delta_shoot_range);
    damage_lua.set_field(lua, -10002, "slingshotSystem");
    damage_lua.get_field(lua, -10002, "SlingshotSystem");
    damage_lua.get_field(lua, 1, "updateSlingshotDeltaVector");
    const uint32_t *closure = damage_lua.to_pointer(lua, -1);
    const uint8_t *header = (const uint8_t *)closure;
    if (!closure || header[6] || header[7] != 1) {
        fail("Unexpected original sling delta closure\n");
    }
    delta_value = (uint32_t *)(uintptr_t)((uint32_t *)(uintptr_t)closure[5])[2];
    damage_lua.set_top(lua, 0);
    uint32_t random = UINT32_C(0x520de17a);
    semihost(4, "AB_ANDROID_SLING_DELTA_V1 4096\n");
    for (size_t item = 0; item < 4096; ++item) {
        for (size_t field = 0; field < 10; ++field) {
            FloatBits value = {
                .number = (float)((int32_t)(random_word(&random) % 4097) - 2048) / 16};
            delta_input[field] = value.bits;
        }
        const float zooms[] = {0.125f, 0.25f, 0.7f, 1, 1.5f, 2, -0.5f, -3};
        FloatBits zoom = {.number = zooms[item % 8]};
        delta_input[4] = zoom.bits;
        FloatBits sensitivity = {
            .number = (float)((int32_t)(random_word(&random) % 33) - 16) / 4};
        delta_input[5] = sensitivity.bits;
        FloatBits range = {.number = (float)(random_word(&random) % 129) / 16};
        delta_input[7] = range.bits;
        delta_input[8] = (uint32_t)item & 3;
        if (item < 64) {
            for (size_t field = 0; field < 4; ++field) {
                delta_input[field] = item & (4u << field) ? UINT32_C(0x80000000) : 0;
            }
        }
        delta_camera[28] = delta_input[4];
        delta_camera[26] = delta_input[6];
        delta_camera[27] = delta_input[9];
        delta_camera[29] = UINT32_C(0x7fc00000);
        damage_lua.push_boolean(lua, (delta_input[8] & 1) != 0);
        damage_lua.set_field(lua, -10002, "g_deltaBasedSlingshot");
        damage_number(lua, -10002, "g_slingshotSensitivity", delta_input[5]);
        delta_push_vector(lua);
        damage_number(lua, 1, "x", delta_input[0]);
        damage_number(lua, 1, "y", delta_input[1]);
        damage_lua.set_top(lua, 0);
        uint32_t before = delta_value[0];
        damage_lua.get_field(lua, -10002, "SlingshotSystem");
        damage_lua.get_field(lua, 1, "updateSlingshotDeltaVector");
        damage_lua.push_nil(lua);
        for (size_t field = 2; field < 4; ++field) {
            FloatBits value = {.bits = delta_input[field]};
            damage_lua.push_number(lua, value.number);
        }
        FloatBits unused = {.bits = delta_input[9]};
        damage_lua.push_number(lua, unused.number);
        delta_event_count = 0;
        delta_call(lua, 4, 1);
        uint32_t output[8] = {0};
        output[0] = (uint32_t)damage_lua.type(lua, -1);
        output[1] =
            output[0] == 5 && (uintptr_t)damage_lua.to_pointer(lua, -1) == before;
        delta_snapshot(lua, output + 2);
        damage_lua.set_top(lua, 0);
        damage_lua.push_boolean(lua, (delta_input[8] & 2) != 0);
        damage_lua.set_field(lua, -10002, "g_deltaBasedSlingshot");
        damage_lua.get_field(lua, -10002, "SlingshotSystem");
        damage_lua.get_field(lua, 1, "resetSlingshotDeltaVector");
        damage_lua.push_nil(lua);
        delta_call(lua, 1, 0);
        damage_lua.set_top(lua, 0);
        output[4] = delta_value[0] != before;
        delta_snapshot(lua, output + 5);
        output[7] = delta_event_count;
        print_words(delta_input, 10);
        print_words(output, 8);
        for (size_t index = 0; index < delta_event_count; ++index) {
            print_words(delta_events[index], 6);
        }
    }
    damage_lua.close_state(lua);
    semihost(4, "AB sling delta reference complete\n");
}

#endif
