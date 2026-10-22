#ifndef AB_ORACLE_BLOCK_COMPONENT_H
#define AB_ORACLE_BLOCK_COMPONENT_H

#include "selected_function.h"

static void component_value(void *lua, uint32_t kind) {
    if (!kind) {
        damage_lua.push_nil(lua);
    } else if (kind <= 2) {
        damage_lua.push_boolean(lua, kind == 2);
    } else if (kind == 3) {
        damage_lua.push_number(lua, 0);
    } else if (kind == 4) {
        damage_lua.push_string(lua, "table");
    } else {
        damage_lua.create_table(lua, 0, 0);
    }
}

static void probe_block_component(void) {
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original component predicate state\n");
    }
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    const char *functions[] = {"_ab_flame", "_ab_animations", "_ab_particles"};
    const char *paths[] = {"/data/local/tmp/ab520/block-event/can-flame.lua",
                           "/data/local/tmp/ab520/block-event/can-animations.lua",
                           "/data/local/tmp/ab520/block-event/can-particles.lua"};
    const char *fields[] = {"flame", "ambientAnimations", "ambientParticles"};
    for (size_t index = 0; index < 3; ++index) {
        load_selected_function(lua, paths[index], functions[index]);
    }
    semihost(4, "AB_ANDROID_BLOCK_COMPONENT_V1 4096\n");
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[] = {(uint32_t)(item % 6), (uint32_t)((item / 6) % 6),
                            (uint32_t)((item / 36) % 6), (uint32_t)((item / 216) % 6)};
        damage_lua.create_table(lua, 0, 1);
        if (input[0] == 5) {
            damage_lua.create_table(lua, 0, 3);
            for (size_t index = 0; index < 3; ++index) {
                component_value(lua, input[index + 1]);
                damage_lua.set_field(lua, 2, fields[index]);
            }
        } else {
            component_value(lua, input[0]);
        }
        damage_lua.set_field(lua, 1, "components");
        damage_lua.set_field(lua, -10002, "_ab_definition");
        uint32_t output[3];
        for (size_t index = 0; index < 3; ++index) {
            damage_lua.get_field(lua, -10002, functions[index]);
            damage_lua.push_nil(lua);
            damage_lua.get_field(lua, -10002, "_ab_definition");
            if (damage_lua.call(lua, 2, 1, 0) || damage_lua.type(lua, 1) != 1) {
                fail("Original block component predicate failed\n");
            }
            output[index] = (uint32_t)damage_lua.to_boolean(lua, 1);
            damage_lua.set_top(lua, 0);
        }
        print_words(input, 4);
        print_words(output, 3);
    }
    damage_lua.close_state(lua);
    semihost(4, "AB block component reference complete\n");
}

#endif
