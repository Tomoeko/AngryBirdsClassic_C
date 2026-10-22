#ifndef AB_ORACLE_UI_TEXT_H
#define AB_ORACLE_UI_TEXT_H

/* This probes the native unclipped, uncolored text route with a prepared
 * current bitmap font. Empty resource maps deliberately retain that font and
 * return label bytes directly. Native Lua conversions, UTF decoding, text
 * transform setup, alpha changes and glyph placement execute unchanged. */
#include "font_metrics_probe.h"
#if defined(AB_ORACLE_UI_TEXT_LINES)
#include "ui_text_lines_probe.h"
#endif
#if defined(AB_ORACLE_UI_TEXT_METRICS)
#include "ui_text_metrics_probe.h"
#endif
#if defined(AB_ORACLE_UI_TEXT_BOUNDS)
#include "ui_text_bounds_probe.h"
#endif
#if defined(AB_ORACLE_UI_TEXT_WRAP)
#include "ui_text_wrap_probe.h"
#endif
#if defined(AB_ORACLE_UI_TEXT_VISIBILITY)
#include "ui_text_visibility_probe.h"
#endif
#if defined(AB_ORACLE_UI_TEXT_CLIP)
#include "ui_text_clip_probe.h"
#endif

static uint32_t ui_text_events[32][68], ui_text_event_count;

static uint32_t *ui_text_context_state(const uint32_t *context) {
#if defined(AB_ORACLE_UI_TEXT_CLIP)
    ui_text_clip_context_access((uintptr_t)__builtin_return_address(0));
#endif
    return (uint32_t *)(uintptr_t)context[1];
}

#if !defined(AB_ORACLE_UI_TEXT_IMAGE)
static void ui_text_glyph(void *data, const uint32_t *context, const uint32_t *sprite,
                          float x, float y) {
    (void)data;
    if (ui_text_event_count == 32) {
        fail("Native UI text glyph output exceeds bounds\n");
    }
    uint32_t glyph = 0;
    while (glyph < 7 && sprite != metric_sprites[glyph]) {
        ++glyph;
    }
    if (glyph == 7) {
        fail("Native UI text selected an unknown glyph\n");
    }
    FloatBits px = {.number = x}, py = {.number = y};
    uint32_t *event = ui_text_events[ui_text_event_count++];
    event[0] = glyph;
    event[1] = px.bits;
    event[2] = py.bits;
    const uint32_t *state = ui_text_context_state(context);
    for (size_t field = 0; field < 24; ++field) {
        event[3 + field] = state[field];
    }
}
#else
static uint32_t ui_text_images[7][5], ui_text_sheets[7][4], ui_text_image_vtable[7];
static uint32_t ui_text_viewport[4] = {0, 0, 640, 480};
static uint32_t ui_text_batcher[54], ui_text_shader[2];
static uint32_t ui_text_vertices[24], ui_text_uvs[16];
static void (*ui_text_gl_image)(void *, void *, float, float, float, float,
                                const void *, void *, const void *);

static uint32_t *ui_text_context_viewport(const uint32_t *context) {
    (void)context;
    return ui_text_viewport;
}

