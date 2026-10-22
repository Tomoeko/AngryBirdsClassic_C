#ifndef AB_ORACLE_UI_IMAGE_BUTTON_H
#define AB_ORACLE_UI_IMAGE_BUTTON_H

static void (*ui_button_raw_set)(void *, int, int);
static uint32_t ui_button_command[19], ui_button_children[2][7];
static uint32_t ui_button_calls[8][10], ui_button_call_count, ui_button_hit_count;
static const char *const ui_button_events[] = {"LPRESS", "LHOLD", "LRELEASE",
                                               "WHEEL",  "HOVER", "CUSTOM"};
static const char *const ui_button_texts[] = {"", "CLICK", "HOVER", "reply",
                                              "menu_confirm"};

static void ui_button_push(void *lua, uint32_t kind, uint32_t payload) {
    switch (kind) {
        case 0:
            damage_lua.push_nil(lua);
            break;
        case 1:
            damage_lua.push_boolean(lua, (int)payload);
            break;
        case 2: {
            FloatBits number = {.bits = payload};
            damage_lua.push_number(lua, number.number);
            break;
        }
        case 3:
            damage_lua.push_string(lua, ui_button_texts[payload]);
            break;
        case 4:
            damage_lua.create_table(lua, 0, 1);
            damage_lua.push_number(lua, (float)payload);
            damage_lua.set_field(lua, -2, "_ab_reply_object");
            break;
        case 5:
            damage_lua.get_field(lua, -10002, "_ab_button_identity_function");
            break;
        default:
            fail("Unsupported button value fixture\n");
    }
}

static void ui_button_value(void *lua, int index, uint32_t output[2]) {
    int top = damage_lua.get_top(lua);
    int type = damage_lua.type(lua, index);
    output[0] = output[1] = 0;
    if (type == 1) {
        output[0] = 1;
        output[1] = (uint32_t)damage_lua.to_boolean(lua, index);
    } else if (type == 3) {
        output[0] = 2;
        FloatBits value = {.number = damage_lua.to_number(lua, index)};
        output[1] = value.bits;
    } else if (type == 4) {
        output[0] = 3;
        output[1] = 5;
        for (uint32_t text = 0; text < 5; ++text) {
            if (damage_named(lua, index, ui_button_texts[text])) {
                output[1] = text;
            }
        }
        if (output[1] == 5) {
            fail("Unexpected original button reply text\n");
        }
    } else if (type == 5) {
        output[0] = 4;
        damage_lua.get_field(lua, index, "_ab_reply_object");
        output[1] = (uint32_t)damage_lua.to_number(lua, -1);
    } else if (type == 6) {
        output[0] = 5;
    } else if (type != 0) {
        fail("Unexpected original button reply type\n");
    }
    damage_lua.set_top(lua, top);
}

#if defined(AB_ORACLE_UI_BUTTON_STATE)
static void ui_button_raw_flags(void *lua, uint32_t output[4]) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, "_ab_button");
    int self = top + 1;
    damage_lua.get_field(lua, self, "clickStarted");
    ui_button_value(lua, self + 1, output);
    damage_lua.set_top(lua, self);
    damage_lua.get_field(lua, self, "shouldClearPointerFlags");
    ui_button_value(lua, self + 1, output + 2);
    damage_lua.set_top(lua, top);
}
#endif

static uint32_t *ui_button_boundary(void *lua, uint32_t kind, uint32_t index, float x,
                                    float y) {
    if (ui_button_call_count >= 8) {
        fail("Original button boundary overflow\n");
    }
    uint32_t *call = ui_button_calls[ui_button_call_count++];
    call[0] = kind;
    call[1] = index;
    call[2] = call[3] = 0;
    FloatBits px = {.number = x}, py = {.number = y};
    call[4] = px.bits;
    call[5] = py.bits;
#if defined(AB_ORACLE_UI_BUTTON_STATE)
    ui_button_raw_flags(lua, call + 6);
#else
    (void)lua;
#endif
    return call;
}

static int ui_button_hit(void *lua) {
    uint32_t *call =
        ui_button_boundary(lua, 1, ui_button_hit_count, damage_lua.to_number(lua, 2),
                           damage_lua.to_number(lua, 3));
    call[2] = (ui_button_command[4] >> ui_button_hit_count++) & 1;
    damage_lua.push_boolean(lua, (int)call[2]);
    return 1;
}

