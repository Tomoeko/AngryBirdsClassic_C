#ifndef AB_ORACLE_FONT_LAYOUT_H
#define AB_ORACLE_FONT_LAYOUT_H

static uint32_t layout_events[16][15], layout_event_count;

static void *layout_context_state(uint32_t *context) {
    return (void *)(uintptr_t)context[1];
}

static void layout_glyph(void *data, uint32_t *context, const uint32_t *sprite, float x,
                         float y) {
    (void)data;
    if (layout_event_count == 16) {
        fail("Native font layout callback exceeds bounded output\n");
    }
    uint32_t *event = layout_events[layout_event_count++];
    size_t index = 0;
    while (index < 7 && metric_sprites[index] != sprite) {
        ++index;
    }
    if (index == 7) {
        fail("Native font layout selected an unknown controlled glyph\n");
    }
    uint32_t *state = layout_context_state(context);
    FloatBits px = {.number = x}, py = {.number = y};
    event[0] = (uint32_t)index;
    event[1] = px.bits;
    event[2] = py.bits;
    event[3] = state[12];
    event[4] = state[13];
}

#if defined(AB_ORACLE_FONT_IMAGE_ONLY)
static uint32_t layout_images[7][2], layout_sheets[7][4], layout_image_vtable[7];

static void layout_image(const uint32_t *image, uint32_t *context, float x, float y,
                         float width, float height, const uint32_t *uv,
                         const void *shader, const void *color) {
    if (layout_event_count == 16 || image[1] >= 7 || shader || color) {
        fail("Native font image boundary is outside controlled inputs\n");
    }
    uint32_t *event = layout_events[layout_event_count++];
    uint32_t *state = layout_context_state(context);
    FloatBits px = {.number = x}, py = {.number = y};
    FloatBits w = {.number = width}, h = {.number = height};
    event[0] = image[1];
    event[1] = px.bits;
    event[2] = py.bits;
    event[3] = w.bits;
    event[4] = h.bits;
    event[5] = state[12];
    event[6] = state[13];
    for (size_t component = 0; component < 8; ++component) {
        event[7 + component] = uv[component];
    }
}
#endif

static uint32_t layout_coordinate(uint32_t *random) {
    static const uint32_t values[] = {0,          0x80000000, 1,          0x80000001,
                                      0x3f800000, 0xbf800000, 0x3f000000, 0x40a00000,
                                      0x45800001, 0xc5800001, 0x3eaaaaab, 0x4b000001,
                                      0xcb000001};
    return values[random_word(random) % (sizeof(values) / sizeof(*values))];
}

