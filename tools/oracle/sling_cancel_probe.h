#ifndef AB_ORACLE_SLING_CANCEL_H
#define AB_ORACLE_SLING_CANCEL_H

static uint32_t cancel_input[12], cancel_events[2][14];
static uint32_t *cancel_dragged, *cancel_selection, *cancel_reset_values[7];
static size_t cancel_event_count;

static void cancel_call(void *lua, int arguments, int results) {
    if (damage_lua.call(lua, arguments, results, 0)) {
        const char *error = damage_lua.to_string(lua, -1, NULL);
        fail(error ? error : "Original sling cancellation failed\n");
    }
}

static uint32_t cancel_number(void *lua, int table, const char *field) {
    damage_lua.get_field(lua, table, field);
    if (damage_lua.type(lua, -1) != 3) {
        fail("Original cancel field is not numeric\n");
    }
    FloatBits result = {.number = damage_lua.to_number(lua, -1)};
    damage_lua.set_top(lua, damage_lua.get_top(lua) - 1);
    return result.bits;
}

static void cancel_snapshot(void *lua, uint32_t output[11]) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, "SlingshotSystem");
    damage_lua.get_field(lua, -1, "getRubberBandPosition");
    damage_lua.get_field(lua, -10002, "_ab_cancel_self");
    cancel_call(lua, 1, 1);
    output[0] = cancel_number(lua, -1, "x");
    output[1] = cancel_number(lua, -1, "y");
    damage_lua.set_top(lua, top);
    for (size_t field = 0; field < 4; ++field) {
        if (cancel_reset_values[field + 2][1] != 3) {
            fail("Original reset scalar changed type\n");
        }
        output[field + 2] = cancel_reset_values[field + 2][0];
    }
    if (cancel_selection[1] != 1 || cancel_reset_values[6][1] != 1) {
        fail("Original cancel boolean changed type\n");
    }
    output[6] = cancel_selection[0];
    output[7] = cancel_reset_values[6][0];
    output[8] = cancel_dragged[1] == 5;
    damage_lua.get_field(lua, -10002, "_ab_cancel_bird");
    output[9] = cancel_number(lua, -1, "x");
    output[10] = cancel_number(lua, -1, "y");
    damage_lua.set_top(lua, top);
}

static uint32_t *cancel_event(void *lua, uint32_t operation) {
    if (cancel_event_count >= 2) {
        fail("Unexpected original cancellation boundary count\n");
    }
    uint32_t *event = cancel_events[cancel_event_count++];
    event[0] = operation;
    event[1] = event[2] = 0;
    cancel_snapshot(lua, event + 3);
    return event;
}

static int cancel_position(void *lua) {
    if (damage_lua.get_top(lua) != 3 || !damage_named(lua, 1, "first")) {
        fail("Unexpected original cancellation position boundary\n");
    }
    uint32_t *event = cancel_event(lua, 1);
    for (int argument = 2; argument <= 3; ++argument) {
        FloatBits coordinate = {.number = damage_lua.to_number(lua, argument)};
        event[(size_t)argument - 1] = coordinate.bits;
    }
    damage_lua.get_field(lua, -10002, "_ab_cancel_bird");
    damage_number(lua, -2, "x", event[1]);
    damage_number(lua, -2, "y", event[2]);
    return 0;
}

static int cancel_audio_query(void *lua) {
    if (damage_lua.get_top(lua) != 1 || !damage_named(lua, 1, "laser_sight")) {
        fail("Unexpected original cancellation audio query\n");
    }
    (void)cancel_event(lua, 2);
    damage_lua.push_boolean(lua, (int)cancel_input[11]);
    return 1;
}

static void cancel_root_vector(void *lua, const char *getter, const uint32_t bits[2]) {
    damage_lua.get_field(lua, -10002, "SlingshotSystem");
    damage_lua.get_field(lua, 1, getter);
    damage_lua.get_field(lua, -10002, "_ab_cancel_self");
    cancel_call(lua, 1, 1);
    damage_number(lua, 2, "x", bits[0]);
    damage_number(lua, 2, "y", bits[1]);
    damage_lua.set_top(lua, 0);
}

