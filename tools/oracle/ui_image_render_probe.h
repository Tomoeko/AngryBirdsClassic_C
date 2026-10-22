#ifndef AB_ORACLE_UI_IMAGE_RENDER_H
#define AB_ORACLE_UI_IMAGE_RENDER_H

static uint32_t ui_render_state[39], ui_render_context[14], ui_render_vtable[26];
static uint32_t ui_render_viewport[4], ui_render_batcher[54], ui_render_shader[2];
static uint32_t ui_render_vertices[24], ui_render_uvs[16], ui_render_event[67];
static uint32_t ui_render_sprite[16], ui_render_sheet[4], ui_render_image[5];
static uint32_t ui_render_image_vtable[7], ui_render_game[18], ui_render_wrapper[8];
static uint32_t ui_render_image_count;
static int (*ui_render_set_state)(void *, void *);
static void (*ui_render_draw_sprite)(void *, void *, float, float, float, float,
                                     int32_t, int32_t, void *, void *);
static void (*ui_render_gl_image)(void *, void *, float, float, float, float,
                                  const void *, void *, const void *);

static uint32_t *ui_render_context_state(const uint32_t *context) {
    (void)context;
    return ui_render_state;
}

static uint32_t *ui_render_context_viewport(const uint32_t *context) {
    (void)context;
    return ui_render_viewport;
}

static int ui_render_state_call(void *lua) {
    (void)ui_image_draw_state(lua);
    if (ui_render_set_state(ui_render_game, ui_render_wrapper) != 0) {
        fail("Native GameLua render-state setter failed\n");
    }
    return 0;
}

static void ui_render_image_call(void *image, void *context, float x, float y,
                                 float width, float height, const uint32_t *uv,
                                 void *shader, void *color) {
    if (ui_render_image_count++ || shader || color) {
        fail("Unexpected native UI image rendering boundary\n");
    }
    for (size_t index = 0; index < 67; ++index) {
        ui_render_event[index] = 0;
    }
    FloatBits rectangle[] = {
        {.number = x}, {.number = y}, {.number = width}, {.number = height}};
    for (size_t index = 0; index < 4; ++index) {
        ui_render_event[index] = rectangle[index].bits;
    }
    for (size_t index = 0; index < 24; ++index) {
        ui_render_event[4 + index] = ui_render_state[index];
    }
    for (size_t index = 0; index < 8; ++index) {
        ui_render_event[28 + index] = uv[index];
    }
    ui_render_batcher[18] = ui_render_state[16];
    ui_render_batcher[41] = 2;
    ui_render_batcher[42] = (uint32_t)(uintptr_t)context;
    ui_render_batcher[43] = (uint32_t)(uintptr_t)image;
    ui_render_batcher[44] = (uint32_t)(uintptr_t)ui_render_shader;
    ui_render_batcher[45] = (uint32_t)(uintptr_t)ui_render_vertices;
    ui_render_batcher[46] = (uint32_t)(uintptr_t)(ui_render_vertices + 3);
    ui_render_batcher[47] = (uint32_t)(uintptr_t)(ui_render_vertices + 24);
    ui_render_batcher[48] = (uint32_t)(uintptr_t)ui_render_uvs;
    ui_render_batcher[49] = (uint32_t)(uintptr_t)(ui_render_uvs + 2);
    ui_render_batcher[50] = (uint32_t)(uintptr_t)(ui_render_uvs + 16);
    ui_render_gl_image(image, context, x, y, width, height, uv, ui_render_shader, NULL);
    uint32_t drawn =
        ui_render_batcher[46] == (uint32_t)(uintptr_t)(ui_render_vertices + 21);
    if ((!drawn &&
         ui_render_batcher[46] != (uint32_t)(uintptr_t)(ui_render_vertices + 3)) ||
        ui_render_batcher[49] !=
            (uint32_t)(uintptr_t)(ui_render_uvs + (drawn ? 14 : 2))) {
        fail("Native UI image batch exceeded controlled output\n");
    }
    ui_render_event[36] = drawn;
    if (drawn) {
        for (size_t index = 0; index < 18; ++index) {
            ui_render_event[37 + index] = ui_render_vertices[3 + index];
        }
        for (size_t index = 0; index < 12; ++index) {
            ui_render_event[55 + index] = ui_render_uvs[2 + index];
        }
    }
}

