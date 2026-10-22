#ifndef AB_ORACLE_THEME_SCENE_H
#define AB_ORACLE_THEME_SCENE_H

/* Prepared resource cache metadata comes from the bounded live theme1 read.
 * Original constructors, lookup, composition, layer, Sprite and CPU batch
 * methods execute. The image/fill boundaries record output without a GPU. */
typedef struct {
    uint32_t length, capacity, references;
    char bytes[128];
} ThemeSceneString;

static ThemeSceneString theme_scene_names[32];
static uint32_t theme_scene_nodes[32][7], theme_scene_entries[32][3];
static uint32_t theme_scene_sprites[24][16], theme_scene_composites[4][15];
static uint32_t theme_scene_parts[4][32][12], theme_scene_part_pointers[4][32];
static uint32_t theme_scene_state[39], theme_scene_viewport[4];
static uint32_t theme_scene_images[24][5], theme_scene_sheet[24][4];
static uint32_t theme_scene_batcher[54], theme_scene_shader[2];
static uint32_t theme_scene_vertices[24], theme_scene_uvs[16], theme_scene_colors[32];
static uint32_t theme_scene_calls, theme_scene_fills;
static int32_t theme_scene_size[2];
static void (*theme_scene_gl_image)(void *, void *, const void *, const void *,
                                    const void *, void *);
#if defined(AB_ORACLE_THEME_SCENE_FILL)
static uint32_t theme_scene_fill_shaders[2][2];
static void (*theme_scene_gl_fill)(void *, uint32_t, int32_t, int32_t, int32_t, int32_t,
                                   uint32_t);
#endif

static void scene_read(int file, void *output, size_t size) {
    uint8_t *bytes = output;
    while (size) {
        int count = system_call(3, file, bytes, size);
        if (count <= 0 || (size_t)count > size) {
            fail("Cannot read bounded native theme fixture\n");
        }
        bytes += count;
        size -= (size_t)count;
    }
}

static void scene_string(int file, ThemeSceneString *name) {
    scene_read(file, &name->length, sizeof(name->length));
    if (!name->length || name->length >= sizeof(name->bytes)) {
        fail("Native theme fixture name exceeds bounds\n");
    }
    scene_read(file, name->bytes, name->length);
    name->bytes[name->length] = 0;
    name->capacity = name->length;
    name->references = 1000;
}

static int scene_compare(const ThemeSceneString *left, const ThemeSceneString *right) {
    size_t length = left->length < right->length ? left->length : right->length;
    for (size_t index = 0; index < length; ++index) {
        if (left->bytes[index] != right->bytes[index]) {
            return left->bytes[index] < right->bytes[index] ? -1 : 1;
        }
    }
    return left->length < right->length ? -1 : left->length > right->length;
}

static void scene_link(uint32_t *resources, size_t index, uint32_t kind,
                       uint32_t *object) {
    uint32_t *node = theme_scene_nodes[index];
    uint32_t *entry = theme_scene_entries[index];
    node[4] = (uint32_t)(uintptr_t)theme_scene_names[index].bytes;
    node[5] = (uint32_t)(uintptr_t)entry;
    node[6] = (uint32_t)(uintptr_t)(entry + 3);
    entry[0] = kind;
    entry[2] = (uint32_t)(uintptr_t)object;
    uint32_t *link = resources + 0x4cc / 4;
    while (*link) {
        uint32_t *parent = (uint32_t *)(uintptr_t)*link;
        const char *bytes = (const char *)(uintptr_t)parent[4];
        const ThemeSceneString *name =
            (const ThemeSceneString *)(const void *)(bytes - 12);
        int order = scene_compare(theme_scene_names + index, name);
        if (!order) {
            fail("Duplicate native theme fixture cache name\n");
        }
        link = parent + (order < 0 ? 2 : 3);
    }
    *link = (uint32_t)(uintptr_t)node;
}

