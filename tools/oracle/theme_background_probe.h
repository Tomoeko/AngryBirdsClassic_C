#ifndef AB_ORACLE_THEME_BACKGROUND_H
#define AB_ORACLE_THEME_BACKGROUND_H

static uint32_t theme_background_state[24], theme_background_calls;
static uint32_t *theme_background_game, *theme_background_camera;

static void *theme_background_get_state(void *context) {
    (void)context;
    return theme_background_state;
}

static void theme_background_layer(uint32_t *layer, void *context, void *camera,
                                   uint32_t filter) {
    (void)context;
    if (camera != theme_background_camera || ++theme_background_calls > 8) {
        fail("Unexpected native background layer boundary\n");
    }
    uint32_t event[] = {layer[1], filter, theme_background_game[0xf0 / 4],
                        theme_background_state[16]};
    print_words(event, 4);
    theme_background_state[16] = layer[2];
}

static void probe_theme_background(void) {
    void (*draw)(void *, void *, void *, uint32_t) =
        (void (*)(void *, void *, void *, uint32_t))find_function(
            "_ZN16ThemeComposition14drawBackgroundEPN2gr7ContextEPN3gfx6CameraEj");
    uint32_t composition[17] = {0}, layers[8][3] = {{0}}, pointers[8];
    uint32_t context_vtable[25] = {0}, layer_vtable[3] = {0}, camera[1] = {0};
    uint32_t context[] = {(uint32_t)(uintptr_t)context_vtable};
    static uint32_t game[0x1884 / 4];
    theme_background_game = game;
    theme_background_camera = camera;
    composition[0] = (uint32_t)(uintptr_t)game;
    composition[2] = (uint32_t)(uintptr_t)pointers;
    context_vtable[24] = (uint32_t)(uintptr_t)theme_background_get_state;
    layer_vtable[2] = (uint32_t)(uintptr_t)theme_background_layer;
    for (size_t index = 0; index < 8; ++index) {
        pointers[index] = (uint32_t)(uintptr_t)layers[index];
        layers[index][0] = (uint32_t)(uintptr_t)layer_vtable;
        layers[index][1] = (uint32_t)index + 1;
    }
    uint32_t random = UINT32_C(0x520ba6c0);
    semihost(4, "AB_ANDROID_THEME_BACKGROUND_V1 8192\n");
    for (size_t item = 0; item < 8192; ++item) {
        uint32_t input[22];
        input[0] = (uint32_t)(item % 9);
        input[1] = random_word(&random);
        for (size_t index = 2; index < 14; ++index) {
            FloatBits value = {
                .number = (float)((int32_t)(random_word(&random) % 8193) - 2048) / 8};
            if (item % 17 == 0) {
                value.bits = random_float(&random, 118, 48, 1);
            }
            input[index] = value.bits;
        }
        if (item % 19 == 0) {
            const float boundaries[] = {-0.5f, -0.0f, 0, 254.5f, 255, 255.5f, 256};
            for (size_t index = 0; index < 3; ++index) {
                FloatBits value = {.number = boundaries[(item + index) % 7]};
                input[2 + index] = value.bits;
            }
        }
        for (size_t index = 0; index < 3; ++index) {
            composition[8 + index] = input[2 + index];
        }
        for (size_t index = 0; index < 8; ++index) {
            layers[index][2] = input[6 + index];
            uint32_t identity = random_word(&random) % 8 + 1;
            input[14 + index] = identity;
            pointers[index] = (uint32_t)(uintptr_t)layers[identity - 1];
        }
        composition[3] = (uint32_t)(uintptr_t)(pointers + input[0]);
        theme_background_state[16] = input[5];
        theme_background_calls = 0;
        game[0xf0 / 4] = UINT32_C(0x12345678);
        print_words(input, 22);
        draw(composition, context, camera, input[1]);
        uint32_t result[] = {theme_background_calls, game[0xf0 / 4],
                             theme_background_state[16]};
        print_words(result, 3);
    }
    semihost(4, "AB theme background reference complete\n");
}

#endif