static void ui_text_image(uint32_t *image, uint32_t *context, float x, float y,
                          float width, float height, const uint32_t *uv, void *shader,
                          const void *color) {
    if (ui_text_event_count == 32 || image[1] >= 7 || shader || color) {
        fail("Native UI text image boundary exceeds audited inputs\n");
    }
    uint32_t *event = ui_text_events[ui_text_event_count++];
    for (size_t field = 0; field < 68; ++field) {
        event[field] = 0;
    }
    FloatBits px = {.number = x}, py = {.number = y};
    FloatBits w = {.number = width}, h = {.number = height};
    event[0] = image[1];
    event[1] = px.bits;
    event[2] = py.bits;
    event[3] = w.bits;
    event[4] = h.bits;
    const uint32_t *state = ui_text_context_state(context);
    for (size_t field = 0; field < 24; ++field) {
        event[5 + field] = state[field];
    }
    for (size_t field = 0; field < 8; ++field) {
        event[29 + field] = uv[field];
    }
    ui_text_batcher[18] = state[16];
    ui_text_batcher[41] = 2;
    ui_text_batcher[42] = (uint32_t)(uintptr_t)context;
    ui_text_batcher[43] = (uint32_t)(uintptr_t)image;
    ui_text_batcher[44] = (uint32_t)(uintptr_t)ui_text_shader;
    ui_text_batcher[45] = (uint32_t)(uintptr_t)ui_text_vertices;
    ui_text_batcher[46] = (uint32_t)(uintptr_t)(ui_text_vertices + 3);
    ui_text_batcher[47] = (uint32_t)(uintptr_t)(ui_text_vertices + 24);
    ui_text_batcher[48] = (uint32_t)(uintptr_t)ui_text_uvs;
    ui_text_batcher[49] = (uint32_t)(uintptr_t)(ui_text_uvs + 2);
    ui_text_batcher[50] = (uint32_t)(uintptr_t)(ui_text_uvs + 16);
    /* The supplied shader token bypasses platform shader selection. Geometry,
     * culling and batch emission execute in original GL_Image/RenderBatcher. */
    ui_text_gl_image(image, context, x, y, width, height, uv, ui_text_shader, NULL);
    uint32_t drawn =
        ui_text_batcher[46] == (uint32_t)(uintptr_t)(ui_text_vertices + 21);
    if ((!drawn &&
         ui_text_batcher[46] != (uint32_t)(uintptr_t)(ui_text_vertices + 3)) ||
        ui_text_batcher[49] != (uint32_t)(uintptr_t)(ui_text_uvs + (drawn ? 14 : 2))) {
        fail("Native UI text batch exceeds reserved output\n");
    }
    event[37] = drawn;
    if (drawn) {
        for (size_t field = 0; field < 18; ++field) {
            event[38 + field] = ui_text_vertices[3 + field];
        }
        for (size_t field = 0; field < 12; ++field) {
            event[56 + field] = ui_text_uvs[2 + field];
        }
    }
}
#endif

static void ui_text_visible(void *lua, uint32_t kind) {
    if (kind == 0) {
        damage_lua.push_nil(lua);
    } else if (kind < 3) {
        damage_lua.push_boolean(lua, kind == 2);
    } else if (kind < 5) {
        damage_lua.push_number(lua, kind == 3 ? 0 : 1);
    } else {
        damage_lua.push_string(lua, kind == 5 ? "" : "true");
    }
    damage_lua.set_field(lua, 1, "visible");
}

