#ifndef AB_ORACLE_SLINGSHOT_H
#define AB_ORACLE_SLINGSHOT_H

static uint32_t sling_input[11];

static int sling_position(void *lua, bool rest) {
    damage_lua.create_table(lua, 0, 2);
    damage_number(lua, -2, "x", sling_input[rest ? 0 : 2]);
    damage_number(lua, -2, "y", sling_input[rest ? 1 : 3]);
    return 1;
}

static int sling_rest(void *lua) {
    return sling_position(lua, true);
}

static int sling_band(void *lua) {
    return sling_position(lua, false);
}

static int sling_stretch(void *lua) {
    FloatBits value = {.bits = sling_input[4]};
    damage_lua.push_number(lua, value.number);
    return 1;
}

static int sling_power(void *lua) {
    FloatBits value = {.bits = sling_input[9]};
    damage_lua.push_number(lua, value.number);
    return 1;
}

static int sling_definition(void *lua) {
    damage_lua.create_table(lua, 0, 1);
    if (sling_input[10] & 2) {
        damage_number(lua, -2, "slingshotLaunchForce", sling_input[5]);
    }
    return 1;
}

static void prepare_sling_launch(void *lua) {
    if (sling_input[10] & 1) {
        damage_lua.push_string(lua, "first");
    } else {
        damage_lua.push_nil(lua);
    }
    damage_lua.set_field(lua, -10002, "currentBirdName");
    damage_number(lua, -10002, "defaultForce", sling_input[6]);
    damage_number(lua, -10002, "physicsScale", sling_input[7]);
    damage_function(lua, -10002, "getObjectDefinition", sling_definition);
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 1);
    damage_number(lua, -2, "mass", sling_input[8]);
    damage_lua.set_field(lua, -2, "first");
    damage_lua.set_field(lua, -2, "world");
    damage_lua.set_field(lua, -10002, "objects");
    damage_lua.create_table(lua, 0, 4);
    int self = damage_lua.get_top(lua);
    damage_function(lua, self, "getRubberBandPosition", sling_band);
    damage_function(lua, self, "getLevelStartPosition", sling_rest);
    damage_function(lua, self, "getStretchFactor", sling_stretch);
    damage_function(lua, self, "getSlingshotPower", sling_power);
    damage_lua.set_field(lua, -10002, "_ab_sling_self");
}

static void *open_sling_reference(void) {
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original slingshot reference state\n");
    }
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    (void)damage_lua.open_math(lua);
    damage_lua.set_top(lua, 0);
    damage_lua.create_table(lua, 0, 1);
    damage_lua.set_field(lua, -10002, "lua");
    damage_function(lua, -10002, "requireFile", damage_require);
    load_damage_chunk(lua, "/data/local/tmp/ab520/slingshot/class.lua");
    load_damage_chunk(lua, "/data/local/tmp/ab520/slingshot/vector2.lua");
    load_damage_chunk(lua, "/data/local/tmp/ab520/slingshot/utils.lua");
    load_damage_chunk(lua, "/data/local/tmp/ab520/slingshot/slingshot.lua");
    return lua;
}

#if defined(AB_ORACLE_SLING_DRAW_ONLY)
#include "sling_draw_probe.h"
#endif
#if defined(AB_ORACLE_SLING_DELTA_ONLY)
#include "sling_delta_probe.h"
#endif
#if defined(AB_ORACLE_GAME_SLING_MOUSE_ONLY) || defined(AB_ORACLE_GAME_UPDATE_TAIL_ONLY)
#include "game_sling_mouse_probe.h"
#endif
#if defined(AB_ORACLE_GAME_UPDATE_TAIL_ONLY)
#include "game_update_tail_probe.h"
#endif
#if defined(AB_ORACLE_SLING_POSE_ONLY)
#include "sling_pose_probe.h"
#endif
#if defined(AB_ORACLE_SLING_START_POSE_ONLY)
#include "sling_start_pose_probe.h"
#endif

#if defined(AB_ORACLE_SLING_DRAG_ONLY)
#include "sling_drag_probe.h"
#endif

#if defined(AB_ORACLE_SLING_IMPULSE_ONLY)
#include "sling_impulse_probe.h"
#endif
#if defined(AB_ORACLE_SLING_LAUNCH_HOST_ONLY)
#include "sling_launch_host_probe.h"
#endif
#if defined(AB_ORACLE_CLASSIC_SHOT_EFFECTS_ONLY)
#include "classic_shot_effects_probe.h"
#endif
#if defined(AB_ORACLE_SLING_WOBBLE_ONLY)
#include "sling_wobble_probe.h"
#endif

