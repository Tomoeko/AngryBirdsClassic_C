#ifndef AB_ORACLE_UI_IMAGE_DRAW_H
#define AB_ORACLE_UI_IMAGE_DRAW_H

static uint32_t ui_image_draw_setup[25], ui_image_draw_calls[11][10],
    ui_image_draw_count;

static uint32_t *ui_image_draw_call(uint32_t kind, uint32_t count) {
    if (ui_image_draw_count >= 11) {
        fail("Original Image drawing boundary overflow\n");
    }
    uint32_t *call = ui_image_draw_calls[ui_image_draw_count++];
    for (size_t field = 0; field < 10; ++field) {
        call[field] = 0;
    }
    call[0] = kind;
    call[1] = count;
    return call;
}

static int ui_image_draw_state(void *lua) {
    uint32_t count = (uint32_t)damage_lua.get_top(lua);
    if (count < 7 || count > 8) {
        fail("Unexpected original Image render-state arity\n");
    }
    uint32_t *call = ui_image_draw_call(0, count);
    for (uint32_t field = 0; field < count; ++field) {
        if (damage_lua.type(lua, (int)field + 1) != 3) {
            fail("Missing original Image render-state number\n");
        }
        FloatBits value = {.number = damage_lua.to_number(lua, (int)field + 1)};
        call[2 + field] = value.bits;
    }
    return 0;
}

static int ui_image_draw_sprite(void *lua) {
    if (damage_lua.get_top(lua) != 8 || !damage_named(lua, 1, "") ||
        !damage_named(lua, 2, "controlled-sprite") || !damage_named(lua, 5, "HPIVOT") ||
        !damage_named(lua, 6, "VPIVOT")) {
        fail("Unexpected original Image sprite boundary\n");
    }
    uint32_t *call = ui_image_draw_call(1, 8);
    const int arguments[] = {3, 4, 7, 8};
    for (size_t field = 0; field < 4; ++field) {
        FloatBits value = {.number = damage_lua.to_number(lua, arguments[field])};
        call[2 + field] = value.bits;
    }
    return 0;
}

static void ui_image_draw_prepare(void *lua) {
    damage_lua.create_table(lua, 0, 16);
    const char *geometry[] = {"x", "y", "scaleX", "scaleY", "angle", "w", "h"};
    for (size_t field = 0; field < 7; ++field) {
        damage_number(lua, 1, geometry[field], ui_image_draw_setup[13 + field]);
    }
    if (ui_image_draw_setup[0] & 1) {
        damage_lua.push_string(lua, "controlled-sprite");
    } else {
        damage_lua.push_nil(lua);
    }
    damage_lua.set_field(lua, 1, "image");
    if (ui_image_draw_setup[1] == 0) {
        damage_lua.push_nil(lua);
    } else if (ui_image_draw_setup[1] < 3) {
        damage_lua.push_boolean(lua, ui_image_draw_setup[1] == 2);
    } else {
        damage_lua.push_number(lua, 1);
    }
    damage_lua.set_field(lua, 1, "visible");
    damage_lua.push_boolean(lua, (int)(ui_image_draw_setup[0] & 2));
    damage_lua.set_field(lua, 1, "floorCoordinates");
    const char *pivots[] = {"pivotX", "pivotY", "rotatePivotX", "rotatePivotY"};
    for (size_t field = 0; field < 4; ++field) {
        if (ui_image_draw_setup[3] & (1u << field)) {
            damage_number(lua, 1, pivots[field], ui_image_draw_setup[20 + field]);
        } else {
            damage_lua.push_boolean(lua, 0);
            damage_lua.set_field(lua, 1, pivots[field]);
        }
    }
    if (ui_image_draw_setup[4]) {
        damage_number(lua, 1, "alpha", ui_image_draw_setup[24]);
    }
    damage_lua.set_field(lua, -10002, "_ab_image_draw");
    damage_lua.create_table(lua, 0, 3);
    damage_number(lua, 1, "screenWidth", ui_image_draw_setup[5]);
    damage_number(lua, 1, "screenHeight", ui_image_draw_setup[6]);
    damage_function(lua, 1, "setRenderState", ui_image_draw_state);
    damage_lua.set_field(lua, -10002, "gamelua");
}