static void scene_load_resources(uint32_t *resources, uint32_t *image_vtable,
                                 uint32_t *context) {
    const char *path = "/data/local/tmp/ab520/theme1-bindings.bin";
    int file = system_call(5, (int)(uintptr_t)path, NULL, 0);
    if (file < 0) {
        fail("Cannot open private theme fixture\n");
    }
    uint32_t header[3];
    scene_read(file, header, sizeof(header));
    if (header[0] != UINT32_C(0x31544241) || header[1] > 24 || header[2] > 4 ||
        header[1] + header[2] > 32) {
        fail("Unexpected bounded theme fixture header\n");
    }
    for (size_t index = 0; index < header[1]; ++index) {
        scene_string(file, theme_scene_names + index);
        uint32_t *sprite = theme_scene_sprites[index];
        scene_read(file, sprite + 4, 12 * sizeof(*sprite));
        sprite[2] = (uint32_t)(uintptr_t)theme_scene_sheet[index];
        theme_scene_sheet[index][3] = (uint32_t)(uintptr_t)theme_scene_images[index];
        theme_scene_images[index][0] = (uint32_t)(uintptr_t)image_vtable;
        theme_scene_images[index][3] = (uint32_t)(uintptr_t)context;
        theme_scene_images[index][4] = (uint32_t)index;
        scene_link(resources, index, 1, sprite);
    }
    for (size_t index = 0; index < header[2]; ++index) {
        size_t name_index = header[1] + index;
        scene_string(file, theme_scene_names + name_index);
        uint32_t *composite = theme_scene_composites[index];
        scene_read(file, composite + 11, 4 * sizeof(*composite));
        uint32_t count;
        scene_read(file, &count, sizeof(count));
        if (count > 32) {
            fail("Native theme fixture part count exceeds bounds\n");
        }
        composite[2] = (uint32_t)(uintptr_t)theme_scene_part_pointers[index];
        composite[3] = (uint32_t)(uintptr_t)(theme_scene_part_pointers[index] + count);
        for (size_t part = 0; part < count; ++part) {
            ThemeSceneString name = {0};
            scene_string(file, &name);
            uint32_t *entry = theme_scene_parts[index][part];
            scene_read(file, entry + 4, 8 * sizeof(*entry));
            size_t sprite = 0;
            while (sprite < header[1] &&
                   scene_compare(&name, theme_scene_names + sprite)) {
                ++sprite;
            }
            if (sprite == header[1]) {
                fail("Missing ordinary sprite in native theme fixture\n");
            }
            entry[2] = (uint32_t)(uintptr_t)theme_scene_names[sprite].bytes;
            entry[3] = (uint32_t)(uintptr_t)theme_scene_sprites[sprite];
            theme_scene_part_pointers[index][part] = (uint32_t)(uintptr_t)entry;
        }
        scene_link(resources, name_index, 2, composite);
    }
    uint8_t tail;
    if (system_call(3, file, &tail, 1) != 0 || system_call(6, file, NULL, 0)) {
        fail("Native theme fixture has trailing bytes or close failure\n");
    }
}

static void *scene_get_state(void *context) {
    (void)context;
    return theme_scene_state;
}

static void *scene_get_viewport(void *context) {
    (void)context;
    return theme_scene_viewport;
}

static int32_t scene_width(void *context) {
    (void)context;
    return theme_scene_size[0];
}

static int32_t scene_height(void *context) {
    (void)context;
    return theme_scene_size[1];
}

static void scene_state(void) {
    uint32_t words[] = {theme_scene_state[8],  theme_scene_state[9],
                        theme_scene_state[10], theme_scene_state[11],
                        theme_scene_state[16], theme_scene_state[18]};
    print_words(words, 6);
}

