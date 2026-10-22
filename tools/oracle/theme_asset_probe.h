#ifndef AB_ORACLE_THEME_ASSET_H
#define AB_ORACLE_THEME_ASSET_H

#include "../../tests/theme_asset_cases.h"

static void (*theme_asset_set_index)(void *, int, int);
static void (*theme_asset_get_index)(void *, int, int);
static const void *theme_asset_old, *theme_asset_target;
static uint32_t theme_asset_output[69], theme_asset_fail;

static uint32_t theme_asset_identity(void *lua, int index) {
    const void *pointer = damage_lua.to_pointer(lua, index);
    uint32_t identity = (pointer == theme_asset_old ? 1u : 0u) |
                        (pointer == theme_asset_target ? 2u : 0u);
    return identity ? identity : 4;
}

static uint32_t theme_asset_name(void *lua, int index) {
    for (uint32_t name = 0; name < 6; ++name) {
        if (damage_named(lua, index, theme_asset_names[name])) {
            return name;
        }
    }
    fail("Unknown original theme asset name\n");
}

static int theme_asset_event(void *lua, uint32_t loading) {
    if (damage_lua.get_top(lua) != 1 || damage_lua.type(lua, 1) != 5 ||
        theme_asset_output[1] >= 13) {
        fail("Invalid original theme asset callback\n");
    }
    theme_asset_get_index(lua, 1, 1);
    uint32_t name = theme_asset_name(lua, 2);
    damage_lua.get_field(lua, -10002, "currentGFXSet");
    size_t offset = 30 + theme_asset_output[1] * 3;
    theme_asset_output[offset] = loading;
    theme_asset_output[offset + 1] = name;
    theme_asset_output[offset + 2] = theme_asset_identity(lua, 3);
    ++theme_asset_output[1];
    damage_lua.push_boolean(
        lua, !(theme_asset_fail && theme_asset_output[1] == theme_asset_fail));
    return 1;
}

static int theme_asset_load(void *lua) {
    return theme_asset_event(lua, 1);
}

static int theme_asset_release(void *lua) {
    return theme_asset_event(lua, 0);
}

static int theme_asset_print(void *lua) {
    (void)lua;
    return 0;
}

/* Raise controlled failures inside the original VM after the C recorder returns.
 * Throwing the target's C++ Lua exception through a freestanding C callback has
 * no supported unwind boundary. These authored wrappers only record/raise; the
 * graphics transition and set difference execute unchanged target bytecode. */
static void theme_asset_wrapper(void *lua, const char *source, size_t size,
                                const char *name) {
    if (damage_lua.load(lua, source, (unsigned)size, name) ||
        damage_lua.call(lua, 0, 1, 0)) {
        fail("Cannot prepare controlled theme asset boundary\n");
    }
    damage_lua.set_field(lua, -10002, name);
}

static void theme_asset_function(void *lua, const char *path, size_t size,
                                 const char *name) {
    char bytes[739];
    int file = system_call(5, (int)(uintptr_t)path, NULL, 0);
    if (file < 0 || size > sizeof(bytes)) {
        fail("Cannot open original theme asset function\n");
    }
    size_t offset = 0;
    while (offset < size) {
        int count = system_call(3, file, bytes + offset, size - offset);
        if (count <= 0 || (size_t)count > size - offset) {
            fail("Incomplete original theme asset function\n");
        }
        offset += (size_t)count;
    }
    char extra;
    if (system_call(3, file, &extra, 1) != 0 || system_call(6, file, NULL, 0) != 0 ||
        damage_lua.load(lua, bytes, (unsigned)size, name)) {
        fail("Cannot load original theme asset function\n");
    }
    damage_lua.set_field(lua, -10002, name);
}

static void theme_asset_table(void *lua, const uint32_t *names, uint32_t count) {
    damage_lua.create_table(lua, 8, 0);
    int index = damage_lua.get_top(lua);
    for (uint32_t member = 0; member < count; ++member) {
        damage_lua.push_string(lua, theme_asset_names[names[member]]);
        theme_asset_set_index(lua, index, (int)member + 1);
    }
}

