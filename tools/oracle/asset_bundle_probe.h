#ifndef AB_ORACLE_ASSET_BUNDLE_H
#define AB_ORACLE_ASSET_BUNDLE_H

#include "../../tests/asset_bundle_cases.h"

static uint32_t bundle_require_count, bundle_event_count, bundle_events[15][2];
static void (*bundle_set_index)(void *, int, int);

static int bundle_require(void *lua) {
    if (damage_lua.get_top(lua) != 1 ||
        !damage_named(lua, 1, "platform/Profiles.lua")) {
        fail("Unexpected original asset-bundle require boundary\n");
    }
    ++bundle_require_count;
    return 0;
}

static int bundle_manage(void *lua) {
    if (damage_lua.get_top(lua) != 2 || damage_lua.type(lua, 1) != 4 ||
        damage_lua.type(lua, 2) != 1 || bundle_event_count == 15) {
        fail("Unexpected original expanded group boundary\n");
    }
    uint32_t name = 5;
    for (uint32_t index = 0; index < 5; ++index) {
        if (damage_named(lua, 1, bundle_names[index])) {
            name = index;
        }
    }
    if (name == 5) {
        fail("Unknown original expanded group name\n");
    }
    bundle_events[bundle_event_count][0] = name;
    bundle_events[bundle_event_count++][1] = (uint32_t)damage_lua.to_boolean(lua, 2);
    return 0;
}

static void bundle_load_function(void *lua, const char *path, size_t size,
                                 const char *name) {
    char bytes[316];
    int file = system_call(5, (int)(uintptr_t)path, NULL, 0);
    size_t offset = 0;
    if (file < 0 || size > sizeof(bytes)) {
        fail("Cannot open original asset-bundle function\n");
    }
    while (offset < size) {
        int count = system_call(3, file, bytes + offset, size - offset);
        if (count <= 0 || (size_t)count > size - offset) {
            fail("Incomplete original asset-bundle function\n");
        }
        offset += (size_t)count;
    }
    char extra;
    if (system_call(3, file, &extra, 1) != 0 || system_call(6, file, NULL, 0) != 0 ||
        damage_lua.load(lua, bytes, (unsigned)size, name)) {
        fail("Cannot load bounded original asset-bundle function\n");
    }
    damage_lua.set_field(lua, -10002, name);
}

static void bundle_prepare(void *lua, const uint32_t input[28]) {
    damage_lua.set_top(lua, 0);
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 5);
    for (uint32_t bundle = 0; bundle < 5; ++bundle) {
        if (!(input[1] & (1u << bundle))) {
            continue;
        }
        damage_lua.create_table(lua, 0, 3);
        for (uint32_t member = 0; member < 3; ++member) {
            if (input[8 + bundle] & (1u << member)) {
                damage_lua.push_string(lua,
                                       bundle_names[input[13 + bundle * 3 + member]]);
                bundle_set_index(lua, 3, (int)member + 1);
            }
        }
        damage_lua.set_field(lua, 2, bundle_names[bundle]);
    }
    damage_lua.set_field(lua, 1, "Profiles");
    damage_lua.set_field(lua, -10002, "platform");
    damage_lua.create_table(lua, 0, 5);
    for (uint32_t item = 0; item < 5; ++item) {
        if (input[2] & (1u << item)) {
            damage_lua.push_string(lua, bundle_names[input[3 + item]]);
            bundle_set_index(lua, 1, (int)item + 1);
        }
    }
    damage_lua.set_field(lua, -10002, "_ab_bundle_requested");
}

static void probe_asset_bundle(void) {
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot create original asset-bundle state\n");
    }
    damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    bundle_set_index =
        (void (*)(void *, int, int))find_function("_Z11lua_rawsetiP9lua_Stateii");
    damage_function(lua, -10002, "requireFile", bundle_require);
    damage_function(lua, -10002, "manageAssets", bundle_manage);
    bundle_load_function(lua, "/data/local/tmp/ab520/load-assets.lua", 316,
                         "_ab_bundle_load");
    bundle_load_function(lua, "/data/local/tmp/ab520/release-assets.lua", 299,
                         "_ab_bundle_release");
    semihost(4, "AB_ANDROID_ASSET_BUNDLE_V1 4096\n");
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        uint32_t input[28];
        asset_bundle_case(sample, input);
        print_words(input, 28);
        bundle_prepare(lua, input);
        bundle_require_count = bundle_event_count = 0;
        damage_lua.get_field(lua, -10002,
                             input[0] ? "_ab_bundle_load" : "_ab_bundle_release");
        damage_lua.get_field(lua, -10002, "_ab_bundle_requested");
        if (damage_lua.call(lua, 1, 0, 0)) {
            const char *message = damage_lua.to_string(lua, -1, NULL);
            fail(message ? message : "Original asset-bundle dispatch failed\n");
        }
        uint32_t counts[2] = {bundle_require_count, bundle_event_count};
        print_words(counts, 2);
        for (uint32_t event = 0; event < bundle_event_count; ++event) {
            print_words(bundle_events[event], 2);
        }
        damage_lua.set_top(lua, 0);
    }
    damage_lua.close_state(lua);
    semihost(4, "AB asset bundle reference complete\n");
}

#endif
