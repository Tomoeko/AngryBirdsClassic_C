#ifndef AB_ORACLE_SLING_RECTANGLE_H
#define AB_ORACLE_SLING_RECTANGLE_H

typedef void (*SlingRectangleImage)(void *, void *, float, float, float, float,
                                    const void *, void *, const void *);
static SlingRectangleImage sling_rectangle_native_image;
static uint32_t sling_rectangle_shader[2], sling_rectangle_calls;

static uint32_t *sling_rectangle_state(uint32_t *context) {
    return (uint32_t *)(uintptr_t)context[1];
}

static uint32_t *sling_rectangle_viewport(uint32_t *context) {
    return (uint32_t *)(uintptr_t)context[2];
}

static void sling_rectangle_image(void *image, void *context, float x, float y,
                                  float width, float height, const void *uv,
                                  void *shader, const void *color) {
    if (++sling_rectangle_calls != 1 || shader || color) {
        fail("Unexpected original sling rectangle image boundary\n");
    }
    sling_rectangle_native_image(image, context, x, y, width, height, uv,
                                 sling_rectangle_shader, NULL);
}

static void probe_sling_rectangle(void) {
    typedef struct {
        uint32_t vertical, horizontal;
    } Anchor;
    typedef void (*Draw)(void *, const void *, float, float, Anchor);
    Draw draw =
        (Draw)find_function("_ZNK4game9Resources10drawSpriteERKSsffNS_6AnchorE");
    sling_rectangle_native_image = (SlingRectangleImage)find_function(
        "_ZN2gr5gles28GL_Image4drawEPNS_7ContextEffffPKN4math6float2EPNS_6ShaderEPNS4_"
        "6float4E");
    RubberbandFixture fixture;
    rubberband_prepare(&fixture);
    uint32_t context_vtable[26] = {0}, context[14] = {0}, state[39] = {0};
    uint32_t viewport[4] = {0}, batcher[54] = {0};
    uint32_t vertices[24] = {0}, coordinates[16] = {0};
    context_vtable[24] = (uint32_t)(uintptr_t)sling_rectangle_state;
    context_vtable[25] = (uint32_t)(uintptr_t)sling_rectangle_viewport;
    context[0] = (uint32_t)(uintptr_t)context_vtable;
    context[1] = (uint32_t)(uintptr_t)state;
    context[2] = (uint32_t)(uintptr_t)viewport;
    context[13] = (uint32_t)(uintptr_t)batcher;
    fixture.resources[3] = (uint32_t)(uintptr_t)context;
    fixture.image_vtable[6] = (uint32_t)(uintptr_t)sling_rectangle_image;
    fixture.image[3] = (uint32_t)(uintptr_t)context;
    batcher[41] = 2;
    batcher[42] = (uint32_t)(uintptr_t)context;
    batcher[43] = (uint32_t)(uintptr_t)fixture.image;
    batcher[44] = (uint32_t)(uintptr_t)sling_rectangle_shader;
    batcher[45] = (uint32_t)(uintptr_t)vertices;
    batcher[47] = (uint32_t)(uintptr_t)(vertices + 24);
    batcher[48] = (uint32_t)(uintptr_t)coordinates;
    batcher[50] = (uint32_t)(uintptr_t)(coordinates + 16);
    state[16] = batcher[18] = UINT32_C(0x3f800000);
    uint32_t random = UINT32_C(0x52051c2e);
    semihost(4, "AB_ANDROID_SLING_RECTANGLE_V1 4096\n");
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[14];
        int32_t width = (int32_t)(random_word(&random) % 513) - 256;
        int32_t height = (int32_t)(random_word(&random) % 513) - 256;
        fixture.sprite[5] = input[0] =
            (uint32_t)(uint16_t)width | ((uint32_t)(uint16_t)height << 16);
        for (size_t field = 1; field < 3; ++field) {
            int32_t pivot = (int32_t)(random_word(&random) % 1025) - 512;
            input[field] = (uint32_t)pivot;
            FloatBits value = {.number = (float)pivot};
            fixture.sprite[5 + field] = value.bits;
        }
        FloatBits values[9];
        for (size_t field = 0; field < 9; ++field) {
            values[field].number =
                (float)((int32_t)(random_word(&random) % 4097) - 2048) / 4.0f;
        }
        const float scales[] = {0, -0.0f, 1, -1, 2, 0.5f, 0.33333334f};
        values[4].number = scales[random_word(&random) % 7];
        values[5].number = scales[random_word(&random) % 7];
        values[6].bits = item % 3 ? next_extended_angle(&random, item) : 0;
        for (size_t field = 0; field < 9; ++field) {
            input[3 + field] = values[field].bits;
        }
        input[12] = 480u << (item % 3);
        input[13] = 320u << (item % 3);
        FloatBits sine = {.number = sinf(values[6].number)};
        FloatBits cosine = {.number = cosf(values[6].number)};
        FloatBits negative_sine = {.number = -sine.number};
        state[4] = state[7] = cosine.bits;
        state[5] = sine.bits;
        state[6] = negative_sine.bits;
        state[8] = input[5];
        state[9] = input[6];
        state[10] = input[7];
        state[11] = input[8];
        state[12] = input[10];
        state[13] = input[11];
        state[18] = input[9];
        viewport[2] = input[12];
        viewport[3] = input[13];
        batcher[46] = (uint32_t)(uintptr_t)(vertices + 3);
        batcher[49] = (uint32_t)(uintptr_t)(coordinates + 2);
        sling_rectangle_calls = 0;
        draw(fixture.resources, &fixture.name, values[0].number, values[1].number,
             (Anchor){4, 3});
        uint32_t visible = batcher[46] == (uint32_t)(uintptr_t)(vertices + 21);
        if (sling_rectangle_calls != 1 ||
            (!visible && batcher[46] != (uint32_t)(uintptr_t)(vertices + 3)) ||
            batcher[49] != (uint32_t)(uintptr_t)(coordinates + (visible ? 14 : 2))) {
            fail("Native sling rectangle exceeded controlled batch storage\n");
        }
        print_words(input, 14);
        print_words(&visible, 1);
        if (visible) {
            print_words(vertices + 3, 18);
            print_words(coordinates + 2, 12);
        }
    }
    semihost(4, "AB sling rectangle reference complete\n");
}

#endif