static void theme_asset_prepare(void *lua, const uint32_t input[18]) {
    damage_lua.set_top(lua, 0);
    theme_asset_table(lua, input + 6, input[3]);
    theme_asset_old = damage_lua.to_pointer(lua, 1);
    damage_lua.set_field(lua, -10002, "_ab_theme_old");
    damage_lua.get_field(lua, -10002, "_ab_theme_old");
    damage_lua.set_field(lua, -10002, "currentGFXSet");
    if (input[0]) {
        damage_lua.push_string(lua, theme_asset_names[input[12]]);
    } else if (input[2]) {
        damage_lua.get_field(lua, -10002, "_ab_theme_old");
    } else {
        theme_asset_table(lua, input + 12, input[4]);
    }
    theme_asset_target = input[0] ? NULL : damage_lua.to_pointer(lua, 1);
    damage_lua.set_field(lua, -10002, "_ab_theme_target");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 1);
    damage_lua.get_field(lua, -10002, "_ab_theme_target");
    damage_lua.set_field(lua, 3, "graphicSetName");
    damage_lua.set_field(lua, 2, "test");
    damage_lua.set_field(lua, 1, "themes");
    damage_lua.set_field(lua, -10002, "blockTable");
    switch (input[1]) {
        case 0:
        case 1:
            damage_lua.push_boolean(lua, (int)input[1]);
            break;
        case 2:
            damage_lua.push_number(lua, 0);
            break;
        case 3:
            damage_lua.push_string(lua, "");
            break;
        default:
            damage_lua.push_nil(lua);
            break;
    }
    damage_lua.set_field(lua, -10002, "showEditor");
    for (size_t index = 0; index < 69; ++index) {
        theme_asset_output[index] = index >= 6 ? UINT32_MAX : 0;
    }
    theme_asset_fail = input[5];
}

static uint32_t theme_asset_members(void *lua, int index, uint32_t *output) {
    uint32_t count = 0;
    int top = damage_lua.get_top(lua);
    for (; count < 8; ++count) {
        theme_asset_get_index(lua, index, (int)count + 1);
        if (!damage_lua.type(lua, top + 1)) {
            damage_lua.set_top(lua, top);
            break;
        }
        output[count] = theme_asset_name(lua, top + 1);
        damage_lua.set_top(lua, top);
    }
    if (count == 8) {
        fail("Original theme asset list exceeds probe bound\n");
    }
    return count;
}

static void probe_theme_asset(void) {
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot create original theme asset state\n");
    }
    damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    int (*open_table)(void *) =
        (int (*)(void *))find_function("_Z13luaopen_tableP9lua_State");
    open_table(lua);
    damage_lua.set_top(lua, 0);
    theme_asset_set_index =
        (void (*)(void *, int, int))find_function("_Z11lua_rawsetiP9lua_Stateii");
    theme_asset_get_index =
        (void (*)(void *, int, int))find_function("_Z11lua_rawgetiP9lua_Stateii");
    damage_function(lua, -10002, "print", theme_asset_print);
    damage_function(lua, -10002, "_ab_theme_load", theme_asset_load);
    damage_function(lua, -10002, "_ab_theme_release", theme_asset_release);
    static const char load_wrapper[] =
        "return function(request)\n"
        "    if not _ab_theme_load(request) then\n"
        "        error('Controlled theme asset boundary failure')\n"
        "    end\n"
        "end\n";
    static const char release_wrapper[] =
        "return function(request)\n"
        "    if not _ab_theme_release(request) then\n"
        "        error('Controlled theme asset boundary failure')\n"
        "    end\n"
        "end\n";
    theme_asset_wrapper(lua, load_wrapper, sizeof(load_wrapper) - 1, "loadAssets");
    theme_asset_wrapper(lua, release_wrapper, sizeof(release_wrapper) - 1,
                        "releaseAssets");
    theme_asset_function(lua, "/data/local/tmp/ab520/theme-assets.lua", 739,
                         "_ab_theme_assets");
    theme_asset_function(lua, "/data/local/tmp/ab520/set-difference.lua", 214,
                         "_ab_theme_difference");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.get_field(lua, -10002, "_ab_theme_difference");
    damage_lua.set_field(lua, 1, "difference");
    damage_lua.set_field(lua, -10002, "Set");
    semihost(4, "AB_ANDROID_THEME_ASSET_V1 4096\n");
    flush_text();
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        uint32_t input[18];
        theme_asset_case(sample, input);
        print_words(input, 18);
        theme_asset_prepare(lua, input);
        damage_lua.get_field(lua, -10002, "_ab_theme_assets");
        damage_lua.push_string(lua, "test");
        theme_asset_output[0] = damage_lua.call(lua, 1, 0, 0) == 0;
        damage_lua.set_top(lua, 0);
        damage_lua.get_field(lua, -10002, "currentGFXSet");
        theme_asset_output[2] = theme_asset_identity(lua, 1);
        theme_asset_output[3] = theme_asset_members(lua, 1, theme_asset_output + 6);
        damage_lua.set_top(lua, 0);
        damage_lua.get_field(lua, -10002, "_ab_theme_target");
        if (input[0]) {
            theme_asset_output[4] = 1;
            theme_asset_output[14] = theme_asset_name(lua, 1);
        } else {
            theme_asset_output[4] =
                theme_asset_members(lua, 1, theme_asset_output + 14);
        }
        damage_lua.set_top(lua, 0);
        damage_lua.get_field(lua, -10002, "_ab_theme_old");
        theme_asset_output[5] = theme_asset_members(lua, 1, theme_asset_output + 22);
        print_words(theme_asset_output, 69);
        damage_lua.set_top(lua, 0);
    }
    damage_lua.close_state(lua);
    semihost(4, "AB theme asset reference complete\n");
}

#endif