static int ui_render_sprite_call(void *lua) {
    (void)ui_image_draw_sprite(lua);
    ui_render_draw_sprite(ui_render_sprite, ui_render_context,
                          damage_lua.to_number(lua, 3), damage_lua.to_number(lua, 4),
                          damage_lua.to_number(lua, 7), damage_lua.to_number(lua, 8), 4,
                          3, NULL, NULL);
    return 0;
}

static void ui_render_fixture(const uint32_t extra[19]) {
    ui_render_sprite[4] = (extra[0] & 65535) | ((extra[1] & 65535) << 16);
    ui_render_sprite[5] = (extra[2] & 65535) | ((extra[3] & 65535) << 16);
    FloatBits pivot_x = {.number = (float)(int32_t)extra[4]};
    FloatBits pivot_y = {.number = (float)(int32_t)extra[5]};
    ui_render_sprite[6] = pivot_x.bits;
    ui_render_sprite[7] = pivot_y.bits;
    float left = (float)(int32_t)extra[0] / (float)extra[6];
    float top = (float)(int32_t)extra[1] / (float)extra[7];
    float right = (float)((int32_t)extra[0] + (int32_t)extra[2]) / (float)extra[6];
    float bottom = (float)((int32_t)extra[1] + (int32_t)extra[3]) / (float)extra[7];
    const float uv[] = {left, top, right, top, left, bottom, right, bottom};
    for (size_t index = 0; index < 8; ++index) {
        FloatBits value = {.number = uv[index]};
        ui_render_sprite[8 + index] = value.bits;
    }
    for (size_t index = 0; index < 39; ++index) {
        ui_render_state[index] = 0;
    }
    FloatBits angle = {.bits = extra[16]}, sine = {.number = sinf(angle.number)},
              cosine = {.number = cosf(angle.number)},
              negative_sine = {.number = -sine.number};
    ui_render_state[4] = ui_render_state[7] = cosine.bits;
    ui_render_state[5] = sine.bits;
    ui_render_state[6] = negative_sine.bits;
    for (size_t index = 0; index < 4; ++index) {
        ui_render_state[8 + index] = extra[12 + index];
    }
    ui_render_state[12] = extra[17];
    ui_render_state[13] = extra[18];
    ui_render_state[16] = extra[10];
    ui_render_state[18] = extra[16];
    ui_render_state[19] = extra[11];
    ui_render_viewport[2] = extra[8];
    ui_render_viewport[3] = extra[9];
    ui_render_image_count = 0;
}

