#ifndef AB_ORACLE_THEME_LAYER_H
#define AB_ORACLE_THEME_LAYER_H

static int32_t theme_layer_viewport;

static int32_t theme_layer_width(void *context) {
    (void)context;
    return theme_layer_viewport;
}

#if !defined(AB_ORACLE_THEME_LAYER_BASE)
static void probe_theme_layer(void) {
    typedef float (*Transform)(void *, float, void *, void *);
    Transform forward = (Transform)find_function(
        "_ZN10ThemeLayer22screenToLayerTransformEfPN2gr7ContextEPN3gfx6CameraE");
    Transform inverse = (Transform)find_function(
        "_ZN10ThemeLayer22layerToScreenTransformEfPN2gr7ContextEPN3gfx6CameraE");
    uint32_t layer[24] = {0}, camera[30] = {0}, coordinates[4] = {0};
    uint32_t counter[] = {1000, 1}, vtable[28] = {0};
    uint32_t context[] = {(uint32_t)(uintptr_t)vtable};
    coordinates[1] = (uint32_t)(uintptr_t)counter;
    coordinates[3] = (uint32_t)(uintptr_t)camera;
    camera[4] = (uint32_t)(uintptr_t)coordinates;
    vtable[27] = (uint32_t)(uintptr_t)theme_layer_width;
    uint32_t random = UINT32_C(0x5207e1a9);
    semihost(4, "AB_ANDROID_THEME_LAYER_TRANSFORMS_V1 8192\n");
    for (size_t item = 0; item < 8192; ++item) {
        const uint32_t widths[] = {1,        480,      960,      1920,      8192,
                                   16777215, 16777216, 16777217, 2147483647};
        const float parallaxes[] = {0, 0.125f, 0.3f, 0.6f, 1, -0.4f, 2};
        uint32_t input[8];
        input[0] = widths[item % 9];
        input[1] = random_float(&random, 118, 40, 1);
        input[2] = random_float(&random, 118, 46, 1);
        input[3] = random_word(&random);
        input[4] = random_float(&random, 118, 16, 1);
        input[5] = item % 5 ? random_word(&random) : 0;
        FloatBits parallax = {.number = parallaxes[item % 7]};
        input[6] = parallax.bits;
        input[7] = (uint32_t)item % 2;
        if (item % 13 == 0) {
            input[1] &= UINT32_C(0x80000000);
            input[2] &= UINT32_C(0x80000000);
        }
        theme_layer_viewport = (int32_t)input[0];
        camera[26] = input[2];
        camera[27] = input[3];
        camera[28] = input[4];
        camera[29] = input[5];
        layer[10] = input[6];
        FloatBits point = {.bits = input[1]};
        FloatBits result = {.number = (input[7] ? inverse : forward)(
                                layer, point.number, context, camera)};
        uint32_t output[] = {result.bits, counter[0], counter[1],
                             (result.bits & UINT32_C(0x7f800000)) !=
                                 UINT32_C(0x7f800000)};
        print_words(input, 8);
        print_words(output, 4);
    }
    semihost(4, "AB theme layer transforms reference complete\n");
}
#else
static uint32_t theme_base_state[24], theme_base_sprite[16], theme_base_calls;
#if defined(AB_ORACLE_THEME_LAYER_GPU)
static uint32_t theme_base_viewport[4], theme_base_batcher[54], theme_base_shader[2];
static uint32_t theme_base_vertices[24], theme_base_uvs[16], theme_base_colors[32];
static void (*theme_base_gl_image)(void *, void *, const void *, const void *,
                                   const void *, void *);

static uint32_t *theme_base_get_viewport(void *context) {
    (void)context;
    return theme_base_viewport;
}
#endif

static uint32_t *theme_base_get_state(void *context) {
    (void)context;
    return theme_base_state;
}

static uint32_t *theme_base_get_sprite(void *resources, const void *name) {
    (void)resources;
    (void)name;
#if defined(AB_ORACLE_THEME_LAYER_COMPOSITE)
    return NULL;
#else
    return theme_base_sprite;
#endif
}

