#ifndef AB_ORACLE_MAIN_MENU_SETTINGS_H
#define AB_ORACLE_MAIN_MENU_SETTINGS_H

#include "ui_value_probe.h"

static const char *const main_settings_texts[] = {"", "water", "tag", "themeUGC_menu",
                                                  "theme1"};
static uint32_t main_settings_input[13];
static void (*main_settings_push_value)(void *, int);
static void (*main_settings_set_table)(void *, int);

static int main_settings_identity(void *lua) {
    (void)lua;
    fail("Original settings called an identity-only value\n");
}

static void main_settings_push(void *lua, uint32_t kind, uint32_t payload) {
    if (kind == 5) {
        damage_lua.get_field(lua, -10002, "_ab_settings_identity");
    } else {
        ui_reference_value_push(lua, kind, payload, main_settings_texts, 5);
    }
}

static void main_settings_read(void *lua, int index, uint32_t output[2]) {
    if (damage_lua.type(lua, index) == 6) {
        output[0] = 5;
        output[1] = 0;
    } else {
        ui_reference_value_read(lua, index, output, main_settings_texts, 5);
    }
}

static void main_settings_record(void *lua, int key, uint32_t mode) {
    if (!mode || damage_lua.type(lua, key) == 0) {
        return;
    }
    main_settings_push_value(lua, key);
    if (mode == 1) {
        damage_lua.push_boolean(lua, 0);
    } else if (mode == 2) {
        damage_lua.create_table(lua, 0, 0);
    } else {
        damage_lua.push_number(lua, 0);
    }
    main_settings_set_table(lua, 2);
}

static void main_settings_prepare(void *lua) {
    damage_lua.create_table(lua, 0, 3);
    const char *fields[] = {"currentZoomLevelMainMenu", "currentMainMenuTheme",
                            "gfxLowQuality"};
    for (size_t field = 0; field < 3; ++field) {
        main_settings_push(lua, main_settings_input[field * 2],
                           main_settings_input[field * 2 + 1]);
        damage_lua.set_field(lua, 1, fields[field]);
    }
    main_settings_push_value(lua, 1);
    damage_lua.set_field(lua, -10002, "localSettings");
    damage_lua.set_field(lua, -10002, "settings");
    const char *values[] = {"_ab_settings_new_zoom", "_ab_settings_new_theme"};
    for (size_t value = 0; value < 2; ++value) {
        main_settings_push(lua, main_settings_input[8 + value * 2],
                           main_settings_input[9 + value * 2]);
        damage_lua.set_field(lua, -10002, values[value]);
    }
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 3);
    damage_lua.create_table(lua, 0, 1);
    main_settings_push(lua, main_settings_input[6], main_settings_input[7]);
    damage_lua.set_field(lua, 3, "defaultTheme");
    damage_lua.set_field(lua, 2, "settings");
    damage_lua.get_field(lua, -10002, "localSettings");
    damage_lua.get_field(lua, 3, "currentMainMenuTheme");
    main_settings_record(lua, 4, main_settings_input[12]);
    damage_lua.set_top(lua, 2);
    damage_lua.get_field(lua, -10002, "_ab_settings_new_theme");
    main_settings_record(lua, 3, main_settings_input[12]);
    damage_lua.set_top(lua, 2);
    damage_lua.set_field(lua, 1, "themes");
    damage_lua.set_field(lua, -10002, "blockTable");
}

static void main_settings_get(void *lua, const char *name, uint32_t output[2]) {
    damage_lua.get_field(lua, -10002, "settingsWrapper");
    damage_lua.get_field(lua, 1, name);
    main_settings_push_value(lua, 1);
    if (damage_lua.call(lua, 1, 1, 0)) {
        const char *message = damage_lua.to_string(lua, -1, NULL);
        fail(message ? message : "Original main-menu settings getter failed\n");
    }
    main_settings_read(lua, 2, output);
    damage_lua.set_top(lua, 0);
}

