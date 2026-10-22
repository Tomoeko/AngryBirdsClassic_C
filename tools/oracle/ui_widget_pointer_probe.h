#ifndef AB_ORACLE_UI_WIDGET_POINTER_H
#define AB_ORACLE_UI_WIDGET_POINTER_H

static int ui_widget_pointer_identity(void *lua) {
    (void)lua;
    fail("Unexpected invocation of pointer identity value\n");
}

static void ui_widget_pointer_random(uint32_t *random, uint32_t output[2]) {
    output[0] = (random_word(random) >> 16) % 6;
    output[1] = random_word(random);
    if (output[0] == 0 || output[0] == 5) {
        output[1] = 0;
    } else if (output[0] == 1) {
        output[1] &= 1;
    } else if (output[0] == 3) {
        output[1] = (output[1] >> 16) % 5;
    } else if (output[0] == 4) {
        output[1] = (output[1] >> 16) % 31 + 1;
    }
}

static void ui_widget_pointer_prepare(void *lua, const uint32_t flags[10],
                                      size_t step) {
    ui_button_prepare(lua);
    damage_lua.get_field(lua, -10002, "_ab_button");
    const char *fields[] = {"visible", "image", "activateOnRelease", "clickStarted",
                            "shouldClearPointerFlags"};
    size_t count = step % 7 == 0 ? 5 : 3;
    for (size_t field = 0; field < count; ++field) {
        ui_button_push(lua, flags[field * 2], flags[field * 2 + 1]);
        damage_lua.set_field(lua, 1, fields[field]);
    }
    damage_lua.set_top(lua, 0);
}

static void ui_widget_pointer_run(void *lua) {
    const char *functions[] = {"button-handle", "button-pointer", "button-flags",
                               "button-events"};
    uint32_t operation = ui_button_command[0];
    ui_button_hit_count = ui_button_call_count = 0;
    damage_lua.get_field(lua, -10002, functions[operation]);
    damage_lua.get_field(lua, -10002, "_ab_button");
    if (operation < 2) {
        damage_lua.push_string(lua, ui_button_events[ui_button_command[3]]);
        for (size_t axis = 0; axis < 2; ++axis) {
            FloatBits point = {.bits = ui_button_command[7 + axis]};
            damage_lua.push_number(lua, point.number);
        }
    }
    if (damage_lua.call(lua, operation < 2 ? 4 : 1, 3, 0)) {
        const char *message = damage_lua.to_string(lua, -1, NULL);
        fail(message ? message : "Original typed button pointer execution failed\n");
    }
    uint32_t record[11];
    for (size_t field = 0; field < 3; ++field) {
        ui_button_value(lua, (int)field + 1, record + field * 2);
    }
    ui_button_raw_flags(lua, record + 6);
    record[10] = ui_button_call_count;
    print_words(record, 11);
    for (uint32_t index = 0; index < ui_button_call_count; ++index) {
        print_words(ui_button_calls[index], 10);
    }
    damage_lua.set_top(lua, 0);
}

static void probe_ui_widget_pointer(void) {
    void *lua = ui_button_initialize();
    damage_lua.push_closure(lua, ui_widget_pointer_identity, 0);
    damage_lua.set_field(lua, -10002, "_ab_button_identity_function");
    uint32_t random = UINT32_C(0x520b1781);
    semihost(4, "AB_ANDROID_UI_BUTTON_STATE_V1 64 128\n");
    for (uint32_t configuration = 0; configuration < 64; ++configuration) {
        print_words(&configuration, 1);
        ui_button_state(lua, configuration);
        for (size_t step = 0; step < 128; ++step) {
            ui_button_command[0] = step % 13 == 11   ? 2u
                                   : step % 13 == 12 ? 3u
                                                     : (uint32_t)(step & 1);
            ui_button_command[1] = (random_word(&random) >> 16) & 15;
            FloatBits count = {.number = (float)((random_word(&random) >> 16) % 7)};
            ui_button_command[2] = count.bits;
            ui_button_command[3] = (random_word(&random) >> 16) % 6;
            ui_button_command[4] = (random_word(&random) >> 16) & 7;
            for (size_t field = 5; field < 9; ++field) {
                ui_button_command[field] = random_float(&random, 115, 22, 1);
            }
            for (size_t field = 0; field < 5; ++field) {
                ui_widget_pointer_random(&random, ui_button_command + 9 + field * 2);
            }
            uint32_t flags[10];
            for (size_t field = 0; field < 5; ++field) {
                ui_widget_pointer_random(&random, flags + field * 2);
            }
            for (size_t child = 0; child < 2; ++child) {
                ui_button_children[child][0] = (random_word(&random) >> 16) & 3;
                for (size_t field = 0; field < 3; ++field) {
                    ui_widget_pointer_random(&random,
                                             ui_button_children[child] + 1 + field * 2);
                }
            }
            print_words(ui_button_command, 19);
            print_words(flags, 10);
            for (size_t child = 0; child < 2; ++child) {
                print_words(ui_button_children[child], 7);
            }
            ui_widget_pointer_prepare(lua, flags, step);
            ui_widget_pointer_run(lua);
        }
    }
    damage_lua.close_state(lua);
    semihost(4, "AB UI button state reference complete\n");
}

#endif