static void theme_base_image(void *image, void *context, const uint32_t *vertices,
                             const uint32_t *uv, const uint32_t *color, void *shader) {
    (void)image;
    (void)context;
    if (++theme_base_calls > 512 || shader || !color) {
        fail("Unexpected native theme image boundary\n");
    }
    print_words(vertices, 12);
    print_words(uv, 8);
    print_words(color, 16);
    print_words(theme_base_state, 24);
#if defined(AB_ORACLE_THEME_LAYER_GPU)
    theme_base_batcher[18] = theme_base_state[16];
    theme_base_batcher[41] = 2;
    theme_base_batcher[42] = (uint32_t)(uintptr_t)context;
    theme_base_batcher[43] = (uint32_t)(uintptr_t)image;
    theme_base_batcher[44] = (uint32_t)(uintptr_t)theme_base_shader;
    theme_base_batcher[45] = (uint32_t)(uintptr_t)theme_base_vertices;
    theme_base_batcher[46] = (uint32_t)(uintptr_t)(theme_base_vertices + 3);
    theme_base_batcher[47] = (uint32_t)(uintptr_t)(theme_base_vertices + 24);
    theme_base_batcher[48] = (uint32_t)(uintptr_t)theme_base_uvs;
    theme_base_batcher[49] = (uint32_t)(uintptr_t)(theme_base_uvs + 2);
    theme_base_batcher[50] = (uint32_t)(uintptr_t)(theme_base_uvs + 16);
    theme_base_batcher[51] = (uint32_t)(uintptr_t)theme_base_colors;
    theme_base_batcher[52] = (uint32_t)(uintptr_t)(theme_base_colors + 4);
    theme_base_batcher[53] = (uint32_t)(uintptr_t)(theme_base_colors + 32);
    theme_base_gl_image(image, context, vertices, uv, color, theme_base_shader);
    uint32_t drawn =
        theme_base_batcher[46] == (uint32_t)(uintptr_t)(theme_base_vertices + 21);
    if ((!drawn &&
         theme_base_batcher[46] != (uint32_t)(uintptr_t)(theme_base_vertices + 3)) ||
        theme_base_batcher[49] !=
            (uint32_t)(uintptr_t)(theme_base_uvs + (drawn ? 14 : 2)) ||
        theme_base_batcher[52] !=
            (uint32_t)(uintptr_t)(theme_base_colors + (drawn ? 28 : 4))) {
        fail("Native theme image batch exceeded controlled buffers\n");
    }
    print_words(&drawn, 1);
    if (drawn) {
        print_words(theme_base_vertices + 3, 18);
        print_words(theme_base_uvs + 2, 12);
        print_words(theme_base_colors + 4, 24);
    }
#endif
}

