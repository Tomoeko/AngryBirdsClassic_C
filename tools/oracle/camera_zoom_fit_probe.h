#ifndef AB_ORACLE_CAMERA_ZOOM_FIT_H
#define AB_ORACLE_CAMERA_ZOOM_FIT_H

#include "../../tests/camera_zoom_fit_cases.h"

static uint32_t fit_native_input[8], fit_native_output[6];

static int fit_native_project(void *lua) {
    uintptr_t ci = *(const uint32_t *)((const uint8_t *)lua + 0x14);
    uintptr_t base = *(const uint32_t *)((const uint8_t *)lua + 0x28);
    if (ci <= base || (ci - base) % 24u || damage_lua.get_top(lua) != 2) {
        fail("Invalid original zoom-fit projection boundary\n");
    }
    const uint32_t *caller = (const uint32_t *)(ci - 24u);
    const uint32_t *function = (const uint32_t *)(uintptr_t)caller[1];
    if (!function || function[1] != 6 || !function[0]) {
        fail("Missing original zoom-fit caller\n");
    }
    const uint8_t *closure = (const uint8_t *)(uintptr_t)function[0];
    const uint32_t *prototype =
        (const uint32_t *)(uintptr_t)((const uint32_t *)closure)[4];
    if (closure[6] || closure[7] || !prototype || prototype[11] != 36 ||
        caller[3] != prototype[3] + 16u) {
        fail("Unexpected original zoom-fit caller PC/prototype\n");
    }
    const uint32_t *code = (const uint32_t *)(uintptr_t)prototype[3];
    if (code[0] != UINT32_C(0x000000a4) || code[35] != UINT32_C(0x0080001e)) {
        fail("Unexpected original zoom-fit instructions\n");
    }
    damage_lua.get_field(lua, -10002, "_ab_fit_state");
    damage_lua.get_field(lua, -10002, "_ab_fit_point");
    if (damage_lua.to_pointer(lua, 1) != damage_lua.to_pointer(lua, 3) ||
        damage_lua.to_pointer(lua, 2) != damage_lua.to_pointer(lua, 4)) {
        fail("Original zoom-fit projection changed argument identity\n");
    }
    damage_lua.set_top(lua, 2);
    ++fit_native_output[0];
    fit_native_output[1] = 3;
    fit_native_output[2] = 801;
    const char *const axes[] = {"x", "y"};
    for (int index = 0; index < 2; ++index) {
        damage_lua.get_field(lua, 2, axes[index]);
        if (damage_lua.type(lua, -1) != 3) {
            fail("Original zoom-fit point is not numeric\n");
        }
        FloatBits value = {.number = damage_lua.to_number(lua, -1)};
        fit_native_output[3 + index] = value.bits;
        damage_lua.set_top(lua, 2);
    }
    FloatBits x = {.bits = fit_native_input[1]}, y = {.bits = fit_native_input[2]};
    damage_lua.push_number(lua, x.number);
    damage_lua.push_number(lua, y.number);
    return 2;
}

static void probe_camera_zoom_fit(void) {
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original camera zoom-fit state\n");
    }
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    (void)damage_lua.open_math(lua);
    damage_lua.set_top(lua, 0);
    const char *path = "/data/local/tmp/ab520/camera/follow-zoom-fit.lua";
    char chunk[2048];
    int file = system_call(5, (int)(uintptr_t)path, NULL, 0);
    int size = file < 0 ? -1 : system_call(3, file, chunk, sizeof(chunk));
    if (size <= 0 || size == sizeof(chunk) || system_call(6, file, NULL, 0) ||
        damage_lua.load(lua, chunk, (unsigned)size, "original-camera-zoom-fit")) {
        fail("Cannot load original camera zoom-fit method\n");
    }
    damage_lua.set_field(lua, -10002, "_ab_fit_method");
    semihost(4, "AB_ANDROID_CAMERA_ZOOM_FIT_V1 4096\n");
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        camera_zoom_fit_case(sample, fit_native_input);
        print_words(fit_native_input, 8);
        for (uint32_t index = 0; index < 6; ++index) {
            fit_native_output[index] = 0;
        }
        damage_lua.create_table(lua, 0, 1);
        damage_number(lua, 1, "screenHeight", fit_native_input[6]);
        damage_lua.set_field(lua, -10002, "gamelua");
        damage_lua.create_table(lua, 0, 3);
        damage_number(lua, 1, "closeZoom", fit_native_input[5]);
        damage_number(lua, 1, "bottomBorder", fit_native_input[7]);
        damage_function(lua, 1, "projectWantedValuesDown", fit_native_project);
        damage_lua.set_field(lua, -10002, "_ab_fit_state");
        damage_lua.create_table(lua, 0, 2);
        damage_number(lua, 1, "x", fit_native_input[3]);
        damage_number(lua, 1, "y", fit_native_input[4]);
        damage_lua.set_field(lua, -10002, "_ab_fit_point");
        damage_lua.get_field(lua, -10002, "_ab_fit_method");
        damage_lua.get_field(lua, -10002, "_ab_fit_state");
        damage_lua.get_field(lua, -10002, "_ab_fit_point");
        if (damage_lua.call(lua, 2, 1, 0) || damage_lua.type(lua, 1) != 3) {
            fail("Original camera zoom-fit method failed\n");
        }
        FloatBits value = {.number = damage_lua.to_number(lua, 1)};
        fit_native_output[5] = value.bits;
        print_words(fit_native_output, 6);
        damage_lua.set_top(lua, 0);
    }
    damage_lua.close_state(lua);
    semihost(4, "AB camera zoom fit reference complete\n");
}

#endif