static int scene_fill(void *context, uint32_t color, int32_t left, int32_t top,
                      int32_t width, int32_t height, uint32_t blend) {
    (void)context;
    if (++theme_scene_fills != 1 || blend) {
        fail("Unexpected native theme scene fill\n");
    }
    uint32_t words[] = {color, (uint32_t)left, (uint32_t)top, (uint32_t)width,
                        (uint32_t)height};
    semihost(4, "F\n");
    print_words(words, 5);
    scene_state();
#if defined(AB_ORACLE_THEME_SCENE_FILL)
    uint32_t *native_context = context;
    for (size_t index = 0; index < 39; ++index) {
        native_context[0xec / 4 + index] = theme_scene_state[index];
    }
    theme_scene_batcher[42] = (uint32_t)(uintptr_t)context;
    /* Image-provider batches borrow fixture objects without acquiring native
     * ownership. Start the independent fill batch without releasing them. */
    theme_scene_batcher[43] = theme_scene_batcher[44] = 0;
    theme_scene_batcher[45] = theme_scene_batcher[46] =
        (uint32_t)(uintptr_t)theme_scene_vertices;
    theme_scene_batcher[47] = (uint32_t)(uintptr_t)(theme_scene_vertices + 24);
    theme_scene_batcher[48] = theme_scene_batcher[49] =
        (uint32_t)(uintptr_t)theme_scene_uvs;
    theme_scene_batcher[50] = (uint32_t)(uintptr_t)(theme_scene_uvs + 16);
    theme_scene_batcher[51] = theme_scene_batcher[52] =
        (uint32_t)(uintptr_t)theme_scene_colors;
    theme_scene_batcher[53] = (uint32_t)(uintptr_t)(theme_scene_colors + 32);
    theme_scene_gl_fill(context, color, left, top, width, height, blend);
    uint32_t drawn =
        theme_scene_batcher[46] == (uint32_t)(uintptr_t)(theme_scene_vertices + 18);
    uint32_t selected = drawn
                            ? (theme_scene_batcher[44] ==
                                       (uint32_t)(uintptr_t)theme_scene_fill_shaders[0]
                                   ? 1u
                                   : 2u)
                            : 0;
    if ((drawn &&
         (theme_scene_batcher[41] != 1 || theme_scene_batcher[43] ||
          theme_scene_batcher[44] !=
              (uint32_t)(uintptr_t)theme_scene_fill_shaders[selected - 1] ||
          theme_scene_batcher[52] != (uint32_t)(uintptr_t)(theme_scene_colors + 24))) ||
        (!drawn && (theme_scene_batcher[46] != theme_scene_batcher[45] ||
                    theme_scene_batcher[52] != theme_scene_batcher[51]))) {
        fail("Native theme ground fill exceeded controlled batch buffers\n");
    }
    uint32_t summary[] = {drawn, selected};
    semihost(4, "R\n");
    print_words(summary, 2);
    if (drawn) {
        print_words(theme_scene_vertices, 18);
        print_words(theme_scene_colors, 24);
    }
#endif
    return 0;
}

static void scene_image(uint32_t *image, void *context, const uint32_t *vertices,
                        const uint32_t *uv, const uint32_t *color, void *shader) {
    if (++theme_scene_calls > 4096 || image[4] >= 24 || shader || !color) {
        fail("Unexpected native theme scene image\n");
    }
    semihost(4, "I\n");
    print_words(theme_scene_sprites[image[4]] + 4, 12);
    print_words(vertices, 12);
    print_words(uv, 8);
    print_words(color, 16);
    scene_state();
    for (size_t index = 0; index < 4; ++index) {
        theme_scene_batcher[2 + index] = theme_scene_state[index];
    }
    theme_scene_batcher[18] = theme_scene_state[16];
    theme_scene_batcher[41] = 2;
    theme_scene_batcher[42] = (uint32_t)(uintptr_t)context;
    theme_scene_batcher[43] = (uint32_t)(uintptr_t)image;
    theme_scene_batcher[44] = (uint32_t)(uintptr_t)theme_scene_shader;
    theme_scene_batcher[45] = (uint32_t)(uintptr_t)theme_scene_vertices;
    theme_scene_batcher[46] = (uint32_t)(uintptr_t)(theme_scene_vertices + 3);
    theme_scene_batcher[47] = (uint32_t)(uintptr_t)(theme_scene_vertices + 24);
    theme_scene_batcher[48] = (uint32_t)(uintptr_t)theme_scene_uvs;
    theme_scene_batcher[49] = (uint32_t)(uintptr_t)(theme_scene_uvs + 2);
    theme_scene_batcher[50] = (uint32_t)(uintptr_t)(theme_scene_uvs + 16);
    theme_scene_batcher[51] = (uint32_t)(uintptr_t)theme_scene_colors;
    theme_scene_batcher[52] = (uint32_t)(uintptr_t)(theme_scene_colors + 4);
    theme_scene_batcher[53] = (uint32_t)(uintptr_t)(theme_scene_colors + 32);
    theme_scene_gl_image(image, context, vertices, uv, color, theme_scene_shader);
    uint32_t drawn =
        theme_scene_batcher[46] == (uint32_t)(uintptr_t)(theme_scene_vertices + 21);
    if ((!drawn &&
         theme_scene_batcher[46] != (uint32_t)(uintptr_t)(theme_scene_vertices + 3)) ||
        theme_scene_batcher[49] !=
            (uint32_t)(uintptr_t)(theme_scene_uvs + (drawn ? 14 : 2)) ||
        theme_scene_batcher[52] !=
            (uint32_t)(uintptr_t)(theme_scene_colors + (drawn ? 28 : 4))) {
        fail("Native theme scene batch exceeded controlled buffers\n");
    }
    print_words(&drawn, 1);
    if (drawn) {
        print_words(theme_scene_vertices + 3, 18);
        print_words(theme_scene_uvs + 2, 12);
        print_words(theme_scene_colors + 4, 24);
    }
}