static void prepare_cancel_bindings(void *lua) {
    damage_function(lua, -10002, "setPosition", cancel_position);
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 1, "isAudioPlaying", cancel_audio_query);
    damage_lua.set_field(lua, -10002, "res");
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 1, "stopMovement", damage_require);
    damage_lua.set_field(lua, -10002, "cameraSystem");
    damage_lua.get_field(lua, -10002, "SlingshotSystem");
    damage_lua.get_field(lua, 1, "cancelBirdDrag");
    const uint32_t *closure = damage_lua.to_pointer(lua, -1);
    const uint8_t *header = (const uint8_t *)closure;
    if (!closure || header[6] || header[7] != 2) {
        fail("Original cancel closure layout mismatch\n");
    }
    cancel_dragged = (uint32_t *)(uintptr_t)((uint32_t *)(uintptr_t)closure[5])[2];
    cancel_selection = (uint32_t *)(uintptr_t)((uint32_t *)(uintptr_t)closure[6])[2];
    damage_lua.set_top(lua, 1);
    damage_lua.get_field(lua, 1, "resetRubberBand");
    closure = damage_lua.to_pointer(lua, -1);
    header = (const uint8_t *)closure;
    if (!closure || header[6] || header[7] != 7) {
        fail("Original band-reset closure layout mismatch\n");
    }
    for (size_t field = 0; field < 7; ++field) {
        cancel_reset_values[field] =
            (uint32_t *)(uintptr_t)((uint32_t *)(uintptr_t)closure[field + 5])[2];
    }
    damage_lua.set_top(lua, 1);
    damage_lua.create_table(lua, 0, 2);
    const char *methods[] = {"getLevelStartPosition", "resetRubberBand"};
    for (size_t field = 0; field < 2; ++field) {
        damage_lua.get_field(lua, 1, methods[field]);
        damage_lua.set_field(lua, 2, methods[field]);
    }
    damage_lua.set_field(lua, -10002, "_ab_cancel_self");
    damage_lua.set_top(lua, 0);
}

static void prepare_cancel_case(void *lua) {
    cancel_root_vector(lua, "getLevelStartPosition", cancel_input);
    cancel_root_vector(lua, "getRubberBandPosition", cancel_input + 2);
    for (size_t field = 0; field < 4; ++field) {
        cancel_reset_values[field + 2][0] = cancel_input[field + 4];
        cancel_reset_values[field + 2][1] = 3;
    }
    cancel_reset_values[6][0] = cancel_input[9];
    cancel_reset_values[6][1] = 1;
    cancel_selection[0] = cancel_input[8];
    cancel_selection[1] = 1;
    damage_lua.create_table(lua, 0, 3);
    damage_lua.push_string(lua, "first");
    damage_lua.set_field(lua, 1, "name");
    damage_number(lua, 1, "x", cancel_input[2]);
    damage_number(lua, 1, "y", cancel_input[3]);
    damage_lua.set_field(lua, -10002, "_ab_cancel_bird");
    damage_lua.get_field(lua, -10002, "SlingshotSystem");
    damage_lua.get_field(lua, 1, "setDraggedBird");
    damage_lua.get_field(lua, -10002, "_ab_cancel_self");
    if (cancel_input[10]) {
        damage_lua.get_field(lua, -10002, "_ab_cancel_bird");
    } else {
        damage_lua.push_nil(lua);
    }
    cancel_call(lua, 2, 0);
    damage_lua.set_top(lua, 0);
}

static void probe_sling_cancel(void) {
    void *lua = open_sling_reference();
    prepare_cancel_bindings(lua);
    uint32_t state = UINT32_C(0x520ca51e);
    semihost(4, "AB_ANDROID_SLING_CANCEL_V1 4096\n");
    for (size_t item = 0; item < 4096; ++item) {
        for (size_t field = 0; field < 8; ++field) {
            cancel_input[field] = random_float(&state, 122, 8, 1);
        }
        for (size_t field = 8; field < 12; ++field) {
            cancel_input[field] = (uint32_t)(item >> (field - 8)) & 1;
        }
        if (item < 32) {
            for (size_t field = 0; field < 8; ++field) {
                cancel_input[field] = item & 16 ? UINT32_C(0x80000000) : 0;
            }
        }
        prepare_cancel_case(lua);
        print_words(cancel_input, 12);
        cancel_event_count = 0;
        damage_lua.get_field(lua, -10002, "SlingshotSystem");
        damage_lua.get_field(lua, 1, "cancelBirdDrag");
        damage_lua.get_field(lua, -10002, "_ab_cancel_self");
        cancel_call(lua, 1, 0);
        damage_lua.set_top(lua, 0);
        uint32_t output[11];
        cancel_snapshot(lua, output);
        print_words(output, 11);
        uint32_t count = (uint32_t)cancel_event_count;
        print_words(&count, 1);
        for (size_t event = 0; event < cancel_event_count; ++event) {
            print_words(cancel_events[event], 14);
        }
    }
    damage_lua.close_state(lua);
    semihost(4, "AB sling cancel reference complete\n");
}

#endif
