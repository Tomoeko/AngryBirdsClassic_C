#ifndef AB_ORACLE_SPRITE_SIZED_H
#define AB_ORACLE_SPRITE_SIZED_H

static uint32_t sprite_sized_result[12], sprite_sized_count;

static void sprite_sized_image(void *image, void *context, float x, float y,
                               float width, float height, const uint32_t *uv,
                               void *shader, void *color) {
    (void)image;
    (void)context;
    if (sprite_sized_count++ || shader || color) {
        fail("Unexpected native sized Sprite image boundary\n");
    }
    FloatBits values[] = {
        {.number = x}, {.number = y}, {.number = width}, {.number = height}};
    for (size_t index = 0; index < 4; ++index) {
        sprite_sized_result[index] = values[index].bits;
    }
    for (size_t index = 0; index < 8; ++index) {
        sprite_sized_result[4 + index] = uv[index];
    }
}

static void probe_sprite_sized(void) {
    typedef struct {
        int32_t vertical, horizontal;
    } Anchor;
    typedef void (*Draw)(void *, void *, float, float, float, float, Anchor, void *,
                         void *);
    Draw draw = (Draw)find_function("_ZNK4game6Sprite4drawEPN2gr7ContextEffffNS_"
                                    "6AnchorEPNS1_6ShaderEPN4math6float4E");
    uint32_t sprite[16] = {0}, sheet[4] = {0}, image[2] = {0}, image_vtable[7] = {0};
    uint32_t context[1] = {0};
    sprite[0] = (uint32_t)find_symbol("_ZTVN4game6SpriteE", 1) + 8;
    sprite[2] = (uint32_t)(uintptr_t)sheet;
    sheet[3] = (uint32_t)(uintptr_t)image;
    image[0] = (uint32_t)(uintptr_t)image_vtable;
    image_vtable[6] = (uint32_t)(uintptr_t)sprite_sized_image;
    uint32_t random = UINT32_C(0x52051a6e);
    semihost(4, "AB_ANDROID_SPRITE_SIZED_V1 4096\n");
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[14];
        for (size_t field = 0; field < 6; ++field) {
            int32_t value = (int32_t)(random_word(&random) % 8193) - 4096;
            input[field] = (uint32_t)value;
        }
        input[6] = random_word(&random) % 2048 + 1;
        input[7] = random_word(&random) % 2048 + 1;
        for (size_t field = 8; field < 12; ++field) {
            input[field] = random_float(&random, 115, 22, 1);
            if ((item + field) % 13 == 0) {
                input[field] &= UINT32_C(0x80000000);
            }
        }
        input[12] = (uint32_t)(item % 5);
        input[13] = (uint32_t)((item / 5) % 4);
        sprite[4] = (input[0] & 65535) | ((input[1] & 65535) << 16);
        sprite[5] = (input[2] & 65535) | ((input[3] & 65535) << 16);
        FloatBits pivot_x = {.number = (float)(int32_t)input[4]};
        FloatBits pivot_y = {.number = (float)(int32_t)input[5]};
        sprite[6] = pivot_x.bits;
        sprite[7] = pivot_y.bits;
        float left = (float)(int32_t)input[0] / (float)input[6];
        float top = (float)(int32_t)input[1] / (float)input[7];
        float right = (float)((int32_t)input[0] + (int32_t)input[2]) / (float)input[6];
        float bottom = (float)((int32_t)input[1] + (int32_t)input[3]) / (float)input[7];
        const float uv[] = {left, top, right, top, left, bottom, right, bottom};
        for (size_t index = 0; index < 8; ++index) {
            FloatBits value = {.number = uv[index]};
            sprite[8 + index] = value.bits;
        }
        FloatBits values[4];
        for (size_t field = 0; field < 4; ++field) {
            values[field].bits = input[8 + field];
        }
        Anchor anchor = {(int32_t)input[12], (int32_t)input[13]};
        sprite_sized_count = 0;
        draw(sprite, context, values[0].number, values[1].number, values[2].number,
             values[3].number, anchor, NULL, NULL);
        if (sprite_sized_count != 1) {
            fail("Native sized Sprite did not draw one image\n");
        }
        print_words(input, 14);
        print_words(sprite_sized_result, 12);
    }
    semihost(4, "AB sprite sized reference complete\n");
}

#endif
