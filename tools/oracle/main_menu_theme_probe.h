#ifndef AB_ORACLE_MAIN_MENU_THEME_H
#define AB_ORACLE_MAIN_MENU_THEME_H

#include "ui_value_probe.h"

#if defined(AB_ORACLE_MAIN_MENU_RESOURCES)
static const char *const main_theme_names[] = {"themeBeach", "settings", "", "missing"};
static void (*main_theme_push_value)(void *, int);
#else
static const char *const main_theme_names[] = {"theme1", "themeUGC_menu", "",
                                               "blue grass"};
#endif
static const char *const main_theme_texts[] = {"", "water", "tag", "themeUGC_menu"};
static uint32_t main_theme_input[14], main_theme_calls[8][8], main_theme_call_count;
static unsigned main_theme_require_count;

static int main_theme_identity(void *lua) {
    (void)lua;
    fail("Original main-menu theme called an identity-only field\n");
}

static void main_theme_push(void *lua, uint32_t kind, uint32_t payload) {
    if (kind == 5) {
        damage_lua.get_field(lua, -10002, "_ab_main_theme_identity");
    } else {
        ui_reference_value_push(lua, kind, payload, main_theme_texts, 4);
    }
}

static void main_theme_value(void *lua, int index, uint32_t output[2]) {
    if (damage_lua.type(lua, index) == 6) {
        output[0] = 5;
        output[1] = 0;
    } else {
        ui_reference_value_read(lua, index, output, main_theme_texts, 4);
    }
}

static void main_theme_water(void *lua, uint32_t output[2]) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, "objects");
    damage_lua.get_field(lua, top + 1, "waterLevel");
    main_theme_value(lua, top + 2, output);
    damage_lua.set_top(lua, top);
}

static uint32_t *main_theme_record(void *lua, uint32_t operation) {
    if (main_theme_call_count >= 8) {
        fail("Original main-menu theme callback count exceeds storage\n");
    }
    uint32_t *row = main_theme_calls[main_theme_call_count++];
    for (size_t field = 0; field < 8; ++field) {
        row[field] = 0;
    }
    row[0] = operation;
    main_theme_water(lua, row + 5);
    return row;
}

static int main_theme_setting(void *lua, uint32_t operation) {
    if (damage_lua.get_top(lua) != 1 || damage_lua.type(lua, 1) != 5) {
        fail("Original main-menu theme settings method omitted self\n");
    }
    (void)main_theme_record(lua, operation);
#if defined(AB_ORACLE_MAIN_MENU_RESOURCES)
    const char *methods[] = {"setCurrentZoomLevelMainMenu",
                             "getCurrentZoomLevelMainMenu", "getCurrentMainMenuTheme"};
    damage_lua.get_field(lua, -10002, "_ab_main_theme_actual_settings");
    damage_lua.get_field(lua, 2, methods[operation]);
    main_theme_push_value(lua, 2);
    if (damage_lua.call(lua, 1, operation ? 1 : 0, 0)) {
        const char *message = damage_lua.to_string(lua, -1, NULL);
        fail(message ? message : "Original main-menu resource settings failed\n");
    }
    if (operation == 2) {
        main_theme_push_value(lua, 3);
        damage_lua.set_field(lua, -10002, "_ab_main_theme_selected");
    }
    return operation ? 1 : 0;
#else
    if (operation == 1) {
        FloatBits zoom = {.bits = main_theme_input[1]};
        damage_lua.push_number(lua, zoom.number);
        return 1;
    }
    if (operation == 2) {
        damage_lua.push_string(lua, main_theme_names[main_theme_input[5]]);
        return 1;
    }
    return 0;
#endif
}

static int main_theme_set_zoom(void *lua) {
    return main_theme_setting(lua, 0);
}

static int main_theme_get_zoom(void *lua) {
    return main_theme_setting(lua, 1);
}

static int main_theme_get_theme(void *lua) {
    return main_theme_setting(lua, 2);
}