static int ui_button_audio(void *lua) {
    uint32_t *call = ui_button_boundary(lua, 2, 0, damage_lua.to_number(lua, 2), 0);
    call[5] = (uint32_t)damage_lua.to_boolean(lua, 3);
    ui_button_value(lua, 1, call + 2);
    return 0;
}

static int ui_button_child(void *lua) {
    damage_lua.get_field(lua, 1, "_ab_index");
    uint32_t index = (uint32_t)damage_lua.to_number(lua, -1);
    damage_lua.set_top(lua, 4);
    if (index >= 2 || !damage_named(lua, 2, ui_button_events[ui_button_command[3]])) {
        fail("Unexpected original button child event\n");
    }
    (void)ui_button_boundary(lua, 3, index, damage_lua.to_number(lua, 3),
                             damage_lua.to_number(lua, 4));
    for (size_t field = 0; field < 3; ++field) {
        ui_button_push(lua, ui_button_children[index][1 + field * 2],
                       ui_button_children[index][2 + field * 2]);
    }
    return 3;
}

static void ui_button_state(void *lua, uint32_t configuration) {
    damage_lua.create_table(lua, 0, 12);
    damage_lua.push_boolean(lua, (int)(configuration & 1));
    damage_lua.set_field(lua, 1, "clickStarted");
    damage_lua.push_boolean(lua, (int)(configuration & 2));
    damage_lua.set_field(lua, 1, "shouldClearPointerFlags");
    damage_lua.push_number(lua, (float)configuration + 128);
    damage_lua.set_field(lua, 1, "_ab_reply_object");
    damage_function(lua, 1, "hitTest", ui_button_hit);
    damage_lua.get_field(lua, -10002, "button-handle");
    damage_lua.set_field(lua, 1, "handlePointerEvent");
    damage_lua.set_field(lua, -10002, "_ab_button");
}

static void ui_button_prepare(void *lua) {
    damage_lua.get_field(lua, -10002, "_ab_button");
    const char *flags[] = {"visible", "image", "activateOnRelease"};
    for (size_t field = 0; field < 3; ++field) {
        damage_lua.push_boolean(lua, (int)(ui_button_command[1] & (1u << field)));
        damage_lua.set_field(lua, 1, flags[field]);
    }
    damage_number(lua, 1, "x", ui_button_command[5]);
    damage_number(lua, 1, "y", ui_button_command[6]);
    const char *values[] = {"enabled", "returnValue", "disabledReturnValue", "meta",
                            "clickSound"};
    for (size_t field = 0; field < 5; ++field) {
        ui_button_push(lua, ui_button_command[9 + field * 2],
                       ui_button_command[10 + field * 2]);
        damage_lua.set_field(lua, 1, values[field]);
    }
    damage_lua.create_table(lua, 2, 0);
    for (uint32_t index = 0; index < 2; ++index) {
        damage_lua.create_table(lua, 0, 4);
        damage_lua.push_number(lua, (float)index);
        damage_lua.set_field(lua, 3, "_ab_index");
        damage_lua.push_boolean(lua, (int)(ui_button_children[index][0] & 1));
        damage_lua.set_field(lua, 3, "visible");
        damage_lua.push_boolean(lua, (int)(ui_button_children[index][0] & 2));
        damage_lua.set_field(lua, 3, "active");
        damage_function(lua, 3, "onPointerEvent", ui_button_child);
        ui_button_raw_set(lua, 2, (int)index + 1);
    }
    damage_lua.set_field(lua, 1, "children");
    damage_lua.set_top(lua, 0);
    damage_lua.create_table(lua, 0, 2);
    damage_lua.push_string(lua, ui_button_command[1] & 8 ? "ipad" : "android");
    damage_lua.set_field(lua, 1, "deviceModel");
    damage_number(lua, 1, "touchcount", ui_button_command[2]);
    damage_lua.set_field(lua, -10002, "gamelua");
}

static void ui_button_run(void *lua) {
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
        fail(message ? message : "Original button pointer execution failed\n");
    }
    uint32_t record[9];
    for (size_t field = 0; field < 3; ++field) {
        ui_button_value(lua, (int)field + 1, record + field * 2);
    }
    damage_lua.get_field(lua, -10002, "_ab_button");
    damage_lua.get_field(lua, 4, "clickStarted");
    record[6] = (uint32_t)damage_lua.to_boolean(lua, 5);
    damage_lua.get_field(lua, 4, "shouldClearPointerFlags");
    record[7] = (uint32_t)damage_lua.to_boolean(lua, 6);
    record[8] = ui_button_call_count;
    print_words(record, 9);
    for (uint32_t index = 0; index < ui_button_call_count; ++index) {
        print_words(ui_button_calls[index], 6);
    }
    damage_lua.set_top(lua, 0);
}

