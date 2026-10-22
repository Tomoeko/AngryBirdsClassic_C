#ifndef AB_ORACLE_CAMERA_SEQUENCE_H
#define AB_ORACLE_CAMERA_SEQUENCE_H

static void (*sequence_push_value)(void *, int);
static uint32_t sequence_flags, sequence_events[3][7];
static size_t sequence_event_count;

static void sequence_snapshot(void *lua, uint32_t output[7]) {
    int top = damage_lua.get_top(lua), table = top + 1;
    damage_lua.get_field(lua, -10002, "_ab_sequence_manager");
    const char *fields[] = {"currentSequence", "sequenceStageIndex", "timer",
                            "stageTimer", "doneCallback"};
    for (size_t field = 0; field < 5; ++field) {
        damage_lua.get_field(lua, table, fields[field]);
        int type = damage_lua.type(lua, -1);
        if (field == 0 || field == 4) {
            if (type != 0 && type != (field == 0 ? 5 : 6)) {
                fail("Unexpected original camera sequence reference field\n");
            }
            output[field == 0 ? 0 : 5] = (uint32_t)(type != 0);
        } else if (field == 3) {
            if (type != 0 && type != 3) {
                fail("Unexpected original camera stage timer\n");
            }
            output[3] = (uint32_t)type;
            FloatBits value = {.number = type ? damage_lua.to_number(lua, -1) : 0};
            output[4] = value.bits;
        } else {
            if (type != 3) {
                fail("Missing original numeric camera sequence field\n");
            }
            FloatBits value = {.number = damage_lua.to_number(lua, -1)};
            output[field] = value.bits;
        }
        damage_lua.set_top(lua, table);
    }
    output[6] = (uint32_t)sequence_event_count;
    damage_lua.set_top(lua, top);
}

static void sequence_event(void *lua, uint32_t operation) {
    if (sequence_event_count == 3) {
        fail("Original camera sequence boundary overflow\n");
    }
    uint32_t state[7];
    sequence_snapshot(lua, state);
    uint32_t *event = sequence_events[sequence_event_count++];
    event[0] = operation;
    for (size_t field = 0; field < 6; ++field) {
        event[field + 1] = state[field];
    }
}

static void sequence_set_number(void *lua, const char *field, float number) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, "_ab_sequence_manager");
    damage_lua.push_number(lua, number);
    damage_lua.set_field(lua, top + 1, field);
    damage_lua.set_top(lua, top);
}

static int sequence_print(void *lua) {
    (void)lua;
    return 0;
}

static int sequence_custom(void *lua) {
    sequence_event(lua, 1);
    if (sequence_flags & 4) {
        sequence_set_number(lua, "stageTimer", 32);
    }
    damage_lua.push_boolean(lua, (int)(sequence_flags & 1));
    return 1;
}

static int sequence_done(void *lua) {
    sequence_event(lua, 2);
    sequence_set_number(lua, "timer", 99);
    if (sequence_flags & 8) {
        sequence_set_number(lua, "stageTimer", 64);
    }
    return 0;
}

/* Camera action effects are controlled here. The original condition builder
 * runs before this boundary; no original action or scene effects are invoked. */
static int sequence_delegate(void *lua) {
    if (damage_lua.get_top(lua) != 2 || damage_lua.type(lua, 1) != 5 ||
        damage_lua.type(lua, 2) != 5) {
        fail("Unexpected controlled camera sequence action boundary\n");
    }
    damage_lua.get_field(lua, -10002, "sequence-condition");
    sequence_push_value(lua, 1);
    sequence_push_value(lua, 2);
    if (damage_lua.call(lua, 2, 0, 0)) {
        fail("Original camera condition construction failed\n");
    }
    sequence_event(lua, 0);
    if (sequence_flags & 16) {
        sequence_set_number(lua, "timer", 17);
    }
    return 0;
}

static void sequence_prepare(void *lua, const uint32_t setup[5]) {
    void (*raw_set)(void *, int, int) =
        (void (*)(void *, int, int))find_function("_Z11lua_rawsetiP9lua_Stateii");
    damage_lua.create_table(lua, 0, 10);
    const char *fields[] = {"reset", "moveToNextStage", "setCondition", "endSequence"};
    const char *names[] = {"sequence-reset", "sequence-next", "sequence-condition",
                           "sequence-end"};
    for (size_t field = 0; field < 4; ++field) {
        damage_lua.get_field(lua, -10002, names[field]);
        damage_lua.set_field(lua, 1, fields[field]);
    }
    damage_function(lua, 1, "delegateSequenceAction", sequence_delegate);
    damage_lua.set_field(lua, -10002, "_ab_sequence_manager");
    damage_lua.get_field(lua, -10002, names[0]);
    damage_lua.get_field(lua, -10002, "_ab_sequence_manager");
    if (damage_lua.call(lua, 1, 0, 0)) {
        fail("Original camera manager reset failed\n");
    }
    damage_lua.create_table(lua, (int)setup[0], 0);
    for (size_t index = 0; index < setup[0]; ++index) {
        damage_lua.create_table(lua, 0, 4);
        damage_lua.push_boolean(lua, (int)((setup[4] >> index) & 1));
        damage_lua.set_field(lua, 2, "interruptOnUserInput");
        if (index == 0 && setup[1]) {
            damage_number(lua, 2, "duration", setup[2]);
        } else if (index == 1) {
            damage_lua.push_string(lua, "noMovingObjects");
            damage_lua.set_field(lua, 2, "condition");
            damage_number(lua, 2, "minDuration", setup[3]);
        } else if (index == 2) {
            damage_lua.push_string(lua, "untilSpecialtyTriggered");
            damage_lua.set_field(lua, 2, "condition");
        } else if (index == 3) {
            damage_function(lua, 2, "condition", sequence_custom);
        }
        raw_set(lua, 1, (int)index + 1);
    }
    damage_lua.set_field(lua, -10002, "_ab_sequence_source");
}