static int main_theme_bounds(void *lua) {
    if (damage_lua.get_top(lua) != 1 || !damage_named(lua, 1, "BUTTON_EMPTY")) {
        fail("Original main-menu theme requested different sprite bounds\n");
    }
    (void)main_theme_record(lua, 3);
    for (size_t axis = 0; axis < 2; ++axis) {
        FloatBits value = {.bits = main_theme_input[2 + axis]};
        damage_lua.push_number(lua, value.number);
    }
    return 2;
}

static int main_theme_camera(void *lua, uint32_t operation, int count) {
    if (damage_lua.get_top(lua) != count) {
        fail("Original main-menu theme camera arguments changed\n");
    }
    uint32_t *row = main_theme_record(lua, operation);
    row[2] = (uint32_t)count;
    for (int index = 0; index < count; ++index) {
        if (damage_lua.type(lua, index + 1) != 3) {
            fail("Original main-menu theme camera omitted a number\n");
        }
        FloatBits value = {.number = damage_lua.to_number(lua, index + 1)};
        row[3 + index] = value.bits;
    }
    return 0;
}

static int main_theme_camera_zoom(void *lua) {
    return main_theme_camera(lua, 4, 1);
}

static int main_theme_camera_position(void *lua) {
    return main_theme_camera(lua, 7, 2);
}

static int main_theme_select(void *lua, uint32_t operation) {
#if defined(AB_ORACLE_MAIN_MENU_RESOURCES)
    damage_lua.get_field(lua, -10002, "_ab_main_theme_selected");
    uint32_t selected = 4;
    for (uint32_t index = 0; index < 4; ++index) {
        if (damage_named(lua, 2, main_theme_names[index])) {
            selected = index;
        }
    }
    damage_lua.set_top(lua, 1);
#else
    uint32_t selected = main_theme_input[5];
#endif
    if (damage_lua.get_top(lua) != 1 || selected >= 4 ||
        !damage_named(lua, 1, main_theme_names[selected])) {
        fail("Original main-menu theme/effects selected a different identifier\n");
    }
    uint32_t *row = main_theme_record(lua, operation);
    row[1] = selected + 1;
    return 0;
}

static int main_theme_select_theme(void *lua) {
    return main_theme_select(lua, 5);
}

static int main_theme_select_effects(void *lua) {
    return main_theme_select(lua, 6);
}

static int main_theme_require(void *lua) {
    const char *paths[] = {"lua/lua.lua", "lua/Class.lua", "Scaling.lua"};
    if (main_theme_require_count >= 3 || damage_lua.get_top(lua) != 1 ||
        !damage_named(lua, 1, paths[main_theme_require_count++])) {
        fail("Original main-menu installation dependency request changed\n");
    }
    return 0;
}

static void main_theme_install(void *lua) {
    damage_function(lua, -10002, "requireFile", main_theme_require);
    damage_function(lua, -10002, "_ab_main_theme_identity", main_theme_identity);
    const char *paths[] = {"/data/local/tmp/ab520/ui/menu-lua-base.lua",
                           "/data/local/tmp/ab520/ui/menu-class.lua",
                           "/data/local/tmp/ab520/ui/menu-frame.lua"};
    for (size_t index = 0; index < 3; ++index) {
        load_damage_chunk(lua, paths[index]);
    }
    damage_lua.get_field(lua, -10002, "ui");
    damage_lua.create_table(lua, 0, 0);
    damage_lua.set_field(lua, 1, "Image");
    damage_lua.create_table(lua, 0, 0);
    damage_lua.set_field(lua, 1, "ImageButton");
    damage_lua.set_top(lua, 0);
    damage_lua.create_table(lua, 0, 0);
    damage_lua.set_field(lua, -10002, "Scaling");
    load_damage_chunk(lua, "/data/local/tmp/ab520/ui/menu-main.lua");
    if (main_theme_require_count != 3) {
        fail("Original main-menu installation omitted expected dependencies\n");
    }
#if defined(AB_ORACLE_MAIN_MENU_RESOURCES)
    damage_lua.push_string(lua, "fixture");
    damage_lua.set_field(lua, -10002, "uniqueDeviceId");
    damage_lua.push_string(lua, "fixture");
    damage_lua.set_field(lua, -10002, "uniqueInstallationId");
    damage_lua.create_table(lua, 0, 0);
    damage_lua.set_field(lua, -10002, "settings");
    load_damage_chunk(lua, "/data/local/tmp/ab520/ui/menu-settings.lua");
    damage_lua.get_field(lua, -10002, "settingsWrapper");
    damage_lua.set_field(lua, -10002, "_ab_main_theme_actual_settings");
    load_damage_chunk(lua, "/data/local/tmp/ab520/ui/menu-themes.lua");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 2);
    damage_lua.get_field(lua, -10002, "settings");
    damage_lua.set_field(lua, 2, "settings");
    damage_lua.get_field(lua, -10002, "themeBeach");
    damage_lua.set_field(lua, 2, "themeBeach");
    damage_lua.set_field(lua, 1, "themes");
    damage_lua.set_field(lua, -10002, "blockTable");
