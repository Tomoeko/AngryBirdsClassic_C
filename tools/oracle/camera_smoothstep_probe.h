#ifndef AB_ORACLE_CAMERA_SMOOTHSTEP_H
#define AB_ORACLE_CAMERA_SMOOTHSTEP_H

#include "../../tests/camera_smoothstep_cases.h"

static void probe_camera_smoothstep(void) {
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original camera smoothstep state\n");
    }
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    (void)damage_lua.open_math(lua);
    damage_lua.set_top(lua, 0);
    const char *const names[] = {"clamp", "tweenLinear", "smoothstep",
                                 "easeSmoothStep"};
    const char *const paths[] = {
        "/data/local/tmp/ab520/camera/follow-clamp.lua",
        "/data/local/tmp/ab520/camera/follow-linear.lua",
        "/data/local/tmp/ab520/camera/follow-smoothstep.lua",
        "/data/local/tmp/ab520/camera/follow-ease-smoothstep.lua"};
    for (size_t index = 0; index < 4; ++index) {
        char chunk[512];
        int file = system_call(5, (int)(uintptr_t)paths[index], NULL, 0);
        int size = file < 0 ? -1 : system_call(3, file, chunk, sizeof(chunk));
        if (size <= 0 || size == sizeof(chunk) || system_call(6, file, NULL, 0) ||
            damage_lua.load(lua, chunk, (unsigned)size, "original-camera-easing")) {
            fail("Cannot load original camera smoothstep helper\n");
        }
        damage_lua.set_field(lua, -10002, names[index]);
    }
    semihost(4, "AB_ANDROID_CAMERA_SMOOTHSTEP_V1 4096\n");
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        uint32_t input[8], output[2] = {0};
        camera_smoothstep_case(sample, input);
        print_words(input, 8);
        for (size_t method = 0; method < 2; ++method) {
            damage_lua.get_field(lua, -10002, names[method + 2]);
            uint32_t first = method ? 4u : 1u;
            uint32_t count = method ? 4u : 3u;
            for (uint32_t index = first; index < first + count; ++index) {
                FloatBits value = {.bits = input[index]};
                damage_lua.push_number(lua, value.number);
            }
            if (damage_lua.call(lua, (int)count, 1, 0) ||
                damage_lua.type(lua, 1) != 3) {
                fail("Original camera smoothstep helper failed\n");
            }
            FloatBits value = {.number = damage_lua.to_number(lua, 1)};
            output[method] = value.bits;
            damage_lua.set_top(lua, 0);
        }
        print_words(output, 2);
    }
    damage_lua.close_state(lua);
    semihost(4, "AB camera smoothstep reference complete\n");
}

#endif