static void ui_image_draw_run(void *lua) {
    ui_image_draw_count = 0;
    damage_lua.get_field(lua, -10002, "image-draw-self");
    damage_lua.get_field(lua, -10002, "_ab_image_draw");
    for (size_t field = 0; field < 6; ++field) {
        if (ui_image_draw_setup[2] & (1u << field)) {
            FloatBits value = {.bits = ui_image_draw_setup[7 + field]};
            damage_lua.push_number(lua, value.number);
        } else if (field & 1) {
            damage_lua.push_boolean(lua, 0);
        } else {
            damage_lua.push_nil(lua);
        }
    }
    if (damage_lua.call(lua, 7, 0, 0)) {
        const char *message = damage_lua.to_string(lua, -1, NULL);
        fail(message ? message : "Original Image.drawSelf failed\n");
    }
    print_words(&ui_image_draw_count, 1);
    for (uint32_t index = 0; index < ui_image_draw_count; ++index) {
        print_words(ui_image_draw_calls[index], 10);
    }
    damage_lua.set_top(lua, 0);
}

#if defined(AB_ORACLE_UI_BUTTON_DRAW)
#include "ui_button_draw_probe.h"
#endif
#if defined(AB_ORACLE_UI_IMAGE_RENDER)
#include "ui_image_render_probe.h"
#endif

static void probe_ui_image_draw(void) {
#if defined(AB_ORACLE_UI_IMAGE_RENDER)
    probe_ui_image_render();
    return;
#endif
#if defined(AB_ORACLE_UI_BUTTON_DRAW)
    probe_ui_button_draw();
    return;
#endif
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original Image drawing state\n");
    }
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    (void)damage_lua.open_math(lua);
    damage_lua.set_top(lua, 0);
    load_selected_function(lua, "/data/local/tmp/ab520/ui/image-draw-self-oracle.lua",
                           "image-draw-self");
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 1, "drawSprite", ui_image_draw_sprite);
    damage_lua.set_field(lua, -10002, "res");
    uint32_t random = UINT32_C(0x5201da90);
    semihost(4, "AB_ANDROID_UI_IMAGE_DRAW_V1 4096\n");
    for (size_t item = 0; item < 4096; ++item) {
        ui_image_draw_setup[0] = (random_word(&random) >> 16) & 3;
        ui_image_draw_setup[1] = (uint32_t)((item / 7) % 4);
        ui_image_draw_setup[2] = (uint32_t)(item % 64);
        ui_image_draw_setup[3] = (uint32_t)((item / 64) % 16);
        ui_image_draw_setup[4] = (random_word(&random) >> 16) & 1;
        FloatBits width = {.number = 480}, height = {.number = 320};
        ui_image_draw_setup[5] = width.bits;
        ui_image_draw_setup[6] = height.bits;
        for (size_t field = 7; field < 25; ++field) {
            ui_image_draw_setup[field] = random_float(&random, 117, 20, 1);
            if ((item + field) % 11 == 0) {
                ui_image_draw_setup[field] &= UINT32_C(0x80000000);
            }
        }
        if ((item / 64) % 2 == 0) {
            ui_image_draw_setup[17] =
                ui_image_draw_setup[2] & 16
                    ? ui_image_draw_setup[11] ^ UINT32_C(0x80000000)
                    : 0;
        }
        if (item < 32) {
            ui_image_draw_setup[0] = 3;
            ui_image_draw_setup[1] = 2;
            ui_image_draw_setup[2] = 63;
            ui_image_draw_setup[3] = 15;
            ui_image_draw_setup[4] = (uint32_t)(item % 2);
            for (size_t field = 7; field < 25; ++field) {
                ui_image_draw_setup[field] = 0;
            }
            ui_image_draw_setup[9] = ui_image_draw_setup[10] = UINT32_C(0x3f800000);
            ui_image_draw_setup[12] = UINT32_C(0x3f800000);
            ui_image_draw_setup[15] = ui_image_draw_setup[16] = UINT32_C(0x3f800000);
            ui_image_draw_setup[18] = ui_image_draw_setup[19] = UINT32_C(0x40000000);
            FloatBits x = {.number = item < 16 ? -(float)(item % 8)
                                               : 480 + (float)(item % 8)};
            ui_image_draw_setup[13] = x.bits;
            if (item % 8 == 0) {
                ui_image_draw_setup[15] = ui_image_draw_setup[16] =
                    item & 8 ? UINT32_C(0x80000000) : 0;
            }
            if (item % 8 == 1) {
                ui_image_draw_setup[18] = ui_image_draw_setup[19] = 0;
            }
            if (item % 8 == 2) {
                ui_image_draw_setup[17] = UINT32_C(0x3f800000);
            }
        }
        print_words(ui_image_draw_setup, 25);
        ui_image_draw_prepare(lua);
        ui_image_draw_run(lua);
    }
    damage_lua.close_state(lua);
    semihost(4, "AB UI image draw reference complete\n");
}

#endif