#endif
    damage_lua.create_table(lua, 0, 3);
    damage_function(lua, 1, "setCurrentZoomLevelMainMenu", main_theme_set_zoom);
    damage_function(lua, 1, "getCurrentZoomLevelMainMenu", main_theme_get_zoom);
    damage_function(lua, 1, "getCurrentMainMenuTheme", main_theme_get_theme);
    damage_lua.set_field(lua, -10002, "settingsWrapper");
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 1, "getSpriteBounds", main_theme_bounds);
    damage_lua.set_field(lua, -10002, "res");
    damage_lua.create_table(lua, 0, 2);
    damage_function(lua, 1, "setCameraZoomScale", main_theme_camera_zoom);
    damage_function(lua, 1, "setCameraTopLeft", main_theme_camera_position);
    damage_lua.set_field(lua, -10002, "CameraNative");
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 1, "setTheme", main_theme_select_theme);
    damage_lua.set_field(lua, -10002, "ThemeSystem");
    damage_function(lua, -10002, "setLevelEffects", main_theme_select_effects);
}

static void main_theme_prepare(void *lua) {
    damage_number(lua, -10002, "screenHeight", main_theme_input[0]);
    damage_number(lua, -10002, "g_current_menu_scroll", main_theme_input[4]);
    damage_lua.create_table(lua, 0, 1);
    main_theme_push(lua, main_theme_input[12], main_theme_input[13]);
    damage_lua.set_field(lua, 1, "waterLevel");
    damage_lua.set_field(lua, -10002, "objects");
#if defined(AB_ORACLE_MAIN_MENU_RESOURCES)
    damage_lua.create_table(lua, 0, 3);
    damage_number(lua, 1, "currentZoomLevelMainMenu", main_theme_input[1]);
    if (main_theme_input[5] < 4) {
        damage_lua.push_string(lua, main_theme_names[main_theme_input[5]]);
    } else {
        const uint32_t kinds[] = {0, 1, 2, 4, 5};
        main_theme_push(lua, kinds[main_theme_input[5] - 4], 0);
    }
    damage_lua.set_field(lua, 1, "currentMainMenuTheme");
    main_theme_push(lua, main_theme_input[8], main_theme_input[9]);
    damage_lua.set_field(lua, 1, "gfxLowQuality");
    damage_lua.set_field(lua, -10002, "localSettings");
#else
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 3);
    const char *fields[] = {"menuTopOffset", "menuWater", "waterLevel"};
    for (size_t field = 0; field < 3; ++field) {
        main_theme_push(lua, main_theme_input[6 + field * 2],
                        main_theme_input[7 + field * 2]);
        damage_lua.set_field(lua, 3, fields[field]);
    }
    damage_lua.set_field(lua, 2, main_theme_names[main_theme_input[5]]);
    damage_lua.set_field(lua, 1, "themes");
    damage_lua.set_field(lua, -10002, "blockTable");
#endif
}

