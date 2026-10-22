#ifndef AB_ORACLE_UI_LOCALIZATION_H
#define AB_ORACLE_UI_LOCALIZATION_H

static void (*ui_localization_raw_set)(void *, int, int);
static const char *const ui_localization_groups[] = {"", "TEXTS_BASIC", "GROUP_A",
                                                     "GROUP_B", "GROUP_C"};
static const char *const ui_localization_texts[] = {"", "id", "same", "translated",
                                                    "value"};
static uint32_t ui_localization_responses[5][5], ui_localization_calls[8][5];
static uint32_t ui_localization_call_count, ui_localization_require_count;

static uint32_t ui_localization_id(void *lua, int index, const char *const texts[5]) {
    for (uint32_t id = 0; id < 5; ++id) {
        if (damage_named(lua, index, texts[id])) {
            return id;
        }
    }
    fail("Unexpected original UI localization fixture text\n");
}

static void ui_localization_cache(void *lua, uint32_t output[2]) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, "_ab_localization_cache");
    damage_lua.get_field(lua, -1, "group");
    output[0] = damage_lua.type(lua, -1) == 4;
    output[1] = output[0] ? ui_localization_id(lua, -1, ui_localization_groups) : 0;
    if (!output[0] && damage_lua.type(lua, -1) != 0) {
        fail("Unexpected original UI localization cache type\n");
    }
    damage_lua.set_top(lua, top);
}

static int ui_localization_get(void *lua) {
    if (damage_lua.get_top(lua) != 2 || ui_localization_call_count >= 8) {
        fail("Unexpected original UI localization lookup boundary\n");
    }
    uint32_t *call = ui_localization_calls[ui_localization_call_count++];
    call[0] = ui_localization_id(lua, 1, ui_localization_groups);
    call[1] = ui_localization_id(lua, 2, ui_localization_texts);
    call[2] = ui_localization_responses[call[0]][call[1]];
    ui_localization_cache(lua, call + 3);
    damage_lua.push_string(lua, ui_localization_texts[call[2]]);
    return 1;
}

static int ui_localization_require(void *lua) {
    const char *path = ui_localization_require_count == 0 ? "platform/platform.lua"
                                                          : "platform/Profiles.lua";
    if (ui_localization_require_count >= 2 || !damage_named(lua, 1, path)) {
        fail("Unexpected original UI localization dependency request\n");
    }
    ++ui_localization_require_count;
    return 0;
}

static void ui_localization_reset(void *lua, uint32_t present, uint32_t id) {
    damage_lua.get_field(lua, -10002, "_ab_localization_cache");
    if (present) {
        damage_lua.push_string(lua, ui_localization_groups[id]);
    } else {
        damage_lua.push_nil(lua);
    }
    damage_lua.set_field(lua, 1, "group");
    damage_lua.set_top(lua, 0);
}

static void ui_localization_profile(void *lua, uint32_t present,
                                    const uint32_t groups[6], uint32_t count) {
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 1);
    if (present) {
        damage_lua.create_table(lua, (int)count, 0);
        for (uint32_t index = 0; index < count; ++index) {
            damage_lua.push_string(lua, ui_localization_groups[groups[index]]);
            ui_localization_raw_set(lua, 3, (int)index + 1);
        }
        damage_lua.set_field(lua, 2, "TEXT_GROUP");
    }
    damage_lua.set_field(lua, 1, "Profiles");
    damage_lua.set_field(lua, -10002, "platform");
}

static void ui_localization_run(void *lua, const uint32_t command[5]) {
    if (command[2]) {
        ui_localization_reset(lua, command[3], command[4]);
    }
    ui_localization_call_count = ui_localization_require_count = 0;
    damage_lua.get_field(lua, -10002, "localization-string");
    damage_lua.push_string(lua, ui_localization_texts[command[0]]);
    if (command[1]) {
        damage_lua.get_field(lua, -10002, "_ab_localization_cache");
    } else {
        damage_lua.push_nil(lua);
    }
    if (damage_lua.call(lua, 2, 1, 0)) {
        const char *message = damage_lua.to_string(lua, -1, NULL);
        fail(message ? message : "Original UI localization helper failed\n");
    }
    if (ui_localization_require_count != 2) {
        fail("Original UI localization dependencies were not prepared\n");
    }
    uint32_t output[4];
    output[0] = ui_localization_id(lua, 1, ui_localization_texts);
    ui_localization_cache(lua, output + 1);
    output[3] = ui_localization_call_count;
    print_words(output, 4);
    for (uint32_t index = 0; index < ui_localization_call_count; ++index) {
        print_words(ui_localization_calls[index], 5);
    }
    damage_lua.set_top(lua, 0);
}

static void probe_ui_localization(void) {
    initialize_damage_lua();
    ui_localization_raw_set =
        (void (*)(void *, int, int))find_function("_Z11lua_rawsetiP9lua_Stateii");
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original UI localization state\n");
    }
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    load_selected_function(lua,
                           "/data/local/tmp/ab520/ui/localization-string-oracle.lua",
                           "localization-string");
    damage_function(lua, -10002, "requireFile", ui_localization_require);
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 1, "getString", ui_localization_get);
    damage_lua.set_field(lua, -10002, "res");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.set_field(lua, -10002, "_ab_localization_cache");
    uint32_t random = UINT32_C(0x52010ca1);
    semihost(4, "AB_ANDROID_UI_LOCALIZATION_V1 32 256\n");
    for (size_t configuration = 0; configuration < 32; ++configuration) {
        uint32_t setup[] = {
            (uint32_t)(configuration % 9 != 0), (uint32_t)(configuration % 7),
            (uint32_t)(configuration % 2), (random_word(&random) >> 16) % 5};
        if (!setup[0]) {
            setup[1] = 0;
        }
        uint32_t groups[6];
        for (uint32_t index = 0; index < setup[1]; ++index) {
            groups[index] = (random_word(&random) >> 16) % 5;
        }
        print_words(setup, 4);
        if (setup[1]) {
            print_words(groups, setup[1]);
        }
        for (uint32_t group = 0; group < 5; ++group) {
            for (uint32_t id = 0; id < 5; ++id) {
                uint32_t value = (random_word(&random) >> 16) % 5;
                ui_localization_responses[group][id] = configuration % 4 == 0   ? id
                                                       : configuration % 4 == 1 ? 0
                                                                                : value;
            }
            print_words(ui_localization_responses[group], 5);
        }
        ui_localization_profile(lua, setup[0], groups, setup[1]);
        ui_localization_reset(lua, setup[2], setup[3]);
        for (size_t step = 0; step < 256; ++step) {
            uint32_t command[] = {(random_word(&random) >> 16) % 5,
                                  (random_word(&random) >> 16) & 1, step % 17 == 0,
                                  (random_word(&random) >> 16) & 1,
                                  (random_word(&random) >> 16) % 5};
            print_words(command, 5);
            ui_localization_run(lua, command);
        }
    }
    damage_lua.close_state(lua);
    semihost(4, "AB UI localization reference complete\n");
}

#endif
