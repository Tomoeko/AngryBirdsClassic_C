#ifndef AB_ORACLE_IMAGE_RECTANGLE_H
#define AB_ORACLE_IMAGE_RECTANGLE_H

static uint32_t *rectangle_state(uint32_t *context) {
    return (uint32_t *)(uintptr_t)context[1];
}

static uint32_t *rectangle_viewport(uint32_t *context) {
    return (uint32_t *)(uintptr_t)context[2];
}

static void probe_image_rectangle(void) {
    typedef void (*Draw)(void *, void *, float, float, float, float, const void *,
                         void *, const void *);
    Draw draw =
        (Draw)find_function("_ZN2gr5gles28GL_Image4drawEPNS_"
                            "7ContextEffffPKN4math6float2EPNS_6ShaderEPNS4_6float4E");
    uint32_t context_vtable[26] = {0}, state[39] = {0}, viewport[4] = {0};
    uint32_t context[14] = {0}, batcher[54] = {0}, image[5] = {0}, shader[2] = {0};
    uint32_t vertices[24] = {0}, coordinates[16] = {0};
    context_vtable[24] = (uint32_t)(uintptr_t)rectangle_state;
    context_vtable[25] = (uint32_t)(uintptr_t)rectangle_viewport;
    context[0] = (uint32_t)(uintptr_t)context_vtable;
    context[1] = (uint32_t)(uintptr_t)state;
    context[2] = (uint32_t)(uintptr_t)viewport;
    context[13] = (uint32_t)(uintptr_t)batcher;
    image[3] = (uint32_t)(uintptr_t)context;
    batcher[41] = 2;
    batcher[42] = (uint32_t)(uintptr_t)context;
    batcher[43] = (uint32_t)(uintptr_t)image;
    batcher[44] = (uint32_t)(uintptr_t)shader;
    batcher[45] = (uint32_t)(uintptr_t)vertices;
    batcher[47] = (uint32_t)(uintptr_t)(vertices + 24);
    batcher[48] = (uint32_t)(uintptr_t)coordinates;
    batcher[50] = (uint32_t)(uintptr_t)(coordinates + 16);
    state[16] = batcher[18] = 0x3f800000;
    const uint32_t uv[] = {0, 0, 0x3f800000, 0, 0, 0x3f800000, 0x3f800000, 0x3f800000};
    uint32_t random = UINT32_C(0x5201a9e0);
    semihost(4, "AB_ANDROID_IMAGE_RECTANGLE_V1 4096\n");
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[16];
        FloatBits values[11];
        for (size_t field = 0; field < 11; ++field) {
            values[field].number =
                (float)((int32_t)(random_word(&random) % 65537) - 32768) / 32.0f;
        }
        values[4].bits = item % 3 ? next_extended_angle(&random, item) : 0;
        static const float scales[] = {0, -0.0f, 1, -1, 2, 0.5f, 0.33333334f};
        values[7].number = scales[random_word(&random) % 7];
        values[8].number = scales[random_word(&random) % 7];
        for (size_t field = 0; field < 11; ++field) {
            input[field] = values[field].bits;
        }
        input[11] = item % 7 == 0;
        input[12] = random_word(&random) % 2048 + 1;
        input[13] = random_word(&random) % 2048 + 1;
        input[14] = (uint32_t)(int32_t)((int32_t)(random_word(&random) % 65) - 32);
        input[15] = (uint32_t)(int32_t)((int32_t)(random_word(&random) % 65) - 32);
        FloatBits sine = {.number = sinf(values[4].number)};
        FloatBits cosine = {.number = cosf(values[4].number)};
        FloatBits negative_sine = {.number = -sine.number};
        state[4] = state[7] = cosine.bits;
        state[5] = sine.bits;
        state[6] = negative_sine.bits;
        state[8] = input[5];
        state[9] = input[6];
        state[10] = input[7];
        state[11] = input[8];
        state[12] = input[9];
        state[13] = input[10];
        state[18] = input[4];
        state[19] = input[11];
        viewport[0] = input[14];
        viewport[1] = input[15];
        viewport[2] = input[14] + input[12];
        viewport[3] = input[15] + input[13];
        batcher[46] = (uint32_t)(uintptr_t)(vertices + 3);
        batcher[49] = (uint32_t)(uintptr_t)(coordinates + 2);
        draw(image, context, values[0].number, values[1].number, values[2].number,
             values[3].number, uv, shader, NULL);
        uint32_t visible = batcher[46] == (uint32_t)(uintptr_t)(vertices + 21);
        if ((!visible && batcher[46] != (uint32_t)(uintptr_t)(vertices + 3)) ||
            batcher[49] != (uint32_t)(uintptr_t)(coordinates + (visible ? 14 : 2))) {
            fail("Native rectangle exceeded controlled batch storage\n");
        }
        print_words(input, 16);
        print_words(&visible, 1);
        if (visible) {
            print_words(vertices + 3, 18);
            print_words(coordinates + 2, 12);
        }
    }
    semihost(4, "AB image rectangle reference complete\n");
}

#endif
