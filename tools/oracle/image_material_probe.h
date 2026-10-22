#ifndef AB_ORACLE_IMAGE_MATERIAL_H
#define AB_ORACLE_IMAGE_MATERIAL_H

static uint32_t image_material_state[39], image_material_viewport[4];
static uint32_t image_material_format, image_material_format_queries;
static uint32_t image_material_shader_queries, image_material_path_queries;
static uint32_t image_material_selected, image_material_tokens[2][2];
static uint32_t image_material_vertices[24], image_material_uvs[16];
static uint32_t image_material_colors[32];

static void *image_material_get_state(void *context) {
    (void)context;
    return image_material_state;
}

static void *image_material_get_viewport(void *context) {
    (void)context;
    return image_material_viewport;
}

static uint32_t image_material_get_format(void *texture) {
    (void)texture;
    ++image_material_format_queries;
    return image_material_format;
}

static const char *const *image_material_get_path(void *context) {
    (void)context;
    static struct {
        uint32_t length, capacity, references;
        char bytes[8];
    } path = {7, 7, 1000, "shaders"};
    static const char *pointer = path.bytes;
    ++image_material_path_queries;
    return &pointer;
}

static void *image_material_get_shader(void *context, const char *const *name,
                                       uint32_t flags) {
    (void)context;
    if (++image_material_shader_queries != 1 || flags || !name || !*name) {
        fail("Unexpected native default image shader lookup\n");
    }
    const char *const names[] = {"shaders/2d-sprite", "shaders/2d-sprite-alpha"};
    uint32_t length = ((const uint32_t *)(const void *)*name)[-3];
    for (size_t index = 0; index < 2; ++index) {
        size_t offset = 0;
        while (names[index][offset] && offset < length &&
               names[index][offset] == (*name)[offset]) {
            ++offset;
        }
        if (offset == length && !names[index][offset]) {
            image_material_selected = (uint32_t)index + 1;
            return image_material_tokens[index];
        }
    }
    fail("Original point image selected an unexpected default shader\n");
}