static void probe_ui_text(void) {
#if defined(AB_ORACLE_UI_TEXT_BOUNDS)
    probe_ui_text_bounds();
    return;
#endif
#if defined(AB_ORACLE_UI_TEXT_METRICS)
    probe_ui_text_metrics();
    return;
#endif
#if defined(AB_ORACLE_UI_TEXT_LINES)
    probe_ui_text_lines();
    return;
#endif
#if defined(AB_ORACLE_UI_TEXT_WRAP)
    probe_ui_text_wrap();
    return;
#endif
#if defined(AB_ORACLE_UI_TEXT_VISIBILITY)
    probe_ui_text_visibility();
    return;
#endif
    initialize_damage_lua();
    void *parent = damage_lua.new_state();
    if (!parent) {
        fail("Cannot allocate native UI text state\n");
    }
    void (*construct_state)(void *, void *) =
        (void (*)(void *, void *))find_function("_ZN3lua8LuaStateC1EP9lua_State");
    void (*claim)(void *) = (void (*)(void *))find_function("_ZN4lang6Object5claimEv");
    int (*draw)(void *, void *) = (int (*)(void *, void *))find_function(
        "_ZN7GameLua10drawUITextEPN3lua8LuaStateE");
    _Alignas(8) uint32_t wrapper[8] = {0};
    construct_state(wrapper, parent);
    claim(wrapper);
    void *lua = (void *)(uintptr_t)wrapper[2];
    _Alignas(8) uint32_t game[0x1984 / 4] = {0};
    _Alignas(8) uint32_t resources[0x4b0 / 4] = {0};
    uint32_t context_vtable[29] = {0}, state[39] = {0};
    context_vtable[24] = (uint32_t)(uintptr_t)ui_text_context_state;
    uint32_t context[14] = {(uint32_t)(uintptr_t)context_vtable,
                            (uint32_t)(uintptr_t)state};
#if defined(AB_ORACLE_UI_TEXT_CLIP)
    ui_text_clip_raw_set =
        (void (*)(void *, int, int))find_function("_Z11lua_rawsetiP9lua_Stateii");
    context_vtable[28] = (uint32_t)(uintptr_t)ui_text_clip_screen_height;
    ui_text_clip_internal_start = find_function(
        "_ZN7GameLua18drawUITextInternalERKN3lua8LuaTableEffffffffSsSsPN4game6AnchorE");
    ui_text_clip_internal_end = (uintptr_t)draw;
    if (ui_text_clip_internal_end <= ui_text_clip_internal_start ||
        ui_text_clip_internal_end - ui_text_clip_internal_start > 65536) {
        fail("Unexpected native UI text function interval\n");
    }
#endif
    uint32_t font[18] = {0};
    font[0] = (uint32_t)find_symbol("_ZTVN4game10BitmapFontE", 1) + 8;
    font[5] = metric_tree(0, 7, (uint32_t)(uintptr_t)(font + 4));
    font[6] = (uint32_t)(uintptr_t)metric_nodes[0];
    font[7] = (uint32_t)(uintptr_t)metric_nodes[6];
    font[8] = 7;
    font[10] = 1;
    font[11] = 20;
    font[12] = 5;
    font[13] = 30;
#if defined(AB_ORACLE_UI_TEXT_IMAGE)
    context_vtable[25] = (uint32_t)(uintptr_t)ui_text_context_viewport;
    context[13] = (uint32_t)(uintptr_t)ui_text_batcher;
    ui_text_gl_image = (void (*)(void *, void *, float, float, float, float,
                                 const void *, void *, const void *))
        find_function("_ZN2gr5gles28GL_Image4drawEPNS_7ContextEffffPKN4math6float2EPNS_"
                      "6ShaderEPNS4_6float4E");
    ui_text_image_vtable[6] = (uint32_t)(uintptr_t)ui_text_image;
#else
    font[16] = 1;
    font[17] = (uint32_t)(uintptr_t)ui_text_glyph;
#endif
    for (size_t glyph = 0; glyph < 7; ++glyph) {
        metric_sprites[glyph][5] =
            (uint32_t)(8 + glyph) | ((uint32_t)(12 + glyph) << 16);
        FloatBits pivot = {.number = (float)glyph - 2};
        metric_sprites[glyph][6] = 0;
        metric_sprites[glyph][7] = pivot.bits;
#if defined(AB_ORACLE_UI_TEXT_IMAGE)
        ui_text_images[glyph][0] = (uint32_t)(uintptr_t)ui_text_image_vtable;
        ui_text_images[glyph][1] = (uint32_t)glyph;
        ui_text_images[glyph][3] = (uint32_t)(uintptr_t)context;
        ui_text_sheets[glyph][3] = (uint32_t)(uintptr_t)ui_text_images[glyph];
        metric_sprites[glyph][2] = (uint32_t)(uintptr_t)ui_text_sheets[glyph];
        float left = (float)(glyph * 10) / 256;
        float top = (float)(glyph * 3) / 256;
        float right = (float)(glyph * 10 + 8 + glyph) / 256;
        float bottom = (float)(glyph * 3 + 12 + glyph) / 256;
        const float uv[] = {left, top, right, top, left, bottom, right, bottom};
        for (size_t field = 0; field < 8; ++field) {
            FloatBits value = {.number = uv[field]};
            metric_sprites[glyph][8 + field] = value.bits;
        }
#endif
    }
    game[0x44 / 4] = (uint32_t)(uintptr_t)context;
    game[0x50 / 4] = (uint32_t)(uintptr_t)resources;
    resources[0xc / 4] = (uint32_t)(uintptr_t)context;
    resources[0x28 / 4] = (uint32_t)(uintptr_t)font;
    static const char *const horizontal[] = {"LEFT", "HCENTER", "RIGHT", "HPIVOT"};
    static const char *const vertical[] = {"TOP", "VCENTER", "BOTTOM", "BASELINE",
                                           "VPIVOT"};
    static const char *const texts[] = {
        "",   "AB",  "a0 B", "A\nB", "missing", "\xe4\xb8\x80\xf0\x9f\x98\x80",
        "  ", "A0aB"};
    static const uint32_t scalars[] = {0,          0x80000000, 0x3e000000, 0xbe000000,
                                       0x3f000000, 0xbf000000, 0x3f800000, 0xbf800000,
                                       0x40000000, 0xc0000000, 0x40800000, 0xc0800000};
    uint32_t random = 0x5207e470;
#if defined(AB_ORACLE_UI_TEXT_IMAGE)
    semihost(4, "AB_ANDROID_UI_TEXT_IMAGE_V1 4096\n");
#elif defined(AB_ORACLE_UI_TEXT_CLIP)
    semihost(4, "AB_ANDROID_UI_TEXT_CLIP_V1 4096\n");
#else
    semihost(4, "AB_ANDROID_UI_TEXT_V1 4096\n");
#endif
    for (size_t test = 0; test < 4096; ++test) {
        uint32_t input[46] = {0};
        input[0] = (uint32_t)test % 7;
        input[1] = random_word(&random) % 2;
        input[2] = random_word(&random) % 2;
        input[3] = random_word(&random) % 4;
        input[4] = 2 + (uint32_t)(test / 7) % 5;
        input[5] = random_word(&random) % 4;
        input[6] = random_word(&random) % 5;
        input[7] = random_word(&random) % 8;
        input[8] = random_word(&random) % 3;
        for (size_t field = 9; field < 22; ++field) {
            input[field] = random_float(&random, 122, 10, 1);
        }
        input[11] = scalars[2 + random_word(&random) % 10];
        input[12] = scalars[2 + random_word(&random) % 10];
        input[18] = scalars[2 + random_word(&random) % 10];
        input[19] = scalars[2 + random_word(&random) % 10];
        input[21] = scalars[2 + random_word(&random) % 10];
        for (size_t field = 22; field < 46; ++field) {
            input[field] = random_float(&random, 122, 10, 1);
#if defined(AB_ORACLE_UI_TEXT_IMAGE)
            if (field != 30 && field != 31) {
                input[field] = 0;
            }
            if (field == 26 || field == 29 || field == 32 || field == 33 ||
                field == 38) {
                input[field] = 0x3f800000;
            }
            if (field == 41) {
                input[field] = test % 5 == 0;
            }
#endif
            state[field - 22] = input[field];
        }
        damage_lua.set_top(lua, 0);
        damage_lua.create_table(lua, 0, 16);
        ui_text_visible(lua, input[0]);
        damage_lua.push_boolean(lua, (int)input[1]);
        damage_lua.set_field(lua, 1, "floorCoordinates");
        const char *fields[] = {"x", "y", "scaleX", "scaleY"};
        for (size_t field = 0; field < 4; ++field) {
            damage_number(lua, 1, fields[field], input[9 + field]);
        }
        if (input[2]) {
            damage_number(lua, 1, "alpha", input[13]);
        }
        if (input[3] & 1) {
            damage_number(lua, 1, "rotationPivotX", input[14]);
        }
        if (input[3] & 2) {
            damage_number(lua, 1, "rotationPivotY", input[15]);
        }
        damage_lua.push_string(lua, horizontal[input[5]]);
        damage_lua.set_field(lua, 1, "hanchor");
        damage_lua.push_string(lua, vertical[input[6]]);
        damage_lua.set_field(lua, 1, "vanchor");
        damage_lua.push_string(lua, texts[input[7]]);
        damage_lua.set_field(lua, 1, "text");
        damage_lua.push_string(lua, "");
        damage_lua.set_field(lua, 1, "group");
        if (input[8] == 1) {
            damage_lua.push_string(lua, "FONT_BASIC");
        } else if (input[8] == 2) {
            damage_lua.push_string(lua, "MISSING_FONT");
        } else {
            damage_lua.push_nil(lua);
        }
        damage_lua.set_field(lua, 1, "font");
#if defined(AB_ORACLE_UI_TEXT_CLIP)
        ui_text_clip_prepare(lua, &random, test);
#endif
        for (uint32_t field = 0; field < input[4]; ++field) {
            FloatBits value = {.bits = input[16 + field]};
            damage_lua.push_number(lua, value.number);
        }
        print_words(input, 46);
#if defined(AB_ORACLE_UI_TEXT_CLIP)
        ui_text_clip_emit();
#endif
        ui_text_event_count = 0;
#if defined(AB_ORACLE_UI_TEXT_CLIP)
        ui_text_clip_leaf_count = 0;
#endif
        if (draw(game, wrapper) != 0 || damage_lua.get_top(lua) != (int)input[4] + 1) {
            fail("Native UI text wrapper changed its argument stack\n");
        }
#if defined(AB_ORACLE_UI_TEXT_CLIP)
        print_words(&ui_text_clip_leaf_count, 1);
#endif
        print_words(&ui_text_event_count, 1);
        for (uint32_t event = 0; event < ui_text_event_count; ++event) {
#if defined(AB_ORACLE_UI_TEXT_IMAGE)
            print_words(ui_text_events[event], 68);
#else
            print_words(ui_text_events[event], 27);
#endif
        }
        print_words(state, 24);
    }
#if defined(AB_ORACLE_UI_TEXT_IMAGE)
    semihost(4, "AB UI text image reference complete\n");
#elif defined(AB_ORACLE_UI_TEXT_CLIP)
    semihost(4, "AB UI text clip reference complete\n");
#else
    semihost(4, "AB UI text reference complete\n");
#endif
}

#endif