static void main_settings_set(void *lua, const char *name, const char *value) {
    damage_lua.get_field(lua, -10002, "settingsWrapper");
    damage_lua.get_field(lua, 1, name);
    main_settings_push_value(lua, 1);
    int arguments = 1;
    if (value) {
        damage_lua.get_field(lua, -10002, value);
        ++arguments;
    }
    if (damage_lua.call(lua, arguments, 0, 0)) {
        const char *message = damage_lua.to_string(lua, -1, NULL);
        fail(message ? message : "Original main-menu settings setter failed\n");
    }
    damage_lua.set_top(lua, 0);
}

static void main_settings_field(void *lua, const char *name, uint32_t output[2]) {
    damage_lua.get_field(lua, -10002, "localSettings");
    damage_lua.get_field(lua, 1, name);
    main_settings_read(lua, 2, output);
    damage_lua.set_top(lua, 0);
}

static void main_settings_case(void *lua) {
    uint32_t output[22];
    main_settings_prepare(lua);
    main_settings_get(lua, "getCurrentZoomLevelMainMenu", output);
    main_settings_get(lua, "getCurrentMainMenuTheme", output + 2);
    main_settings_get(lua, "isGfxLowQuality", output + 4);
    main_settings_set(lua, "setCurrentZoomLevelMainMenu", "_ab_settings_new_zoom");
    main_settings_get(lua, "getCurrentZoomLevelMainMenu", output + 6);
    main_settings_field(lua, "currentZoomLevelMainMenu", output + 8);
    main_settings_set(lua, "setCurrentMainMenuTheme", "_ab_settings_new_theme");
    main_settings_get(lua, "getCurrentMainMenuTheme", output + 10);
    main_settings_field(lua, "currentMainMenuTheme", output + 12);
    main_settings_set(lua, "setCurrentZoomLevelMainMenu", NULL);
    main_settings_get(lua, "getCurrentZoomLevelMainMenu", output + 14);
    main_settings_field(lua, "currentZoomLevelMainMenu", output + 16);
    main_settings_field(lua, "currentMainMenuTheme", output + 18);
    main_settings_field(lua, "gfxLowQuality", output + 20);
    print_words(output, 22);
}

static void probe_main_menu_settings(void) {
    initialize_damage_lua();
    main_settings_push_value =
        (void (*)(void *, int))find_function("_Z13lua_pushvalueP9lua_Statei");
    main_settings_set_table =
        (void (*)(void *, int))find_function("_Z12lua_settableP9lua_Statei");
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original main-menu settings state\n");
    }
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    damage_lua.push_string(lua, "fixture");
    damage_lua.set_field(lua, -10002, "uniqueDeviceId");
    damage_lua.push_string(lua, "fixture");
    damage_lua.set_field(lua, -10002, "uniqueInstallationId");
    damage_lua.create_table(lua, 0, 0);
    damage_lua.set_field(lua, -10002, "settings");
    damage_function(lua, -10002, "_ab_settings_identity", main_settings_identity);
    load_damage_chunk(lua, "/data/local/tmp/ab520/ui/menu-settings.lua");
    semihost(4, "AB_ANDROID_MAIN_MENU_SETTINGS_V1 4096\n");
    uint32_t random = UINT32_C(0x5205e771);
    for (size_t item = 0; item < 4096; ++item) {
        for (size_t field = 0; field < 6; ++field) {
            uint32_t kind = random_word(&random) % 6;
            uint32_t payload = random_word(&random);
            if (kind == 0 || kind == 5) {
                payload = 0;
            } else if (kind == 1) {
                payload &= 1;
            } else if (kind == 3) {
                payload %= 4;
            } else if (kind == 4) {
                payload %= 32;
            } else if (field == 1 || field == 5) {
                payload = random_float(&random, 118, 20, 1);
            }
            main_settings_input[field * 2] = kind;
            main_settings_input[field * 2 + 1] = payload;
        }
        main_settings_input[12] = (uint32_t)item % 4;
        print_words(main_settings_input, 13);
        main_settings_case(lua);
    }
    damage_lua.close_state(lua);
    semihost(4, "AB main-menu settings reference complete\n");
}

#endif