static void probe_image_material(void) {
#if defined(AB_ORACLE_IMAGE_MATERIAL_RECTANGLE)
    typedef void (*ImageDraw)(void *, void *, float, float, float, float, const void *,
                              void *, const void *);
    ImageDraw draw = (ImageDraw)find_function(
        "_ZN2gr5gles28GL_Image4drawEPNS_7ContextEffffPKN4math6float2EPNS_6ShaderEPNS4_"
        "6float4E");
#else
    typedef void (*ImageDraw)(void *, void *, const void *, const void *, const void *,
                              void *);
    ImageDraw draw = (ImageDraw)find_function(
        "_ZN2gr5gles28GL_Image4drawEPNS_7ContextEPKN4math6float3EPKNS4_6float2E"
        "PKNS4_6float4EPNS_6ShaderE");
#endif
    int (*has_alpha)(const uint32_t *) =
        (int (*)(const uint32_t *))find_function("_ZNK3img13SurfaceFormat8hasAlphaEv");
    void (*construct_object)(void *) =
        (void (*)(void *))find_function("_ZN4lang6ObjectC1Ev");
    uint32_t context_vtable[39] = {0}, texture_vtable[8] = {0};
    uint32_t context[0x2c4 / 4] = {(uint32_t)(uintptr_t)context_vtable};
    uint32_t texture[] = {(uint32_t)(uintptr_t)texture_vtable};
    uint32_t image[5] = {0, 0, 0, (uint32_t)(uintptr_t)context,
                         (uint32_t)(uintptr_t)texture};
    uint32_t batcher[55] = {0};
    context[13] = (uint32_t)(uintptr_t)batcher;
    context_vtable[2] = (uint32_t)(uintptr_t)image_material_get_shader;
    context_vtable[24] = (uint32_t)(uintptr_t)image_material_get_state;
    context_vtable[25] = (uint32_t)(uintptr_t)image_material_get_viewport;
    context_vtable[38] = (uint32_t)(uintptr_t)image_material_get_path;
    texture_vtable[7] = (uint32_t)(uintptr_t)image_material_get_format;
    construct_object(image_material_tokens[0]);
    construct_object(image_material_tokens[1]);
    uint32_t random = UINT32_C(0x520c0105);
    const float uv[] = {0, 0, 1, 0, 0, 1, 1, 1};
#if defined(AB_ORACLE_IMAGE_MATERIAL_RECTANGLE)
    image_material_state[4] = image_material_state[7] = UINT32_C(0x3f800000);
    image_material_state[10] = image_material_state[11] = UINT32_C(0x3f800000);
    semihost(4, "AB_ANDROID_IMAGE_MATERIAL_RECTANGLE_V1 4096\n");
#else
    semihost(4, "AB_ANDROID_IMAGE_MATERIAL_V1 4096\n");
#endif
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[8];
        input[0] = (uint32_t)(item % 34);
        input[1] = 480u << (item % 3);
        input[2] = 320u << (item % 3);
        FloatBits alpha = {.number = (float)(random_word(&random) % 513) / 256};
        if (item % 13 == 0) {
            alpha.bits = UINT32_C(0x80000000);
        }
#if defined(AB_ORACLE_IMAGE_MATERIAL_RECTANGLE)
        if (item < 272) {
            const uint32_t boundaries[] = {0x80000000, 0,          0x3f7fffff,
                                           0x3f800000, 0x3f800001, 0x40000000,
                                           0xbf000000, 0x3f000000};
            alpha.bits = boundaries[item / 34];
        }
#endif
        input[3] = alpha.bits;
        float left = (float)((int32_t)(random_word(&random) % (input[1] * 4)) -
                             (int32_t)input[1]);
        float top = (float)((int32_t)(random_word(&random) % (input[2] * 4)) -
                            (int32_t)input[2]);
#if defined(AB_ORACLE_IMAGE_MATERIAL_RECTANGLE)
        if (item < 272) {
            left = top = 0;
        }
#endif
        FloatBits edges[] = {{.number = left},
                             {.number = top},
                             {.number = left + 257},
                             {.number = top + 193}};
        float vertices[12];
        const size_t x_indices[] = {0, 2, 0, 2}, y_indices[] = {1, 1, 3, 3};
        uint32_t color[16];
        for (size_t index = 0; index < 4; ++index) {
            input[4 + index] = edges[index].bits;
            vertices[index * 3] = edges[x_indices[index]].number;
            vertices[index * 3 + 1] = edges[y_indices[index]].number;
            vertices[index * 3 + 2] = 0;
        }
        for (size_t index = 0; index < 16; ++index) {
            color[index] = random_float(&random, 118, 16, 1);
        }
        image_material_format = input[0];
        uint32_t alpha_present = (uint32_t)has_alpha(&image_material_format);
        image_material_state[16] = input[3];
        image_material_viewport[2] = input[1];
        image_material_viewport[3] = input[2];
        context[0x2bc / 4] = context[0x2c0 / 4] = 0;
        image_material_format_queries = image_material_shader_queries = 0;
        image_material_path_queries = image_material_selected = 0;
        batcher[18] = input[3];
        batcher[41] = 2;
        batcher[42] = (uint32_t)(uintptr_t)context;
        batcher[43] = (uint32_t)(uintptr_t)image;
        uint32_t alpha_shader = alpha_present;
#if defined(AB_ORACLE_IMAGE_MATERIAL_RECTANGLE)
        alpha_shader = alpha_present || alpha.number < 1;
#endif
        batcher[44] = (uint32_t)(uintptr_t)image_material_tokens[alpha_shader ? 1 : 0];
        batcher[45] = (uint32_t)(uintptr_t)image_material_vertices;
        batcher[46] = (uint32_t)(uintptr_t)(image_material_vertices + 3);
        batcher[47] = (uint32_t)(uintptr_t)(image_material_vertices + 24);
        batcher[48] = (uint32_t)(uintptr_t)image_material_uvs;
        batcher[49] = (uint32_t)(uintptr_t)(image_material_uvs + 2);
        batcher[50] = (uint32_t)(uintptr_t)(image_material_uvs + 16);
        batcher[51] = (uint32_t)(uintptr_t)image_material_colors;
        batcher[52] = (uint32_t)(uintptr_t)(image_material_colors + 4);
        batcher[53] = (uint32_t)(uintptr_t)(image_material_colors + 32);
#if defined(AB_ORACLE_IMAGE_MATERIAL_RECTANGLE)
        draw(image, context, left, top, 257, 193, uv, NULL, NULL);
#else
        draw(image, context, vertices, uv, color, NULL);
#endif
        uint32_t drawn =
            batcher[46] == (uint32_t)(uintptr_t)(image_material_vertices + 21);
        if ((!drawn &&
             batcher[46] != (uint32_t)(uintptr_t)(image_material_vertices + 3)) ||
            drawn != image_material_format_queries ||
            drawn != image_material_shader_queries ||
            drawn != image_material_path_queries) {
            fail("Native image material fixture exceeded controlled boundaries\n");
        }
        uint32_t output[] = {alpha_present,
                             image_material_selected,
                             image_material_format_queries,
                             image_material_shader_queries,
                             image_material_path_queries,
                             drawn};
        print_words(input, 8);
        print_words(output, 6);
    }
#if defined(AB_ORACLE_IMAGE_MATERIAL_RECTANGLE)
    semihost(4, "AB rectangle image material reference complete\n");
#else
    semihost(4, "AB image material reference complete\n");
#endif
}

#endif
