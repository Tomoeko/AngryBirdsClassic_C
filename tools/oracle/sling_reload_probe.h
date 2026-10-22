#ifndef AB_ORACLE_SLING_RELOAD_H
#define AB_ORACLE_SLING_RELOAD_H

static uint32_t reload_input[5];
static uint32_t *reload_launch_cue;

static int reload_next_bird(void *lua) {
    if (transfer_event_count >= 5 || damage_lua.get_top(lua) != 1) {
        fail("Unexpected original reload next-bird boundary\n");
    }
    FloatBits index = {.number = damage_lua.to_number(lua, 1)};
    uint32_t *event = transfer_events[transfer_event_count++];
    event[0] = 4;
    event[1] = index.bits;
    event[2] = 0;
    if (reload_input[4] == 0) {
        damage_lua.push_nil(lua);
    } else if (reload_input[4] == 1) {
        damage_lua.push_boolean(lua, 0);
    } else {
        damage_lua.push_string(lua, "first");
    }
    return 1;
}

static uint32_t reload_scalar(void *lua, int table, const char *field, int tag) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, table, field);
    if (damage_lua.type(lua, -1) != tag) {
        fail("Original reload field changed type\n");
    }
    FloatBits value = {.number = damage_lua.to_number(lua, -1)};
    uint32_t output = tag == 1 ? (uint32_t)damage_lua.to_boolean(lua, -1) : value.bits;
    damage_lua.set_top(lua, top);
    return output;
}

static void probe_sling_reload(void) {
    void *lua = open_sling_reference();
    prepare_transfer_boundaries(lua);
    damage_function(lua, -10002, "getNextBird", reload_next_bird);
    damage_lua.get_field(lua, -10002, "SlingshotSystem");
    damage_lua.get_field(lua, 1, "defaultAnimateBirdToSlingShot");
    const uint32_t *closure = damage_lua.to_pointer(lua, -1);
    if (!closure || ((const uint8_t *)closure)[6] ||
        ((const uint8_t *)closure)[7] != 1) {
        fail("Original reload closure layout mismatch\n");
    }
    reload_launch_cue = (uint32_t *)(uintptr_t)((uint32_t *)(uintptr_t)closure[5])[2];
    damage_lua.set_top(lua, 0);
    uint32_t random = UINT32_C(0x520ae10d);
    semihost(4, "AB_ANDROID_SLING_RELOAD_V1 4096\n");
    for (size_t item = 0; item < 4096; ++item) {
        FloatBits timer = {.number = (float)((int)(item % 25) - 8) / 100.0f};
        FloatBits seconds = {.number = (float)(item % 17) / 100.0f};
        reload_input[0] = timer.bits;
        reload_input[1] = seconds.bits;
        reload_input[2] = random_float(&random, 120, 10, 1);
        reload_input[3] = (uint32_t)item & 7;
        reload_input[4] = (uint32_t)(item / 8) % 3;
        if (item < 32) {
            reload_input[0] = item & 16 ? UINT32_C(0x80000000) : 0;
        } else if (item % 7 == 0) {
            reload_input[0] = reload_input[1];
        }
        const uint32_t fixed[] = {0x40000000, 0x40400000, 0xc0800000,
                                  0xc0a00000, 0xbf800000, 0x4016cbe4,
                                  0x3e800000, 0x3f400000, 0};
        for (size_t field = 0; field < 9; ++field) {
            transfer_input[field] = fixed[field];
        }
        transfer_input[8] = reload_input[1];
        prepare_transfer_frame(lua);
        damage_lua.get_field(lua, -10002, "_ab_sling_self");
        damage_number(lua, 1, "nextBirdTimer", reload_input[0]);
        damage_lua.set_top(lua, 0);
        damage_number(lua, -10002, "currentBirdIndex", reload_input[2]);
        const char *names[] = {"birdFired", "fillInNextBird", "allowResetToBirdCamera"};
        for (size_t field = 0; field < 3; ++field) {
            damage_lua.push_boolean(lua, (reload_input[3] & (1u << field)) != 0);
            damage_lua.set_field(lua, -10002, names[field]);
        }
        reload_launch_cue[0] = UINT32_C(0x42280000);
        reload_launch_cue[1] = 3;
        transfer_event_count = 0;
        print_words(reload_input, 5);
        damage_lua.get_field(lua, -10002, "SlingshotSystem");
        damage_lua.get_field(lua, 1, "defaultAnimateBirdToSlingShot");
        damage_lua.get_field(lua, -10002, "_ab_sling_self");
        damage_lua.push_number(lua, seconds.number);
        if (damage_lua.call(lua, 2, 0, 0)) {
            const char *error = damage_lua.to_string(lua, -1, NULL);
            fail(error ? error : "Original reload update failed\n");
        }
        damage_lua.set_top(lua, 0);
        damage_lua.get_field(lua, -10002, "_ab_sling_self");
        uint32_t output[7];
        output[0] = reload_scalar(lua, 1, "nextBirdTimer", 3);
        for (size_t field = 0; field < 3; ++field) {
            output[field + 1] = reload_scalar(lua, -10002, names[field], 1);
        }
        output[4] = reload_launch_cue[1];
        output[5] = output[4] ? reload_launch_cue[0] : 0;
        damage_lua.get_field(lua, 1, "birdReloadAnimation");
        output[6] = reload_scalar(lua, 2, "timer", 3);
        damage_lua.set_top(lua, 0);
        print_words(output, 7);
        uint32_t count = (uint32_t)transfer_event_count;
        print_words(&count, 1);
        for (size_t event = 0; event < transfer_event_count; ++event) {
            print_words(transfer_events[event], 3);
        }
    }
    damage_lua.close_state(lua);
    semihost(4, "AB sling reload reference complete\n");
}

#endif