static void main_theme_random_value(uint32_t *random, uint32_t output[2]) {
    output[0] = random_word(random) % 6;
    output[1] = random_word(random);
    if (output[0] == 0 || output[0] == 5) {
        output[1] = 0;
    } else if (output[0] == 1) {
        output[1] &= 1;
    } else if (output[0] == 3) {
        output[1] %= 4;
    } else if (output[0] == 4) {
        output[1] %= 32;
    }
}

static void probe_main_menu_theme(void) {
    initialize_damage_lua();
#if defined(AB_ORACLE_MAIN_MENU_RESOURCES)
    main_theme_push_value =
        (void (*)(void *, int))find_function("_Z13lua_pushvalueP9lua_Statei");
#endif
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original main-menu theme state\n");
    }
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    main_theme_install(lua);
    uint32_t random = UINT32_C(0x5206e719);
#if defined(AB_ORACLE_MAIN_MENU_RESOURCES)
    semihost(4, "AB_ANDROID_MAIN_MENU_RESOURCES_V1 4096\n");
#else
    semihost(4, "AB_ANDROID_MAIN_MENU_THEME_V1 4096\n");
#endif
    for (size_t item = 0; item < 4096; ++item) {
        const float heights[] = {240, 320, 321,  480,  640,  720,
                                 768, 960, 1024, 1080, 1440, 2160};
        FloatBits height = {.number = heights[item % 12]};
        FloatBits zoom = {.number = (float)(random_word(&random) % 1024 + 1) / 256};
        FloatBits button = {
            .number = (float)((int32_t)(random_word(&random) % 513) - 256) / 8};
        main_theme_input[0] = height.bits;
        main_theme_input[1] = zoom.bits;
        main_theme_input[2] = random_word(&random);
        main_theme_input[3] = button.bits;
        main_theme_input[4] = random_float(&random, 118, 20, 1);
        main_theme_input[5] = (uint32_t)item % 4;
        main_theme_input[6] = (uint32_t)item % 3;
        FloatBits offset = {
            .number = (float)((int32_t)(random_word(&random) % 4097) - 2048) / 8};
        main_theme_input[7] = main_theme_input[6] == 2 ? offset.bits : 0;
        main_theme_random_value(&random, main_theme_input + 8);
        main_theme_random_value(&random, main_theme_input + 10);
        main_theme_random_value(&random, main_theme_input + 12);
#if defined(AB_ORACLE_MAIN_MENU_RESOURCES)
        /* Prepared bounds of the two pinned BUTTONS_SHEET_1 SPRTs. Runtime C
         * independently reads these dimensions from APK metadata. */
        FloatBits sprite_width = {.number = item % 2 ? 104 : 58};
        FloatBits sprite_height = {.number = item % 2 ? 111 : 58};
        main_theme_input[2] = sprite_width.bits;
        main_theme_input[3] = sprite_height.bits;
        main_theme_input[5] = (uint32_t)item % 9;
#endif
        print_words(main_theme_input, 14);
        main_theme_prepare(lua);
        main_theme_call_count = 0;
        damage_lua.get_field(lua, -10002, "layoutMainMenuTheme");
        if (damage_lua.call(lua, 0, 1, 0)) {
            const char *message = damage_lua.to_string(lua, -1, NULL);
            fail(message ? message : "Original main-menu theme layout failed\n");
        }
        uint32_t output[3];
        FloatBits top = {.number = damage_lua.to_number(lua, 1)};
        output[0] = top.bits;
        main_theme_water(lua, output + 1);
        print_words(output, 3);
        print_words(&main_theme_call_count, 1);
        for (uint32_t index = 0; index < main_theme_call_count; ++index) {
            print_words(main_theme_calls[index], 8);
        }
        damage_lua.set_top(lua, 0);
    }
    damage_lua.close_state(lua);
#if defined(AB_ORACLE_MAIN_MENU_RESOURCES)
    semihost(4, "AB main-menu resources reference complete\n");
#else
    semihost(4, "AB main-menu theme reference complete\n");
#endif
}

#endif