static void probe_theme_scene(void) {
    initialize_damage_lua();
    typedef void (*Construct)(void *, void *);
    typedef void (*Draw)(void *, void *, void *, uint32_t);
    Construct construct_state =
        (Construct)find_function("_ZN3lua8LuaStateC1EP9lua_State");
    void (*construct_table)(void *, void *, int) = (void (*)(
        void *, void *, int))find_function("_ZN3lua8LuaTableC1EPNS_8LuaStateEi");
    void (*construct_composition)(void *, const void *, void *, void *, void *) =
        (void (*)(void *, const void *, void *, void *, void *))find_function(
            "_ZN16ThemeCompositionC1ERKN3lua8LuaTableERN4game9ResourcesEP7GameLuaP9Draw"
            "Utils");
    Draw background = (Draw)find_function(
        "_ZN16ThemeComposition14drawBackgroundEPN2gr7ContextEPN3gfx6CameraEj");
    Draw foreground = (Draw)find_function(
        "_ZN16ThemeComposition14drawForegroundEPN2gr7ContextEPN3gfx6CameraEj");
    uint32_t wrapper[8] = {0}, table[6] = {0}, composition[17] = {0};
    uint32_t camera[30] = {0}, coordinates[4] = {0}, counter[] = {1000, 1};
    uint32_t context_vtable[29] = {0}, image_vtable[9] = {0};
    uint32_t context[0x2c4 / 4] = {(uint32_t)(uintptr_t)context_vtable};
    uint32_t resources[0x4e0 / 4] = {0}, resources_vtable[3] = {0}, utils[7] = {0};
    static uint32_t game[0x1884 / 4];
    coordinates[1] = (uint32_t)(uintptr_t)counter;
    coordinates[3] = (uint32_t)(uintptr_t)camera;
    camera[4] = (uint32_t)(uintptr_t)coordinates;
    game[0x44 / 4] = (uint32_t)(uintptr_t)context;
    context[13] = (uint32_t)(uintptr_t)theme_scene_batcher;
    context_vtable[16] = (uint32_t)(uintptr_t)scene_fill;
    context_vtable[24] = (uint32_t)(uintptr_t)scene_get_state;
    context_vtable[25] = (uint32_t)(uintptr_t)scene_get_viewport;
    context_vtable[27] = (uint32_t)(uintptr_t)scene_width;
    context_vtable[28] = (uint32_t)(uintptr_t)scene_height;
    image_vtable[8] = (uint32_t)(uintptr_t)scene_image;
    resources_vtable[2] = (uint32_t)find_function("_ZN4game9Resources9getSpriteERKSs");
    resources[0] = (uint32_t)(uintptr_t)resources_vtable;
    utils[5] = (uint32_t)(uintptr_t)resources;
    utils[6] = (uint32_t)(uintptr_t)context;
    theme_scene_gl_image =
        (void (*)(void *, void *, const void *, const void *, const void *, void *))
            find_function(
                "_ZN2gr5gles28GL_Image4drawEPNS_7ContextEPKN4math6float3EPKNS4_6float2E"
                "PKNS4_6float4EPNS_6ShaderE");
#if defined(AB_ORACLE_THEME_SCENE_FILL)
    theme_scene_gl_fill = (void (*)(void *, uint32_t, int32_t, int32_t, int32_t,
                                    int32_t, uint32_t))
        find_function("_ZN2gr5gles210GL_Context10fillRect2DEjiiiiNS_7Context5BlendE");
    void (*construct_object)(void *) =
        (void (*)(void *))find_function("_ZN4lang6ObjectC1Ev");
    void (*claim_object)(void *) =
        (void (*)(void *))find_function("_ZN4lang6Object5claimEv");
    for (size_t index = 0; index < 2; ++index) {
        construct_object(theme_scene_fill_shaders[index]);
        claim_object(theme_scene_fill_shaders[index]);
    }
    context[0x2b4 / 4] = (uint32_t)(uintptr_t)theme_scene_fill_shaders[0];
    context[0x2b8 / 4] = (uint32_t)(uintptr_t)theme_scene_fill_shaders[1];
#endif
    scene_load_resources(resources, image_vtable, context);
    void *parent = damage_lua.new_state();
    if (!parent) {
        fail("Cannot allocate native theme scene Lua state\n");
    }
    construct_state(wrapper, parent);
    ((void (*)(void *))find_function("_ZN4lang6Object5claimEv"))(wrapper);
    void *lua = (void *)(uintptr_t)wrapper[2];
    load_damage_chunk(lua, "/data/local/tmp/ab520/serialized.lua");
    damage_lua.get_field(lua, -10002, "theme1");
    construct_table(table, wrapper, 1);
    damage_lua.set_top(lua, 0);
    construct_composition(composition, table, resources, game, utils);
    if (composition[3] - composition[2] != 12 || composition[6] - composition[5] != 8) {
        fail("Expected three background and two foreground native theme layers\n");
    }
#if defined(AB_ORACLE_THEME_SCENE_FILL)
    semihost(4, "AB_ANDROID_THEME_SCENE_V2 256\n");
#else
    semihost(4, "AB_ANDROID_THEME_SCENE_V1 256\n");
#endif
    print_words(composition + 8, 9);
    for (size_t group = 0; group < 2; ++group) {
        const uint32_t *layers =
            (const uint32_t *)(uintptr_t)composition[group ? 5 : 2];
        for (size_t index = 0; index < (group ? 2u : 3u); ++index) {
            const uint32_t *layer = (const uint32_t *)(uintptr_t)layers[index];
            uint32_t fields[19];
            for (size_t field = 0; field < 19; ++field) {
                fields[field] = layer[5 + field];
            }
            fields[0] &= 255;
            fields[7] &= 255;
            print_words(fields, 19);
        }
    }
    uint32_t random = UINT32_C(0x5207ce11);
    const float zooms[] = {0.125f, 0.4f, 1, 2};
    for (size_t item = 0; item < 256; ++item) {
        uint32_t input[9];
        input[0] = 480u << (item % 3);
        input[1] = 320u << (item % 3);
        if (item % 31 == 0) {
            input[0] = input[1] = 1;
        }
        for (size_t index = 2; index < 4; ++index) {
            FloatBits point = {
                .number = (float)((int32_t)(random_word(&random) % 8193) - 4096) / 8};
            input[index] = point.bits;
        }
        FloatBits zoom_x = {.number = zooms[item % 4]},
                  zoom_y = {.number = zooms[(item / 4) % 4]},
                  time = {.number = (float)item / 60};
        input[4] = zoom_x.bits;
        input[5] = zoom_y.bits;
        input[6] = time.bits;
        input[7] = random_word(&random);
        input[8] = random_float(&random, 118, 16, 1);
        for (size_t index = 0; index < 39; ++index) {
            theme_scene_state[index] = 0;
        }
        theme_scene_state[16] = input[8];
        for (size_t index = 0; index < 4; ++index) {
            camera[26 + index] = input[2 + index];
        }
        theme_scene_size[0] = (int32_t)input[0];
        theme_scene_size[1] = (int32_t)input[1];
        theme_scene_viewport[2] = input[0];
        theme_scene_viewport[3] = input[1];
        game[0x1788 / 4] = input[6];
        theme_scene_calls = theme_scene_fills = 0;
        print_words(input, 9);
        background(composition, context, camera, input[7]);
        semihost(4, "B\n");
        uint32_t summary[] = {theme_scene_calls, theme_scene_fills, game[0xf0 / 4]};
        print_words(summary, 3);
        scene_state();
        foreground(composition, context, camera, input[7]);
        semihost(4, "G\n");
        summary[0] = theme_scene_calls;
        summary[1] = theme_scene_fills;
        summary[2] = game[0xf0 / 4];
        print_words(summary, 3);
        scene_state();
    }
    /* One composition remains alive until this isolated process exits. */
    semihost(4, "AB theme scene reference complete\n");
}

#endif