static void probe_theme_layer(void) {
    typedef void (*Draw)(void *, void *, void *);
    Draw draw = (Draw)find_function(
        "_ZN10ThemeLayer14drawBaseSpriteEPN2gr7ContextEPN3gfx6CameraE");
    uint32_t layer[24] = {0}, camera[30] = {0}, coordinates[4] = {0};
    uint32_t counter[] = {1000, 1}, vtable[28] = {0};
    uint32_t context[14] = {(uint32_t)(uintptr_t)vtable};
    uint32_t resources_vtable[3] = {0};
    uint32_t resources[0x4e0 / 4] = {0};
    resources[0] = (uint32_t)(uintptr_t)resources_vtable;
    uint32_t utils[7] = {0}, sheet[4] = {0}, image_vtable[9] = {0};
    uint32_t image[5] = {(uint32_t)(uintptr_t)image_vtable};
    static uint32_t game[1507];
    layer[2] = (uint32_t)(uintptr_t)game;
    layer[3] = (uint32_t)(uintptr_t)utils;
    coordinates[1] = (uint32_t)(uintptr_t)counter;
    coordinates[3] = (uint32_t)(uintptr_t)camera;
    camera[4] = (uint32_t)(uintptr_t)coordinates;
    vtable[24] = (uint32_t)(uintptr_t)theme_base_get_state;
    vtable[27] = (uint32_t)(uintptr_t)theme_layer_width;
    resources_vtable[2] = (uint32_t)(uintptr_t)theme_base_get_sprite;
    utils[5] = (uint32_t)(uintptr_t)resources;
    utils[6] = (uint32_t)(uintptr_t)context;
    sheet[3] = (uint32_t)(uintptr_t)image;
    theme_base_sprite[2] = (uint32_t)(uintptr_t)sheet;
    image_vtable[8] = (uint32_t)(uintptr_t)theme_base_image;
#if defined(AB_ORACLE_THEME_LAYER_GPU)
    theme_base_gl_image =
        (void (*)(void *, void *, const void *, const void *, const void *, void *))
            find_function(
                "_ZN2gr5gles28GL_Image4drawEPNS_7ContextEPKN4math6float3EPKNS4_6float2E"
                "PKNS4_6float4EPNS_6ShaderE");
    vtable[25] = (uint32_t)(uintptr_t)theme_base_get_viewport;
    context[13] = (uint32_t)(uintptr_t)theme_base_batcher;
    image[3] = (uint32_t)(uintptr_t)context;
#endif
#if defined(AB_ORACLE_THEME_LAYER_COMPOSITE)
    uint32_t composite[8] = {0}, entries[4][12] = {{0}}, sprites[4][16] = {{0}};
    uint32_t entry_pointers[4], cache_node[7] = {0};
    uint32_t cache_entry[] = {2, 0, (uint32_t)(uintptr_t)composite};
    static struct {
        uint32_t length, capacity, references;
        char bytes[6];
    } name = {5, 5, 1000, "theme"};
    layer[4] = (uint32_t)(uintptr_t)name.bytes;
    cache_node[4] = (uint32_t)(uintptr_t)name.bytes;
    cache_node[5] = (uint32_t)(uintptr_t)cache_entry;
    cache_node[6] = (uint32_t)(uintptr_t)(cache_entry + 3);
    resources[0x4cc / 4] = (uint32_t)(uintptr_t)cache_node;
    for (size_t index = 0; index < 4; ++index) {
        entry_pointers[index] = (uint32_t)(uintptr_t)entries[index];
        entries[index][2] = (uint32_t)(uintptr_t)name.bytes;
        entries[index][3] = (uint32_t)(uintptr_t)sprites[index];
        sprites[index][2] = (uint32_t)(uintptr_t)sheet;
    }
    composite[2] = (uint32_t)(uintptr_t)entry_pointers;
#endif
    uint32_t random = UINT32_C(0x520ba5e1);
#if defined(AB_ORACLE_THEME_LAYER_GPU)
    semihost(4, "AB_ANDROID_THEME_LAYER_BASE_V3 4096\n");
#elif defined(AB_ORACLE_THEME_LAYER_COMPOSITE)
    semihost(4, "AB_ANDROID_THEME_LAYER_BASE_V2 4096\n");
#else
    semihost(4, "AB_ANDROID_THEME_LAYER_BASE_V1 4096\n");
#endif
    for (size_t item = 0; item < 4096; ++item) {
        const float zooms[] = {0.4f, 0.8f, 1, 2};
        const float scales[] = {0.5f, 1, 1.7f, 2.8f, -1};
        const float parallaxes[] = {0, 0.125f, 0.3f, 0.6f, 1, -0.4f, 2};
        uint32_t input[48] = {0};
        input[0] = 480u << (item % 3);
        for (size_t index = 1; index < 21; ++index) {
            FloatBits value = {
                .number = (float)((int32_t)(random_word(&random) % 4097) - 2048) / 8};
            input[index] = value.bits;
        }
        FloatBits zoom_x = {.number = zooms[item % 4]},
                  zoom_y = {.number = zooms[(item / 4) % 4]},
                  scale = {.number = scales[(item / 16) % 5]},
                  parallax = {.number = parallaxes[(item / 80) % 7]};
        input[3] = zoom_x.bits;
        input[4] = zoom_y.bits;
        input[5] = random_word(&random) % 961 + 64;
        input[6] = random_word(&random) % 1024 + 1;
        input[7] = (uint32_t)((int32_t)(random_word(&random) % 257) - 128);
        input[8] = (uint32_t)((int32_t)(random_word(&random) % 257) - 128);
        input[9] = parallax.bits;
        input[10] = scale.bits;
        input[11] = (uint32_t)(item % 2);
        input[21] = 2048;
        input[22] = 1024;
        if (item % 4 == 0) {
            input[13] = input[14] = 0;
        } else if (item % 4 == 1) {
            input[13] = 0;
        } else if (item % 4 == 2) {
            input[14] = 0;
        }
        for (size_t index = 0; index < 24; ++index) {
            theme_base_state[index] = input[24 + index] = random_word(&random);
        }
#if defined(AB_ORACLE_THEME_LAYER_GPU)
        input[23] = 320u << (item % 3);
        theme_base_viewport[2] = input[0];
        theme_base_viewport[3] = input[23];
        for (size_t index = 0; index < 4; ++index) {
            theme_base_state[index] = input[24 + index] = 0;
        }
#endif
        theme_layer_viewport = (int32_t)input[0];
        for (size_t index = 0; index < 4; ++index) {
            camera[26 + index] = input[1 + index];
        }
        const size_t fields[] = {6, 7, 8, 9};
        for (size_t index = 0; index < 4; ++index) {
            FloatBits value = {.number = (float)(int32_t)input[5 + index]};
            layer[fields[index]] = value.bits;
        }
        for (size_t index = 0; index < 6; ++index) {
            layer[10 + index] = input[9 + index];
        }
        for (size_t index = 0; index < 4; ++index) {
            layer[15 + index] = input[17 + index];
        }
        layer[19] = input[14];
        layer[20] = input[15];
        game[0x1788 / 4] = input[16];
        theme_base_sprite[5] = input[5] | (input[6] << 16);
        FloatBits pivot_x = {.number = (float)(int32_t)input[7]},
                  pivot_y = {.number = (float)(int32_t)input[8]},
                  right = {.number = (float)input[5] / (float)input[21]},
                  bottom = {.number = (float)input[6] / (float)input[22]};
        theme_base_sprite[6] = pivot_x.bits;
        theme_base_sprite[7] = pivot_y.bits;
        theme_base_sprite[10] = theme_base_sprite[14] = right.bits;
        theme_base_sprite[13] = theme_base_sprite[15] = bottom.bits;
        theme_base_calls = 0;
        print_words(input, 48);
#if defined(AB_ORACLE_THEME_LAYER_COMPOSITE)
        uint32_t count = (uint32_t)(item % 5);
        composite[3] = (uint32_t)(uintptr_t)(entry_pointers + count);
        print_words(&count, 1);
        for (size_t index = 0; index < count; ++index) {
            uint32_t part[12] = {0};
            part[0] = random_word(&random) % 1024 + 1;
            part[1] = random_word(&random) % 1024 + 1;
            for (size_t field = 2; field < 6; ++field) {
                part[field] = (uint32_t)((int32_t)(random_word(&random) % 257) - 128);
            }
            for (size_t field = 6; field < 9; ++field) {
                part[field] = random_float(&random, 120, 16, 1);
                entries[index][field] = part[field];
            }
            part[9] = (uint32_t)(item % 2);
            part[10] = (uint32_t)((item / 2) % 2);
            entries[index][11] = part[9] | (part[10] << 1);
            sprites[index][5] = part[0] | (part[1] << 16);
            FloatBits px = {.number = (float)(int32_t)part[2]},
                      py = {.number = (float)(int32_t)part[3]},
                      x = {.number = (float)(int32_t)part[4]},
                      y = {.number = (float)(int32_t)part[5]},
                      u = {.number = (float)part[0] / (float)input[21]},
                      v = {.number = (float)part[1] / (float)input[22]};
            sprites[index][6] = px.bits;
            sprites[index][7] = py.bits;
            entries[index][4] = x.bits;
            entries[index][5] = y.bits;
            sprites[index][10] = sprites[index][14] = u.bits;
            sprites[index][13] = sprites[index][15] = v.bits;
            print_words(part, 12);
        }
#endif
        draw(layer, context, camera);
        uint32_t result[] = {theme_base_calls, counter[0], counter[1]};
        print_words(result, 3);
        print_words(theme_base_state, 24);
#if defined(AB_ORACLE_THEME_LAYER_COMPOSITE)
        if (name.references != 1000) {
            fail("Native theme entry string reference count changed\n");
        }
#endif
    }
    semihost(4, "AB theme layer base reference complete\n");
}
#endif

#endif
