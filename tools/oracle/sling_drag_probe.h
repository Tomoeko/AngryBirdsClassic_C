#ifndef AB_ORACLE_SLING_DRAG_H
#define AB_ORACLE_SLING_DRAG_H

static uint32_t drag_input[9], drag_events[3];
static uint32_t *drag_values[8];
static size_t drag_event_count;

static void drag_record(uint32_t event) {
    if (drag_event_count >= 3) {
        fail("Unexpected original drag audio boundary count\n");
    }
    drag_events[drag_event_count++] = event;
}

static int drag_audio_query(void *lua) {
    if (!damage_named(lua, 1, "slingshot_stretched")) {
        fail("Unexpected original drag audio query\n");
    }
    drag_record(1);
    damage_lua.push_boolean(lua, (int)drag_input[8]);
    return 1;
}

static int drag_audio_play(void *lua) {
    if (damage_lua.get_top(lua) != 3 || damage_lua.type(lua, 2) != 3 ||
        damage_lua.to_number(lua, 2) != 1 || damage_lua.type(lua, 3) != 1 ||
        damage_lua.to_boolean(lua, 3)) {
        fail("Unexpected original drag audio arguments\n");
    }
    if (damage_named(lua, 1, "slingshot_stretched")) {
        drag_record(2);
    } else if (damage_named(lua, 1, "selection")) {
        drag_record(3);
    } else {
        fail("Unexpected original drag sound\n");
    }
    return 0;
}

static int drag_selection_definition(void *lua) {
    if (!damage_named(lua, 1, "first")) {
        fail("Unexpected original drag bird definition\n");
    }
    damage_lua.create_table(lua, 0, 1);
    damage_lua.push_string(lua, "selection");
    damage_lua.set_field(lua, -2, "selectionSound");
    return 1;
}

static int drag_audio_name(void *lua) {
    if (!damage_named(lua, 1, "selection")) {
        fail("Unexpected original drag selection sound\n");
    }
    damage_lua.push_string(lua, "selection");
    return 1;
}

static void drag_call(void *lua, int arguments, int results) {
    if (damage_lua.call(lua, arguments, results, 0)) {
        const char *error = damage_lua.to_string(lua, -1, NULL);
        fail(error ? error : "Original drag call failed\n");
    }
}

static void prepare_drag_bindings(void *lua) {
    damage_function(lua, -10002, "playAudio", drag_audio_play);
    damage_function(lua, -10002, "getAudioName", drag_audio_name);
    damage_function(lua, -10002, "getObjectDefinition", drag_selection_definition);
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, -2, "isAudioPlaying", drag_audio_query);
    damage_lua.set_field(lua, -10002, "res");
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, -2, "stopMovement", damage_require);
    damage_lua.set_field(lua, -10002, "cameraSystem");

    damage_lua.create_table(lua, 0, 2);
    damage_lua.get_field(lua, -10002, "Slingshot");
    damage_lua.get_field(lua, 2, "stretchRubberBand");
    const uint32_t *closure = damage_lua.to_pointer(lua, -1);
    const uint8_t *header = (const uint8_t *)closure;
    if (!closure || damage_lua.type(lua, -1) != 6 || header[6] || header[7] != 8) {
        fail("Original drag closure layout mismatch\n");
    }
    for (size_t index = 0; index < 8; ++index) {
        uint32_t *upvalue = (uint32_t *)(uintptr_t)closure[5 + index];
        drag_values[index] = (uint32_t *)(uintptr_t)upvalue[2];
    }
    damage_lua.set_field(lua, 1, "stretchRubberBand");
    damage_lua.get_field(lua, 2, "rubberBandStretchabilityFactor");
    damage_lua.set_field(lua, 1, "rubberBandStretchabilityFactor");
    damage_lua.set_top(lua, 1);
    damage_lua.set_field(lua, -10002, "_ab_drag_sling");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.get_field(lua, -10002, "_ab_drag_sling");
    damage_lua.set_field(lua, 1, "slingshot");
    damage_lua.set_field(lua, -10002, "_ab_drag_self");

    damage_lua.get_field(lua, -10002, "SlingshotSystem");
    damage_lua.get_field(lua, 1, "setDraggedBird");
    damage_lua.get_field(lua, -10002, "_ab_drag_self");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.push_string(lua, "first");
    damage_lua.set_field(lua, -2, "name");
    drag_call(lua, 2, 0);
    damage_lua.set_top(lua, 0);
}