static void probe_font_layout(void) {
    typedef struct {
        int32_t vertical, horizontal;
    } Anchor;
    typedef void (*DrawString)(const void *, void *, const void *, int32_t, int32_t,
                               float, float, Anchor, float, float);
    DrawString draw =
        (DrawString)find_function("_ZNK4game10BitmapFont10drawStringEPN2gr7ContextERKSb"
                                  "IDiSt11char_traitsIDiESaIDiEEiiffNS_6AnchorEff");
    uint32_t font[18] = {0}, state[24] = {0}, context_vtable[25] = {0};
    font[0] = (uint32_t)find_symbol("_ZTVN4game10BitmapFontE", 1) + 8;
    font[16] = 1;
    font[17] = (uint32_t)(uintptr_t)layout_glyph;
#if defined(AB_ORACLE_FONT_IMAGE_ONLY)
    font[16] = 0;
    layout_image_vtable[6] = (uint32_t)(uintptr_t)layout_image;
    for (uint32_t glyph = 0; glyph < 7; ++glyph) {
        layout_images[glyph][0] = (uint32_t)(uintptr_t)layout_image_vtable;
        layout_images[glyph][1] = glyph;
        layout_sheets[glyph][3] = (uint32_t)(uintptr_t)layout_images[glyph];
        metric_sprites[glyph][2] = (uint32_t)(uintptr_t)layout_sheets[glyph];
    }
#endif
    context_vtable[24] = (uint32_t)(uintptr_t)layout_context_state;
    uint32_t context[] = {(uint32_t)(uintptr_t)context_vtable,
                          (uint32_t)(uintptr_t)state};
    uint32_t storage[20] = {0}, *text = storage + 3;
    uint32_t random = UINT32_C(0x520f1a70);
#if defined(AB_ORACLE_FONT_IMAGE_ONLY)
    semihost(4, "AB_ANDROID_FONT_IMAGE_V1 4096\n");
#else
    semihost(4, "AB_ANDROID_FONT_LAYOUT_V1 4096\n");
#endif
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[46];
        input[0] = (uint32_t)item % 8;
        input[1] = (uint32_t)((int32_t)(random_word(&random) % 65) - 32);
        input[2] = (uint32_t)((int32_t)(random_word(&random) % 81) - 40);
        input[3] = (uint32_t)((int32_t)(random_word(&random) % 81) - 40);
        input[4] = (uint32_t)((int32_t)(random_word(&random) % 241) - 120);
        input[5] = (uint32_t)(item / 8) % 17;
        input[6] = random_word(&random) % (input[5] + 1);
        input[7] = random_word(&random) % (input[5] - input[6] + 1);
        for (size_t field = 8; field < 14; ++field) {
            input[field] = layout_coordinate(&random);
        }
        input[14] = random_word(&random) % 5;
        input[15] = random_word(&random) % 4;
        for (size_t glyph = 0; glyph < 7; ++glyph) {
            uint32_t width = (uint32_t)((int32_t)(random_word(&random) % 161) - 32);
            uint32_t height = (uint32_t)((int32_t)(random_word(&random) % 161) - 32);
            input[16 + glyph] = (width & 0xffff) | (height << 16);
            input[23 + glyph] = (uint32_t)((int32_t)(random_word(&random) % 101) - 50);
            metric_sprites[glyph][5] = input[16 + glyph];
            FloatBits pivot = {.number = (float)(int32_t)input[23 + glyph]};
            metric_sprites[glyph][6] = 0;
            metric_sprites[glyph][7] = pivot.bits;
#if defined(AB_ORACLE_FONT_IMAGE_ONLY)
            FloatBits u = {.number = (float)(int16_t)width / 256.0f};
            FloatBits v = {.number = (float)(int16_t)height / 256.0f};
            const uint32_t uv[] = {0, 0, u.bits, 0, 0, v.bits, u.bits, v.bits};
            for (size_t component = 0; component < 8; ++component) {
                metric_sprites[glyph][8 + component] = uv[component];
            }
#endif
        }
        for (size_t character = 0; character < 16; ++character) {
            uint32_t selected = random_word(&random) % 10;
            input[30 + character] = selected < 7 ? metric_codes[selected] : 10;
            text[character] = input[30 + character];
        }
        storage[0] = storage[1] = input[5];
        font[5] = metric_tree(0, input[0], (uint32_t)(uintptr_t)(font + 4));
        font[10] = input[1];
        font[11] = input[2];
        font[12] = input[3];
        font[13] = input[4];
        state[12] = input[12];
        state[13] = input[13];
        layout_event_count = 0;
        FloatBits x = {.bits = input[8]}, y = {.bits = input[9]};
        FloatBits sx = {.bits = input[10]}, sy = {.bits = input[11]};
        Anchor anchor = {(int32_t)input[14], (int32_t)input[15]};
        print_words(input, 46);
        draw(font, context, &text, (int32_t)input[6], (int32_t)input[7], x.number,
             y.number, anchor, sx.number, sy.number);
        print_words(&layout_event_count, 1);
        for (uint32_t event = 0; event < layout_event_count; ++event) {
#if defined(AB_ORACLE_FONT_IMAGE_ONLY)
            print_words(layout_events[event], 15);
#else
            print_words(layout_events[event], 5);
#endif
        }
        print_words(state + 12, 2);
    }
#if defined(AB_ORACLE_FONT_IMAGE_ONLY)
    semihost(4, "AB font image reference complete\n");
#else
    semihost(4, "AB font layout reference complete\n");
#endif
}

#endif