static void sequence_command(void *lua, const uint32_t input[6]) {
    sequence_flags = input[5];
    sequence_event_count = 0;
    damage_lua.push_boolean(lua, (int)input[3]);
    damage_lua.set_field(lua, -10002, "hasMovingObjects");
    damage_lua.push_boolean(lua, (int)input[4]);
    damage_lua.set_field(lua, -10002, "birdSpecialtyAvailable");
    const char *name = input[0] == 0   ? "sequence-reset"
                       : input[0] == 1 ? "sequence-start"
                       : input[0] == 2 ? "sequence-update"
                                       : "sequence-end";
    damage_lua.get_field(lua, -10002, name);
    damage_lua.get_field(lua, -10002, "_ab_sequence_manager");
    int arguments = 1;
    if (input[0] == 1) {
        damage_lua.get_field(lua, -10002, "_ab_sequence_source");
        damage_lua.push_nil(lua);
        if (sequence_flags & 2) {
            damage_lua.push_closure(lua, sequence_done, 0);
        } else {
            damage_lua.push_nil(lua);
        }
        arguments = 4;
    } else if (input[0] == 2) {
        FloatBits seconds = {.bits = input[1]};
        damage_lua.push_number(lua, seconds.number);
        damage_lua.push_boolean(lua, (int)input[2]);
        arguments = 3;
    }
    if (damage_lua.call(lua, arguments, 0, 0)) {
        fail("Original camera sequence command failed\n");
    }
    uint32_t state[7];
    sequence_snapshot(lua, state);
    print_words(state, 7);
    for (size_t event = 0; event < sequence_event_count; ++event) {
        print_words(sequence_events[event], 7);
    }
}

static void probe_camera_sequence(void) {
    initialize_damage_lua();
    sequence_push_value =
        (void (*)(void *, int))find_function("_Z13lua_pushvalueP9lua_Statei");
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original camera sequence state\n");
    }
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    damage_function(lua, -10002, "print", sequence_print);
    const char *names[] = {"sequence-reset", "sequence-start",     "sequence-update",
                           "sequence-next",  "sequence-condition", "sequence-end"};
    const char *paths[] = {"/data/local/tmp/ab520/camera/sequence-reset-oracle.lua",
                           "/data/local/tmp/ab520/camera/sequence-start-oracle.lua",
                           "/data/local/tmp/ab520/camera/sequence-update-oracle.lua",
                           "/data/local/tmp/ab520/camera/sequence-next-oracle.lua",
                           "/data/local/tmp/ab520/camera/sequence-condition-oracle.lua",
                           "/data/local/tmp/ab520/camera/sequence-end-oracle.lua"};
    for (size_t index = 0; index < 6; ++index) {
        load_selected_function(lua, paths[index], names[index]);
    }
    uint32_t random = UINT32_C(0x5205e9ce);
    semihost(4, "AB_ANDROID_CAMERA_SEQUENCE_V1 128 64\n");
    for (size_t item = 0; item < 128; ++item) {
        FloatBits duration = {.number = item < 16 ? 0 : (float)(item % 5) * 0.25f};
        FloatBits minimum = {.number = 0.25f};
        uint32_t setup[5] = {(uint32_t)item % 5, (uint32_t)item % 3 ? 1u : 0u,
                             duration.bits, minimum.bits, (uint32_t)(item / 5) % 16};
        print_words(setup, 5);
        sequence_prepare(lua, setup);
        for (size_t step = 0; step < 64; ++step) {
            uint32_t operation = random_word(&random) % 6;
            FloatBits seconds = {
                .number = (float)((int)(random_word(&random) % 10) - 2) * 0.125f};
            uint32_t input[6] = {
                operation < 4 ? operation : 2, seconds.bits,
                random_word(&random) % 2,      random_word(&random) % 2,
                random_word(&random) % 2,      random_word(&random) % 32};
            if (step < 13) {
                input[0] = step == 0 || step == 10 ? 1u : step == 12 ? 3u : 2u;
                input[2] = step == 11;
                input[3] = step == 3;
                input[4] = step != 7;
                input[5] = 2u | (step == 9 ? 1u : 0u) | (step == 8 ? 4u : 0u);
                seconds.number = step == 1 || step >= 6   ? 0
                                 : step == 2 || step == 5 ? 0.125f
                                 : step == 3              ? 0.5f
                                                          : 0.25f;
                if (step == 11 && setup[4] & 1) {
                    seconds.bits = UINT32_C(0x7fc00000);
                }
                input[1] = seconds.bits;
            }
            print_words(input, 6);
            sequence_command(lua, input);
        }
    }
    damage_lua.close_state(lua);
    semihost(4, "AB camera sequence reference complete\n");
}

#endif
