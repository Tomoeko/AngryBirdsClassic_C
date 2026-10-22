#ifndef AB_ORACLE_ASSET_PROFILE_BUNDLES_H
#define AB_ORACLE_ASSET_PROFILE_BUNDLES_H

#include "../../tests/asset_profile_bundle_names.h"
#include "asset_profiles_private.h"

static void probe_asset_profile_bundles(void) {
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot create original Android bundle state\n");
    }
    damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    damage_function(lua, -10002, "requireFile", damage_require);
    void (*get_index)(void *, int, int) =
        (void (*)(void *, int, int))find_function("_Z11lua_rawgetiP9lua_Stateii");
    semihost(4, "AB_ANDROID_PROFILE_BUNDLES_V1 4\n");
    for (uint32_t sample = 0; sample < 4; ++sample) {
        uint32_t flags[2] = {sample & 1u, sample >> 1};
        print_words(flags, 2);
        damage_lua.create_table(lua, 0, 0);
        damage_lua.set_field(lua, -10002, "platform");
        damage_lua.create_table(lua, 0, 2);
        damage_lua.push_boolean(lua, (int)flags[0]);
        damage_lua.set_field(lua, 1, "isFreemiumChina");
        damage_lua.push_boolean(lua, (int)flags[1]);
        damage_lua.set_field(lua, 1, "Sugotoku");
        damage_lua.set_field(lua, -10002, "gamelua");
        android_load_profiles(lua);
        damage_lua.get_field(lua, -10002, "platform");
        damage_lua.get_field(lua, 1, "Profiles");
        for (size_t key = 0; key < 6; ++key) {
            uint32_t output[4] = {0};
            damage_lua.get_field(lua, 2, profile_bundle_keys[key]);
            int type = damage_lua.type(lua, 3);
            if (type == 5) {
                output[0] = 1;
                for (int member = 1; member <= 4; ++member) {
                    get_index(lua, 3, member);
                    if (damage_lua.type(lua, 4) == 0) {
                        break;
                    }
                    if (member == 4 || damage_lua.type(lua, 4) != 4) {
                        fail("Unexpected original Android bundle member\n");
                    }
                    uint32_t id = 0;
                    for (uint32_t name = 0; name < 13; ++name) {
                        if (damage_named(lua, 4, profile_bundle_members[name])) {
                            id = name + 1;
                        }
                    }
                    if (!id) {
                        fail("Unknown original Android bundle resource name\n");
                    }
                    output[member] = id;
                    damage_lua.set_top(lua, 3);
                }
            } else if (type != 0) {
                fail("Original named Android bundle is neither table nor nil\n");
            }
            print_words(output, 4);
            damage_lua.set_top(lua, 2);
        }
        damage_lua.set_top(lua, 0);
    }
    damage_lua.close_state(lua);
    semihost(4, "AB Android profile bundles reference complete\n");
}

#endif
