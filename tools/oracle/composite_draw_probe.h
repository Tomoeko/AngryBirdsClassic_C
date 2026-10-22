#ifndef AB_ORACLE_COMPOSITE_DRAW_H
#define AB_ORACLE_COMPOSITE_DRAW_H

static uint32_t composite_draw_state[39], composite_draw_count;

static void *composite_draw_get_state(void *context) {
    (void)context;
    return composite_draw_state;
}

static void composite_draw_image(void *image, void *context, const uint32_t *points,
                                 const uint32_t *uv, const void *color,
                                 const void *shader) {
    (void)image;
    (void)context;
    (void)uv;
    (void)color;
    if (shader)
        fail("Composite unexpectedly selected an explicit shader\n");
    ++composite_draw_count;
    print_words(points, 12);
}

static void probe_composite_draw(void) {
    void (*draw)(void *, void *, float, float, int32_t, int32_t) =
        (void (*)(void *, void *, float, float, int32_t, int32_t))find_function(
            "_ZNK4game11CompoSprite4drawEPN2gr7ContextEffNS_6AnchorE");
    void (*calculate)(void *) =
        (void (*)(void *))find_function("_ZN4game11CompoSprite15calculateBoundsEv");
    uint32_t context_vtable[25] = {0}, context[14] = {0}, image_vtable[9] = {0};
    uint32_t image[5] = {0}, sheet[4] = {0};
    uint32_t composite[15] = {0}, entries[4][12] = {{0}}, sprites[4][16] = {{0}};
    uint32_t pointers[4], counters[4][2];
    context_vtable[24] = (uint32_t)(uintptr_t)composite_draw_get_state;
    context[0] = (uint32_t)(uintptr_t)context_vtable;
    image[0] = (uint32_t)(uintptr_t)image_vtable;
    image_vtable[8] = (uint32_t)(uintptr_t)composite_draw_image;
    sheet[3] = (uint32_t)(uintptr_t)image;
    for (size_t index = 0; index < 4; ++index) {
        pointers[index] = (uint32_t)(uintptr_t)entries[index];
        entries[index][1] = (uint32_t)(uintptr_t)counters[index];
        entries[index][3] = (uint32_t)(uintptr_t)sprites[index];
        counters[index][0] = 1000;
        counters[index][1] = 1;
        sprites[index][2] = (uint32_t)(uintptr_t)sheet;
    }
    composite[2] = (uint32_t)(uintptr_t)pointers;
    uint32_t random = UINT32_C(0x520c07d1);
    semihost(4, "AB_ANDROID_COMPOSITE_DRAW_V1 2048\n");
    for (size_t item = 0; item < 2048; ++item) {
        uint32_t count = (uint32_t)(item % 4) + 1;
        composite[3] = (uint32_t)(uintptr_t)(pointers + count);
        uint32_t input[12] = {count};
        for (size_t index = 1; index < 10; ++index) {
            FloatBits value = {
                .number = (float)((int32_t)(random_word(&random) % 1025) - 512) / 16};
            input[index] = value.bits;
        }
        FloatBits sx = {.number = (float)(item % 17 + 1) / 4};
        FloatBits sy = {.number = (float)(item % 13 + 1) / 4};
        if (item % 3 == 0) {
            sx.bits = sy.bits = UINT32_C(0x40088889);
        } else if (item % 3 == 1) {
            sx.number = (float)(item % 17 + 1) / 7;
            sy.number = (float)(item % 13 + 1) / 9;
        }
        input[3] = sx.bits;
        input[4] = sy.bits;
        input[5] = 0;
        input[10] = (uint32_t)(item % 5);
        input[11] = (uint32_t)(item % 4);
        for (size_t index = 0; index < 39; ++index)
            composite_draw_state[index] = 0;
        composite_draw_state[4] = composite_draw_state[7] = UINT32_C(0x3f800000);
        composite_draw_state[16] = UINT32_C(0x3f800000);
        for (size_t index = 0; index < 4; ++index)
            composite_draw_state[8 + index] = input[1 + index];
        composite_draw_state[12] = input[6];
        composite_draw_state[13] = input[7];
        print_words(input, 12);
        for (size_t index = 0; index < count; ++index) {
            uint32_t fields[12];
            fields[0] = random_word(&random) % 513 + 1;
            fields[1] = random_word(&random) % 513 + 1;
            fields[2] = (uint32_t)((int32_t)(random_word(&random) % 129) - 64);
            fields[3] = (uint32_t)((int32_t)(random_word(&random) % 129) - 64);
            for (size_t field = 4; field < 11; ++field) {
                FloatBits value = {
                    .number = (float)((int32_t)(random_word(&random) % 129) - 64) / 8};
                fields[field] = value.bits;
            }
            for (size_t field = 6; field < 10; ++field)
                fields[field] = (fields[field] & UINT32_C(0x80000000)) |
                                (random_word(&random) & UINT32_C(0x007fffff)) |
                                UINT32_C(0x3f000000);
            /* Isolate the axis-aligned source domain first, retaining signed
             * part scales/flips and all original anchor kinds. */
            if (item < 1024)
                fields[10] = 0;
            fields[11] = 1;
            sprites[index][5] = fields[0] | fields[1] << 16;
            for (size_t axis = 0; axis < 2; ++axis) {
                FloatBits pivot = {.number = (float)(int32_t)fields[2 + axis]};
                sprites[index][6 + axis] = pivot.bits;
            }
            for (size_t field = 4; field < 12; ++field)
                entries[index][field] = fields[field];
            print_words(fields, 12);
        }
        calculate(composite);
        print_words(composite + 11, 4);
        composite_draw_count = 0;
        FloatBits x = {.bits = input[8]}, y = {.bits = input[9]};
        draw(composite, context, x.number, y.number, (int32_t)input[10],
             (int32_t)input[11]);
        if (composite_draw_count != count)
            fail("Native composite did not visit each enabled part\n");
    }
    semihost(4, "AB composite draw reference complete\n");
}

#endif
