#ifndef AB_ORACLE_OBJECT_RENDER_H
#define AB_ORACLE_OBJECT_RENDER_H

static uint32_t object_render_event[17], object_render_count;

static void *object_render_context(uint32_t *context) {
    return (void *)(uintptr_t)context[1];
}

static void object_render_image(const void *image, uint32_t *context, float x, float y,
                                float width, float height, const uint32_t *uv,
                                const void *shader, const void *color) {
    (void)image;
    (void)uv;
    if (object_render_count++ || shader || color) {
        fail("Unexpected ordinary object image callback\n");
    }
    FloatBits values[] = {
        {.number = x}, {.number = y}, {.number = width}, {.number = height}};
    for (size_t index = 0; index < 4; ++index) {
        object_render_event[index] = values[index].bits;
    }
    uint32_t *state = object_render_context(context);
    for (size_t index = 0; index < 10; ++index) {
        object_render_event[4 + index] = state[4 + index];
    }
    object_render_event[14] = state[16];
    object_render_event[15] = state[18];
    object_render_event[16] = state[19];
}

static void probe_object_render(void) {
    typedef void (*Draw)(void *, void *, const void *);
    Draw draw = (Draw)find_function(
        "_ZN3gfx18GameObjectRenderer4drawEPN2gr7ContextEPNS_6CameraE");
    uint32_t vtable[25] = {0}, state[39] = {0}, image_vtable[7] = {0};
    uint32_t context[2], image[1], sheet[4] = {0}, sprite[16] = {0};
    uint32_t renderer[20] = {0}, object[48] = {0}, asset[7] = {0}, camera[30] = {0};
    vtable[24] = (uint32_t)(uintptr_t)object_render_context;
    context[0] = (uint32_t)(uintptr_t)vtable;
    context[1] = (uint32_t)(uintptr_t)state;
    image_vtable[6] = (uint32_t)(uintptr_t)object_render_image;
    image[0] = (uint32_t)(uintptr_t)image_vtable;
    sheet[3] = (uint32_t)(uintptr_t)image;
    sprite[2] = (uint32_t)(uintptr_t)sheet;
    asset[3] = (uint32_t)(uintptr_t)sprite;
    renderer[2] = (uint32_t)(uintptr_t)asset;
    renderer[19] = (uint32_t)(uintptr_t)object;
    object[10] = 1;
    uint32_t random = UINT32_C(0x5200b1a0);
    semihost(4, "AB_ANDROID_OBJECT_RENDER_V1 4096\n");
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[12];
        input[0] = random_word(&random);
        sprite[5] = input[0];
        for (size_t index = 1; index < 12; ++index) {
            FloatBits value;
            if (index < 3) {
                value.number = (float)((int32_t)(random_word(&random) % 65536) - 32768);
            } else {
                value.number =
                    (float)((int32_t)(random_word(&random) % 65537) - 32768) / 32.0f;
            }
            input[index] = value.bits;
        }
        input[5] = next_extended_angle(&random, item);
        static const float scales[] = {1, -1, 2, 0.5f, 0.33333334f, -0.33333334f};
        for (size_t index = 6; index < 8; ++index) {
            FloatBits value = {.number = scales[random_word(&random) % 6]};
            input[index] = value.bits;
        }
        sprite[6] = input[1];
        sprite[7] = input[2];
        object[36] = input[3];
        object[37] = input[4];
        object[38] = input[5];
        object[7] = input[6];
        object[8] = input[7];
        camera[26] = input[8];
        camera[27] = input[9];
        camera[28] = input[10];
        camera[29] = input[11];
        object_render_count = 0;
        draw(renderer, context, camera);
        if (object_render_count != 1) {
            fail("Ordinary object renderer omitted its image callback\n");
        }
        print_words(input, 12);
        print_words(object_render_event, 17);
    }
    semihost(4, "AB object render reference complete\n");
}

#endif