static void ui_button_random_value(uint32_t *random, uint32_t output[2]) {
    output[0] = (random_word(random) >> 16) % 5;
    output[1] = random_word(random);
    if (!output[0]) {
        output[1] = 0;
    } else if (output[0] == 1) {
        output[1] &= 1;
    } else if (output[0] == 3) {
        output[1] = (output[1] >> 16) % 5;
    } else if (output[0] == 4) {
        output[1] = (output[1] >> 16) % 31 + 1;
    }
}

static void *ui_button_initialize(void) {
    initialize_damage_lua();
    ui_button_raw_set =
        (void (*)(void *, int, int))find_function("_Z11lua_rawsetiP9lua_Stateii");
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original ImageButton state\n");
    }
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    const char *paths[] = {"/data/local/tmp/ab520/ui/image-button-handle-oracle.lua",
                           "/data/local/tmp/ab520/ui/image-button-pointer-oracle.lua",
                           "/data/local/tmp/ab520/ui/image-button-flags-oracle.lua",
                           "/data/local/tmp/ab520/ui/image-button-events-oracle.lua",
                           "/data/local/tmp/ab520/ui/image-button-frame-oracle.lua"};
    const char *names[] = {"button-handle", "button-pointer", "button-flags",
                           "button-events", "button-frame"};
    for (size_t index = 0; index < 5; ++index) {
        load_selected_function(lua, paths[index], names[index]);
    }
    damage_lua.create_table(lua, 0, 1);
    damage_lua.get_field(lua, -10002, "button-frame");
    damage_lua.set_field(lua, 1, "onPointerEvent");
    damage_lua.set_field(lua, -10002, "Frame");
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 1, "playAudio", ui_button_audio);
    damage_lua.set_field(lua, -10002, "res");
    return lua;
}

#if defined(AB_ORACLE_UI_BUTTON_STATE)
#include "ui_widget_pointer_probe.h"
#endif

static void probe_ui_image_button(void) {
#if defined(AB_ORACLE_UI_BUTTON_STATE)
    probe_ui_widget_pointer();
    return;
#endif
    void *lua = ui_button_initialize();
    uint32_t random = UINT32_C(0x520b1770);
    semihost(4, "AB_ANDROID_UI_IMAGE_BUTTON_V1 64 128\n");
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
                ui_button_random_value(&random, ui_button_command + 9 + field * 2);
            }
            for (size_t child = 0; child < 2; ++child) {
                ui_button_children[child][0] = (random_word(&random) >> 16) & 3;
                for (size_t field = 0; field < 3; ++field) {
                    ui_button_random_value(&random,
                                           ui_button_children[child] + 1 + field * 2);
                }
            }
            if (step < 16) {
                const uint32_t operations[] = {1, 1, 1, 3, 1, 1, 2, 1,
                                               1, 1, 1, 2, 1, 1, 1, 1};
                const uint32_t events[] = {0, 1, 2, 0, 0, 4, 0, 2,
                                           0, 2, 0, 0, 0, 0, 2, 5};
                ui_button_command[0] = operations[step];
                ui_button_command[1] = step == 9                  ? 6u
                                       : step == 10               ? 6u
                                       : step == 12 || step == 13 ? 3u
                                                                  : 7u;
                ui_button_command[2] = 0;
                ui_button_command[3] = events[step];
                ui_button_command[4] = 7;
                ui_button_command[9] = 1;
                ui_button_command[10] = step != 13;
                ui_button_command[11] = 3;
                ui_button_command[12] = 3;
                ui_button_command[13] = 1;
                ui_button_command[14] = 0;
                ui_button_command[17] = 3;
                ui_button_command[18] = 4;
            }
            print_words(ui_button_command, 19);
            for (size_t child = 0; child < 2; ++child) {
                print_words(ui_button_children[child], 7);
            }
            ui_button_prepare(lua);
            ui_button_run(lua);
        }
    }
    damage_lua.close_state(lua);
    semihost(4, "AB UI image button reference complete\n");
}

#endif
