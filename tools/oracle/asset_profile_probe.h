#ifndef AB_ORACLE_ASSET_PROFILE_H
#define AB_ORACLE_ASSET_PROFILE_H

#include "../../tests/asset_profile_cases.h"

#include <stdbool.h>

static uint32_t profile_input[5], profile_splash_calls;

static int profile_adventure(void *lua) {
    if (damage_lua.get_top(lua) != 1 || damage_lua.type(lua, 1) != 5) {
        fail("Unexpected original adventure-mode query\n");
    }
    damage_lua.push_boolean(lua, profile_input[3] != 0);
    return 1;
}

static int profile_contains(void *lua) {
    if (damage_lua.get_top(lua) != 2 || damage_lua.type(lua, 1) != 5 ||
        damage_lua.type(lua, 2) != 4) {
        fail("Unexpected original asset-group membership query\n");
    }
    const char *wanted = damage_lua.to_string(lua, 2, NULL);
    void (*get_index)(void *, int, int) =
        (void (*)(void *, int, int))find_function("_Z11lua_rawgetiP9lua_Stateii");
    bool found = false;
    for (int index = 1; index <= 64; ++index) {
        get_index(lua, 1, index);
        int type = damage_lua.type(lua, -1);
        if (type == 0) {
            damage_lua.set_top(lua, 2);
            break;
        }
        if (type != 4) {
            fail("Original selected membership array is not typed strings\n");
        }
        found = damage_named(lua, -1, wanted) != 0;
        damage_lua.set_top(lua, 2);
        if (found) {
            break;
        }
        if (index == 64) {
            fail("Original selected membership array exceeded its bound\n");
        }
    }
    damage_lua.push_boolean(lua, found);
    return 1;
}

static int profile_splash(void *lua) {
    if (damage_lua.get_top(lua) != 0 || ++profile_splash_calls != 1) {
        fail("Unexpected original splash-profile delegation\n");
    }
    damage_lua.push_string(lua, asset_profile_splashes[(profile_input[0] >> 11) & 1u]);
    return 1;
}

static void profile_load_private(void *lua, const char *path, size_t size,
                                 const char *label) {
    static char bytes[2584];
    if (size > sizeof(bytes)) {
        fail("Private profile chunk exceeds its bound\n");
    }
    int file = system_call(5, (int)(uintptr_t)path, NULL, 0);
    if (file < 0) {
        fail("Cannot open private original asset-profile input\n");
    }
    size_t offset = 0;
    while (offset < size) {
        int count = system_call(3, file, bytes + offset, size - offset);
        if (count <= 0 || (size_t)count > size - offset) {
            fail("Incomplete private original asset-profile input\n");
        }
        offset += (size_t)count;
    }
    char extra;
    if (system_call(3, file, &extra, 1) != 0) {
        fail("Private asset-profile input exceeds its pinned extent\n");
    }
    (void)system_call(6, file, NULL, 0);
    if (damage_lua.load(lua, bytes, (unsigned)size, label)) {
        fail("Cannot load private original asset-profile chunk\n");
    }
}

static void probe_asset_profile(void) {
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot create original asset-profile Lua state\n");
    }
    damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    int (*open_string)(void *) =
        (int (*)(void *))find_function("_Z14luaopen_stringP9lua_State");
    open_string(lua);
    damage_lua.set_top(lua, 0);
    profile_load_private(lua, "/data/local/tmp/ab520/asset-groups.lua", 821,
                         "original-asset-groups");
    if (damage_lua.call(lua, 0, 0, 0)) {
        fail("Cannot execute original static asset-group data\n");
    }
    damage_lua.set_top(lua, 0);
    damage_lua.create_table(lua, 0, 0);
    damage_lua.set_field(lua, -10002, "platform");
    damage_lua.create_table(lua, 0, 0);
    damage_lua.set_field(lua, -10002, "gamelua");
    damage_function(lua, -10002, "requireFile", damage_require);
    profile_load_private(lua, "/data/local/tmp/ab520/android-asset-profiles.lua", 2584,
                         "original-android-asset-profiles");
    if (damage_lua.call(lua, 0, 0, 0)) {
        fail("Cannot initialize original Android profile closures\n");
    }
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "platform");
    damage_lua.get_field(lua, 1, "Profiles");
    damage_function(lua, 2, "selectSplashProfile", profile_splash);
    damage_lua.get_field(lua, 2, "selectAssetProfile");
    damage_lua.set_field(lua, -10002, "_ab_asset_profile");
    damage_lua.get_field(lua, 2, "getDefaultProfile");
    damage_lua.set_field(lua, -10002, "_ab_default_profile");
    damage_lua.set_top(lua, 0);
    semihost(4, "AB_ANDROID_ASSET_PROFILE_V1 4096\n");
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        asset_profile_case(sample, profile_input);
        print_words(profile_input, 5);
        damage_lua.create_table(lua, 0, 3);
        damage_number(lua, 1, "screenWidth", profile_input[1]);
        damage_function(lua, 1, "contains", profile_contains);
        if (profile_input[2]) {
            damage_lua.create_table(lua, 0, 1);
            damage_function(lua, 2, "isInAdventureMode", profile_adventure);
            damage_lua.set_field(lua, 1, "modeSystem");
        }
        damage_lua.set_field(lua, -10002, "gamelua");
        profile_splash_calls = 0;
        damage_lua.get_field(lua, -10002, "_ab_asset_profile");
        damage_lua.push_string(lua, asset_profile_groups[profile_input[4]]);
        if (damage_lua.call(lua, 1, 1, 0) || damage_lua.type(lua, 1) != 4) {
            fail("Original asset-profile selection failed\n");
        }
        unsigned length = 0;
        const char *directory = damage_lua.to_string(lua, 1, &length);
        uint32_t hash = UINT32_C(2166136261);
        for (size_t index = 0; index < length; ++index) {
            hash = (hash ^ (uint8_t)directory[index]) * UINT32_C(16777619);
        }
        uint32_t output[5] = {hash, length, profile_splash_calls, 0, 0};
        damage_lua.set_top(lua, 0);
        damage_lua.get_field(lua, -10002, "_ab_default_profile");
        if (damage_lua.call(lua, 0, 2, 0) || damage_lua.type(lua, 1) != 3 ||
            damage_lua.type(lua, 2) != 3) {
            fail("Original default-profile selection failed\n");
        }
        for (int index = 1; index <= 2; ++index) {
            FloatBits value = {.number = damage_lua.to_number(lua, index)};
            output[2 + (size_t)index] = value.bits;
        }
        print_words(output, 5);
        damage_lua.set_top(lua, 0);
    }
    damage_lua.close_state(lua);
    semihost(4, "AB asset profile reference complete\n");
}

#endif