#if defined(AB_ORACLE_BIRD_SHOT_ONLY)
#include "bird_shot_probe.h"
#endif

#if defined(AB_ORACLE_SLING_CANCEL_ONLY)
#include "sling_cancel_probe.h"
#endif

#if defined(AB_ORACLE_SLING_TRANSFER_ONLY)
#include "sling_transfer_probe.h"
#endif

static void probe_slingshot(void) {
#if defined(AB_ORACLE_SLING_WOBBLE_ONLY)
    probe_sling_wobble();
    return;
#endif
#if defined(AB_ORACLE_CLASSIC_SHOT_EFFECTS_ONLY)
    probe_classic_shot_effects();
    return;
#endif
#if defined(AB_ORACLE_SLING_LAUNCH_HOST_ONLY)
    probe_sling_launch_host();
    return;
#endif
#if defined(AB_ORACLE_GAME_UPDATE_TAIL_ONLY)
    probe_game_update_tail();
    return;
#endif
#if defined(AB_ORACLE_GAME_SLING_MOUSE_ONLY)
    probe_game_sling_mouse();
    return;
#endif
#if defined(AB_ORACLE_SLING_DELTA_ONLY)
    probe_sling_delta();
    return;
#endif
#if defined(AB_ORACLE_SLING_START_POSE_ONLY)
    probe_sling_start_pose();
    return;
#endif
#if defined(AB_ORACLE_SLING_POSE_ONLY)
    probe_sling_pose();
    return;
#endif
#if defined(AB_ORACLE_SLING_DRAW_ONLY)
    probe_sling_draw();
    return;
#endif
#if defined(AB_ORACLE_SLING_CANCEL_ONLY)
    probe_sling_cancel();
    return;
#endif
#if defined(AB_ORACLE_BIRD_SHOT_ONLY)
    probe_bird_shot();
    return;
#endif
#if defined(AB_ORACLE_SLING_IMPULSE_ONLY)
    probe_sling_impulse();
    return;
#endif
#if defined(AB_ORACLE_SLING_DRAG_ONLY)
    probe_sling_drag();
    return;
#endif
#if defined(AB_ORACLE_SLING_TRANSFER_ONLY)
    probe_sling_transfer();
    return;
#endif
    void *lua = open_sling_reference();
    uint32_t state = UINT32_C(0x52051a6c);
    semihost(4, "AB_ANDROID_SLINGSHOT_V1 4096\n");
    for (size_t index = 0; index < 4096; ++index) {
        for (size_t field = 0; field < 4; ++field) {
            sling_input[field] = random_float(&state, 122, 10, 1);
        }
        sling_input[4] = random_float(&state, 122, 7, index % 13 == 0);
        sling_input[5] = index % 19 ? random_float(&state, 124, 10, 0) : 0;
        sling_input[6] = random_float(&state, 123, 9, 0);
        sling_input[7] = random_float(&state, 119, 3, 0);
        sling_input[8] = random_float(&state, 124, 4, 0);
        sling_input[9] = random_float(&state, 125, 3, index % 11 == 0);
        sling_input[10] = (uint32_t)index & 3;
        prepare_sling_launch(lua);
        print_words(sling_input, 11);
        damage_lua.get_field(lua, -10002, "SlingshotSystem");
        damage_lua.get_field(lua, -1, "getLaunchVelocity");
        damage_lua.get_field(lua, -10002, "_ab_sling_self");
        if (damage_lua.call(lua, 1, 2, 0)) {
            const char *error = damage_lua.to_string(lua, -1, NULL);
            fail(error ? error : "Original launch velocity failed\n");
        }
        uint32_t output[2];
        for (int field = 0; field < 2; ++field) {
            if (damage_lua.type(lua, field + 2) != 3) {
                fail("Original launch velocity did not return two numbers\n");
            }
            FloatBits value = {.number = damage_lua.to_number(lua, field + 2)};
            output[field] = value.bits;
        }
        damage_lua.set_top(lua, 0);
        print_words(output, 2);
    }
    damage_lua.close_state(lua);
    semihost(4, "AB slingshot reference complete\n");
}

#endif
