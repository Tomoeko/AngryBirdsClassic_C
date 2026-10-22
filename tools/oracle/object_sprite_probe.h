#ifndef AB_ORACLE_OBJECT_SPRITE_H
#define AB_ORACLE_OBJECT_SPRITE_H

static uint32_t object_sprite_events[21], object_sprite_event_count;

static void object_sprite_image(const void *image, const void *context,
                                const uint32_t *vertices, const uint32_t *uv,
                                const void *color, const void *shader) {
    (void)image;
    (void)context;
    if (object_sprite_event_count++ || color || shader) {
        fail("Unexpected object sprite image boundary\n");
    }
    for (size_t index = 0; index < 12; ++index) {
        object_sprite_events[index] = vertices[index];
    }
    for (size_t index = 0; index < 8; ++index) {
        object_sprite_events[12 + index] = uv[index];
    }
}

static float object_sprite_coordinate(uint32_t *random) {
    static const uint32_t edges[] = {0,          0x80000000, 1,          0x80000001,
                                     0x3f800000, 0xbf800000, 0x3f000000, 0x3eaaaaab,
                                     0x45800001, 0xc5800001};
    FloatBits value;
    uint32_t choice = random_word(random);
    if (choice % 3 == 0) {
        value.bits = edges[random_word(random) % (sizeof(edges) / sizeof(*edges))];
    } else {
        value.number = (float)((int32_t)(choice % 65537) - 32768) / 32.0f;
    }
    return value.number;
}

static void probe_object_sprite(void) {
    typedef void (*Draw)(void *, void *, const void *, float, float, float, float,
                         float, float, float, float, float);
    Draw draw = (Draw)find_function("_ZN3gfx18GameObjectRenderer10drawSpriteEPN2gr7Cont"
                                    "extEPKN4game6SpriteEfffffffff");
    uint32_t image_vtable[9] = {0}, image[1], sheet[4] = {0};
    uint32_t sprite[16] = {0}, renderer[6] = {0}, context[1] = {0};
    image_vtable[8] = (uint32_t)(uintptr_t)object_sprite_image;
    image[0] = (uint32_t)(uintptr_t)image_vtable;
    sheet[3] = (uint32_t)(uintptr_t)image;
    sprite[2] = (uint32_t)(uintptr_t)sheet;
    uint32_t random = UINT32_C(0x5200b1ec);
    semihost(4, "AB_ANDROID_OBJECT_SPRITE_V1 4096\n");
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[20];
        uint32_t width = random_word(&random) & 0xffff;
        uint32_t height = random_word(&random) & 0xffff;
        input[0] = width | (height << 16);
        sprite[5] = input[0];
        FloatBits values[11];
        for (size_t index = 0; index < 2; ++index) {
            values[index].number =
                (float)((int32_t)(random_word(&random) % 65536) - 32768);
            sprite[6 + index] = input[1 + index] = values[index].bits;
        }
        for (size_t index = 2; index < 11; ++index) {
            values[index].number = object_sprite_coordinate(&random);
            input[index + 1] = values[index].bits;
        }
        values[4].bits = input[5] = next_extended_angle(&random, item);
        for (size_t index = 0; index < 8; ++index) {
            FloatBits uv = {.number = (float)(random_word(&random) % 2049) / 2048.0f};
            sprite[8 + index] = input[12 + index] = uv.bits;
        }
        object_sprite_event_count = 0;
        draw(renderer, context, sprite, values[2].number, values[3].number,
             values[4].number, values[5].number, values[6].number, values[7].number,
             values[8].number, values[9].number, values[10].number);
        if (object_sprite_event_count != 1) {
            fail("Native object sprite draw omitted its image boundary\n");
        }
        print_words(input, 20);
        print_words(object_sprite_events, 20);
    }
    semihost(4, "AB object sprite reference complete\n");
}

#endif