static void probe_ui_image_render(void) {
    initialize_damage_lua();
    void *parent = damage_lua.new_state();
    if (!parent) {
        fail("Cannot allocate native UI image render state\n");
    }
    void (*construct_state)(void *, void *) =
        (void (*)(void *, void *))find_function("_ZN3lua8LuaStateC1EP9lua_State");
    void (*claim)(void *) = (void (*)(void *))find_function("_ZN4lang6Object5claimEv");
    construct_state(ui_render_wrapper, parent);
    claim(ui_render_wrapper);
    void *lua = (void *)(uintptr_t)ui_render_wrapper[2];
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    (void)damage_lua.open_math(lua);
    damage_lua.set_top(lua, 0);
    load_selected_function(lua, "/data/local/tmp/ab520/ui/image-draw-self-oracle.lua",
                           "image-draw-self");
    ui_render_set_state = (int (*)(void *, void *))find_function(
        "_ZN7GameLua14setRenderStateEPN3lua8LuaStateE");
    ui_render_draw_sprite =
        (void (*)(void *, void *, float, float, float, float, int32_t, int32_t, void *,
                  void *))find_function("_ZNK4game6Sprite4drawEPN2gr7ContextEffffNS_"
                                        "6AnchorEPNS1_6ShaderEPN4math6float4E");
    ui_render_gl_image = (void (*)(void *, void *, float, float, float, float,
                                   const void *, void *, const void *))
        find_function("_ZN2gr5gles28GL_Image4drawEPNS_7ContextEffffPKN4math6float2EPNS_"
                      "6ShaderEPNS4_6float4E");
    ui_render_vtable[24] = (uint32_t)(uintptr_t)ui_render_context_state;
    ui_render_vtable[25] = (uint32_t)(uintptr_t)ui_render_context_viewport;
    ui_render_context[0] = (uint32_t)(uintptr_t)ui_render_vtable;
    ui_render_context[13] = (uint32_t)(uintptr_t)ui_render_batcher;
    ui_render_game[0x44 / 4] = (uint32_t)(uintptr_t)ui_render_context;
    ui_render_image[0] = (uint32_t)(uintptr_t)ui_render_image_vtable;
    ui_render_image[3] = (uint32_t)(uintptr_t)ui_render_context;
    ui_render_image_vtable[6] = (uint32_t)(uintptr_t)ui_render_image_call;
    ui_render_sheet[3] = (uint32_t)(uintptr_t)ui_render_image;
    ui_render_sprite[2] = (uint32_t)(uintptr_t)ui_render_sheet;
    ui_render_sprite[0] = (uint32_t)find_symbol("_ZTVN4game6SpriteE", 1) + 8;
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 1, "drawSprite", ui_render_sprite_call);
    damage_lua.set_field(lua, -10002, "res");
    uint32_t random = UINT32_C(0x5201da91);
    semihost(4, "AB_ANDROID_UI_IMAGE_RENDER_V1 4096\n");
    for (size_t item = 0; item < 4096; ++item) {
        ui_image_draw_setup[0] = item < 128 ? 3 : random_word(&random) % 4;
        ui_image_draw_setup[1] = item < 128 ? 2 : (uint32_t)((item / 7) % 4);
        ui_image_draw_setup[2] = (uint32_t)(item % 64);
        ui_image_draw_setup[3] = (uint32_t)((item / 64) % 16);
        ui_image_draw_setup[4] = (uint32_t)(item % 2);
        ui_image_draw_setup[5] = UINT32_C(0x43f00000);
        ui_image_draw_setup[6] = UINT32_C(0x43a00000);
        for (size_t field = 7; field < 25; ++field) {
            FloatBits value = {
                .number = (float)((int32_t)(random_word(&random) % 2049) - 1024) / 8};
            ui_image_draw_setup[field] = value.bits;
        }
        const size_t scaled[] = {9, 10, 12, 15, 16};
        static const uint32_t scales[] = {0x3e800000, 0xbe800000, 0x3f000000,
                                          0xbf000000, 0x3f800000, 0x40000000};
        for (size_t field = 0; field < 5; ++field) {
            ui_image_draw_setup[scaled[field]] = scales[random_word(&random) % 6];
        }
        if (item % 3 == 0) {
            ui_image_draw_setup[11] = ui_image_draw_setup[17] = 0;
        }
        uint32_t extra[19];
        for (size_t field = 0; field < 6; ++field) {
            extra[field] = (uint32_t)((int32_t)(random_word(&random) % 63) - 31);
        }
        extra[6] = random_word(&random) % 1024 + 1;
        extra[7] = random_word(&random) % 1024 + 1;
        extra[8] = random_word(&random) % 1024 + 1;
        extra[9] = random_word(&random) % 1024 + 1;
        FloatBits alpha = {.number = (float)(random_word(&random) % 17) / 8};
        extra[10] = alpha.bits;
        extra[11] = item % 7 == 0;
        for (size_t field = 12; field < 19; ++field) {
            FloatBits value = {
                .number = (float)((int32_t)(random_word(&random) % 65) - 32) / 8};
            extra[field] = value.bits;
        }
        ui_render_fixture(extra);
        print_words(ui_image_draw_setup, 25);
        print_words(extra, 19);
        ui_image_draw_prepare(lua);
        damage_lua.get_field(lua, -10002, "gamelua");
        damage_function(lua, 1, "setRenderState", ui_render_state_call);
        damage_lua.set_top(lua, 0);
        ui_image_draw_run(lua);
        print_words(&ui_render_image_count, 1);
        if (ui_render_image_count) {
            print_words(ui_render_event, 67);
        }
        print_words(ui_render_state, 24);
    }
    semihost(4, "AB UI image render reference complete\n");
}

#endif