static void prepare_drag_case(void *lua) {
    damage_lua.get_field(lua, -10002, "SlingshotSystem");
    damage_lua.get_field(lua, 1, "getLevelStartPosition");
    damage_lua.get_field(lua, -10002, "_ab_drag_self");
    drag_call(lua, 1, 1);
    damage_number(lua, 2, "x", drag_input[0]);
    damage_number(lua, 2, "y", drag_input[1]);
    damage_lua.set_top(lua, 0);
    /* Seed only selected numeric/boolean UpVal values. Original closure code,
     * helpers, vector allocation and factor policy execute unchanged. Numeric
     * writes need no GC barrier; the dragged object uses the original setter. */
    const size_t slots[] = {2, 4, 7};
    const size_t inputs[] = {4, 5, 6};
    for (size_t index = 0; index < 3; ++index) {
        drag_values[slots[index]][0] = drag_input[inputs[index]];
        drag_values[slots[index]][1] = 3;
    }
    drag_values[5][0] = drag_input[7];
    drag_values[5][1] = 1;
}

static uint32_t drag_number(void *lua, int table, const char *field) {
    damage_lua.get_field(lua, table, field);
    if (damage_lua.type(lua, -1) != 3) {
        fail("Original drag result is not numeric\n");
    }
    FloatBits result = {.number = damage_lua.to_number(lua, -1)};
    damage_lua.set_top(lua, damage_lua.get_top(lua) - 1);
    return result.bits;
}

static void print_drag_result(void *lua) {
    uint32_t result[8];
    damage_lua.get_field(lua, -10002, "SlingshotSystem");
    damage_lua.get_field(lua, 1, "getRubberBandPosition");
    damage_lua.get_field(lua, -10002, "_ab_drag_self");
    drag_call(lua, 1, 1);
    result[0] = drag_number(lua, 2, "x");
    result[1] = drag_number(lua, 2, "y");
    damage_lua.set_top(lua, 1);
    damage_lua.get_field(lua, 1, "getStretchFactor");
    damage_lua.get_field(lua, -10002, "_ab_drag_self");
    drag_call(lua, 1, 1);
    if (damage_lua.type(lua, 2) != 3 || drag_values[1][1] != 3 ||
        drag_values[4][1] != 3 || drag_values[7][1] != 3 || drag_values[5][1] != 1) {
        fail("Original drag state result types mismatch\n");
    }
    FloatBits distance = {.number = damage_lua.to_number(lua, 2)};
    result[2] = drag_values[1][0];
    result[3] = distance.bits;
    result[4] = drag_values[4][0];
    result[5] = drag_values[7][0];
    result[6] = drag_values[5][0];
    result[7] = (uint32_t)drag_event_count;
    damage_lua.set_top(lua, 0);
    print_words(result, 8);
    for (size_t event = 0; event < drag_event_count; ++event) {
        print_words(drag_events + event, 1);
    }
}

static void probe_sling_drag(void) {
    void *lua = open_sling_reference();
    prepare_drag_bindings(lua);
    uint32_t state = UINT32_C(0x52051ad9);
    semihost(4, "AB_ANDROID_SLING_DRAG_V1 4096\n");
    for (size_t index = 0; index < 4096; ++index) {
        for (size_t field = 0; field < 4; ++field) {
            drag_input[field] = random_float(&state, 122, 10, 1);
        }
        drag_input[4] = random_float(&state, 124, 5, 0);
        drag_input[5] = random_float(&state, 123, 5, 0);
        drag_input[6] = random_float(&state, 122, 6, 1);
        drag_input[7] = (uint32_t)(index & 1);
        drag_input[8] = (uint32_t)((index >> 1) & 1);
        if (index < 32) {
            drag_input[0] = drag_input[1] = 0;
            drag_input[2] = index & 4 ? UINT32_C(0x80000000) : 0;
            drag_input[3] = index & 8 ? UINT32_C(0x80000000) : 0;
        } else if (index < 64) {
            drag_input[2] = index & 4 ? drag_input[4] : 0;
            drag_input[3] = index & 4 ? 0 : drag_input[4];
            if (index & 8) {
                drag_input[2] ^= UINT32_C(0x80000000);
                drag_input[3] ^= UINT32_C(0x80000000);
            }
        } else if (index < 512) {
            FloatBits horizontal = {.number = ((float)(index - 64) / 447.0f) - 0.5f};
            drag_input[2] = horizontal.bits;
            drag_input[3] = UINT32_C(0xbf800000);
            drag_input[4] = UINT32_C(0x40000000);
        }
        prepare_drag_case(lua);
        print_words(drag_input, 9);
        drag_event_count = 0;
        damage_lua.get_field(lua, -10002, "SlingshotSystem");
        damage_lua.get_field(lua, 1, "stretchRubberBand");
        damage_lua.get_field(lua, -10002, "_ab_drag_self");
        damage_lua.create_table(lua, 0, 2);
        damage_number(lua, -2, "x", drag_input[2]);
        damage_number(lua, -2, "y", drag_input[3]);
        drag_call(lua, 2, 0);
        damage_lua.set_top(lua, 0);
        print_drag_result(lua);
    }
    damage_lua.close_state(lua);
    semihost(4, "AB sling drag reference complete\n");
}

#endif
