#ifndef AB_ORACLE_ASSET_SPLASH_H
#define AB_ORACLE_ASSET_SPLASH_H

#include "../../tests/asset_splash_cases.h"
#include "asset_profiles_private.h"

static void probe_asset_splash(void) {
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot create original splash profile state\n");
    }
    damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    damage_lua.create_table(lua, 0, 0);
    damage_lua.set_field(lua, -10002, "platform");
    damage_lua.create_table(lua, 0, 0);
    damage_lua.set_field(lua, -10002, "gamelua");
    damage_function(lua, -10002, "requireFile", damage_require);
    android_load_profiles(lua);
    damage_lua.get_field(lua, -10002, "platform");
    damage_lua.get_field(lua, 1, "Profiles");
    damage_lua.get_field(lua, 2, "selectSplashResolution");
    damage_lua.set_field(lua, -10002, "_ab_splash_resolution");
    damage_lua.get_field(lua, 2, "selectSplashProfile");
    damage_lua.set_field(lua, -10002, "_ab_splash_profile");
    damage_lua.set_top(lua, 0);
    semihost(4, "AB_ANDROID_ASSET_SPLASH_V1 4096\n");
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        uint32_t input[2], output[5] = {0};
        asset_splash_case(sample, input);
        print_words(input, 2);
        damage_lua.create_table(lua, 0, 2);
        damage_number(lua, 1, "screenWidth", input[0]);
        damage_number(lua, 1, "screenHeight", input[1]);
        damage_lua.set_field(lua, -10002, "gamelua");
        damage_lua.get_field(lua, -10002, "_ab_splash_resolution");
        if (damage_lua.call(lua, 0, 2, 0)) {
            fail("Original splash resolution unexpectedly raised an error\n");
        }
        if (damage_lua.type(lua, 1) == 3 && damage_lua.type(lua, 2) == 3) {
            output[0] = 1;
            for (int field = 1; field <= 2; ++field) {
                FloatBits value = {.number = damage_lua.to_number(lua, field)};
                output[field] = value.bits;
            }
        } else if (damage_lua.type(lua, 1) != 0 || damage_lua.type(lua, 2) != 0) {
            fail("Unexpected original splash resolution return types\n");
        }
        damage_lua.set_top(lua, 0);
        damage_lua.get_field(lua, -10002, "_ab_splash_profile");
        int failed = damage_lua.call(lua, 0, 1, 0);
        if (!failed) {
            output[3] = 1;
            for (uint32_t profile = 0; profile < 4; ++profile) {
                if (damage_named(lua, 1, splash_profiles[profile])) {
                    output[4] = profile + 1;
                }
            }
            if (!output[4]) {
                fail("Unexpected original splash profile directory\n");
            }
        }
        if ((output[0] != 0) != (failed == 0)) {
            fail("Original splash resolution/profile availability disagreed\n");
        }
        damage_lua.set_top(lua, 0);
        print_words(output, 5);
    }
    damage_lua.close_state(lua);
    semihost(4, "AB asset splash reference complete\n");
}

#endif
