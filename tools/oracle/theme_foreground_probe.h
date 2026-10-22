#ifndef AB_ORACLE_THEME_FOREGROUND_H
#define AB_ORACLE_THEME_FOREGROUND_H

static uint32_t theme_foreground_state[39], theme_foreground_fills;
static uint32_t theme_foreground_calls, theme_foreground_queries;
static int32_t theme_foreground_size[2];

static void *theme_foreground_get_state(void *context) {
    (void)context;
    ++theme_foreground_queries;
    return theme_foreground_state;
}

static int32_t theme_foreground_width(void *context) {
    (void)context;
    return theme_foreground_size[0];
}

static int32_t theme_foreground_height(void *context) {
    (void)context;
    return theme_foreground_size[1];
}

static int theme_foreground_fill(void *context, uint32_t color, int32_t left,
                                 int32_t top, int32_t width, int32_t height,
                                 uint32_t blend) {
    (void)context;
    if (++theme_foreground_fills != 1 || blend) {
        fail("Unexpected native foreground fill boundary\n");
    }
    uint32_t event[] = {color,
                        (uint32_t)left,
                        (uint32_t)top,
                        (uint32_t)width,
                        (uint32_t)height,
                        blend == 0,
                        theme_foreground_state[16],
                        theme_foreground_queries};
    print_words(event, 8);
    return 0;
}

static void theme_foreground_layer(uint32_t *layer, void *context, void *camera,
                                   uint32_t filter) {
    (void)context;
    (void)camera;
    if (++theme_foreground_calls > 8) {
        fail("Native foreground layer dispatch exceeds controlled vector\n");
    }
    uint32_t event[] = {layer[1], filter, theme_foreground_state[16],
                        theme_foreground_fills};
    print_words(event, 4);
    theme_foreground_state[16] = layer[2];
}

static void probe_theme_foreground(void) {
    void (*draw)(void *, void *, void *, uint32_t) =
        (void (*)(void *, void *, void *, uint32_t))find_function(
            "_ZN16ThemeComposition14drawForegroundEPN2gr7ContextEPN3gfx6CameraEj");
    uint32_t composition[17] = {0}, layers[8][24] = {{0}}, pointers[8],
             camera[30] = {0};
    uint32_t vtable[29] = {0}, base_vtable[5] = {0}, rectangle_vtable[5] = {0};
    uint32_t context[] = {(uint32_t)(uintptr_t)vtable}, game[18] = {0};
    const uint32_t *base = (const uint32_t *)find_symbol("_ZTV10ThemeLayer", 1);
    const uint32_t *rectangle =
        (const uint32_t *)find_symbol("_ZTV14RectThemeLayer", 1);
    for (size_t index = 0; index < 2; ++index) {
        base_vtable[index] = base[index];
        rectangle_vtable[index] = rectangle[index];
    }
    base_vtable[4] = rectangle_vtable[4] = (uint32_t)(uintptr_t)theme_foreground_layer;
    composition[0] = (uint32_t)(uintptr_t)game;
    composition[5] = (uint32_t)(uintptr_t)pointers;
    game[0x44 / 4] = (uint32_t)(uintptr_t)context;
    vtable[16] = (uint32_t)(uintptr_t)theme_foreground_fill;
    vtable[24] = (uint32_t)(uintptr_t)theme_foreground_get_state;
    vtable[27] = (uint32_t)(uintptr_t)theme_foreground_width;
    vtable[28] = (uint32_t)(uintptr_t)theme_foreground_height;
    uint32_t random = UINT32_C(0x520f0e61);
    semihost(4, "AB_ANDROID_THEME_FOREGROUND_V1 4096\n");
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[33] = {0};
        input[0] = (uint32_t)(item % 9);
        input[1] = random_word(&random);
        input[2] = 480u << (item % 3);
        input[3] = 320u << (item % 3);
        for (size_t index = 4; index < 15; ++index) {
            FloatBits value = {
                .number = (float)((int32_t)(random_word(&random) % 4097) - 2048) / 8};
            input[index] = value.bits;
        }
        const float zooms[] = {0, 0.4f, 1, 2, -0.5f};
        FloatBits zoom = {.number = zooms[item % 5]};
        input[5] = zoom.bits;
        input[6] = random_word(&random);
        input[7] = (uint32_t)(item % 2);
        for (size_t index = 11; index < 14; ++index) {
            FloatBits color = {.number = (float)(random_word(&random) % 65) / 32};
            input[index] = color.bits;
            composition[index] = color.bits;
        }
        for (size_t index = 0; index < 8; ++index) {
            uint32_t identity = random_word(&random) % 8;
            input[15 + index] = identity + 1;
            pointers[index] = (uint32_t)(uintptr_t)layers[identity];
            layers[index][0] =
                (uint32_t)(uintptr_t)((input[7] ? base_vtable : rectangle_vtable) + 2);
            layers[index][1] = (uint32_t)index + 1;
            input[23 + index] = random_float(&random, 118, 20, 1);
            layers[index][2] = input[23 + index];
            layers[index][7] = input[8];
            layers[index][9] = input[9];
            layers[index][11] = input[10];
        }
        composition[6] = (uint32_t)(uintptr_t)(pointers + input[0]);
        input[31] = random_word(&random);
        camera[26] = input[31];
        camera[27] = input[4];
        camera[28] = input[6];
        camera[29] = input[5];
        theme_foreground_size[0] = (int32_t)input[2];
        theme_foreground_size[1] = (int32_t)input[3];
        theme_foreground_state[16] = input[14];
        theme_foreground_fills = theme_foreground_calls = theme_foreground_queries = 0;
        print_words(input, 33);
        draw(composition, context, camera, input[1]);
        uint32_t result[] = {theme_foreground_calls, theme_foreground_fills,
                             theme_foreground_state[16]};
        print_words(result, 3);
    }
    semihost(4, "AB theme foreground reference complete\n");
}

#endif
