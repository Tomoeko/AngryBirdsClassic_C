#ifndef AB_ORACLE_FILL_RECTANGLE_H
#define AB_ORACLE_FILL_RECTANGLE_H

static uint32_t fill_rectangle_viewport[4], fill_rectangle_noop_calls;

static void *fill_rectangle_get_state(uint32_t *context) {
    return context + 0xec / 4;
}

static void *fill_rectangle_get_viewport(void *context) {
    (void)context;
    return fill_rectangle_viewport;
}

static int fill_rectangle_noop(void *context, uint32_t color, int32_t left, int32_t top,
                               int32_t width, int32_t height, uint32_t blend) {
    (void)context;
    if (++fill_rectangle_noop_calls != 1 || color || left || top || width || height ||
        blend) {
        fail("Unexpected native fill reset boundary\n");
    }
    return 0;
}

static void probe_fill_rectangle(void) {
    typedef void (*Reset)(void *, float, float, float, float, float, float, float,
                          float, int);
    typedef void (*Fill)(void *, uint32_t, int32_t, int32_t, int32_t, int32_t,
                         uint32_t);
    Reset reset = (Reset)find_function("_ZN7GameLua8drawRectEffffffffb");
    Fill fill = (Fill)find_function(
        "_ZN2gr5gles210GL_Context10fillRect2DEjiiiiNS_7Context5BlendE");
    void (*construct)(void *) = (void (*)(void *))find_function("_ZN4lang6ObjectC1Ev");
    void (*claim)(void *) = (void (*)(void *))find_function("_ZN4lang6Object5claimEv");
    uint32_t vtable[26] = {0}, context[0x2c4 / 4] = {0}, game[18] = {0};
    uint32_t batcher[55] = {0}, shaders[2][2] = {{0}};
    uint32_t vertices[18], colors[24];
    context[0] = (uint32_t)(uintptr_t)vtable;
    context[13] = (uint32_t)(uintptr_t)batcher;
    context[0x2b4 / 4] = (uint32_t)(uintptr_t)shaders[0];
    context[0x2b8 / 4] = (uint32_t)(uintptr_t)shaders[1];
    vtable[24] = (uint32_t)(uintptr_t)fill_rectangle_get_state;
    vtable[25] = (uint32_t)(uintptr_t)fill_rectangle_get_viewport;
    game[0x44 / 4] = (uint32_t)(uintptr_t)context;
    batcher[42] = (uint32_t)(uintptr_t)context;
    batcher[45] = (uint32_t)(uintptr_t)vertices;
    batcher[47] = (uint32_t)(uintptr_t)(vertices + 18);
    batcher[51] = (uint32_t)(uintptr_t)colors;
    batcher[53] = (uint32_t)(uintptr_t)(colors + 24);
    for (size_t index = 0; index < 2; ++index) {
        construct(shaders[index]);
        claim(shaders[index]);
    }
    uint32_t random = UINT32_C(0x520f1117);
    semihost(4, "AB_ANDROID_FILL_RECTANGLE_V1 4096\n");
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[8];
        input[0] = 480u << (item % 3);
        input[1] = 320u << (item % 3);
        if (item % 17 == 0) {
            input[0] = input[1] = 1;
        }
        input[2] = random_word(&random);
        if (item % 2 == 0) {
            input[2] |= UINT32_C(0xff000000);
        }
        for (size_t index = 3; index < 7; ++index) {
            int32_t extent = (int32_t)(random_word(&random) % 2049) - 1024;
            input[index] = (uint32_t)extent;
        }
        if (item % 19 == 0) {
            input[3] = random_word(&random);
            input[4] = random_word(&random);
        }
        input[7] = (uint32_t)(item % 3);
        vtable[16] = (uint32_t)(uintptr_t)fill_rectangle_noop;
        fill_rectangle_noop_calls = 0;
        reset(game, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        if (fill_rectangle_noop_calls != 1) {
            fail("Original rectangle state reset did not reach prepared fill\n");
        }
        vtable[16] = (uint32_t)(uintptr_t)fill;
        fill_rectangle_viewport[2] = input[0];
        fill_rectangle_viewport[3] = input[1];
        batcher[46] = batcher[45];
        batcher[52] = batcher[51];
        fill(context, input[2], (int32_t)input[3], (int32_t)input[4], (int32_t)input[5],
             (int32_t)input[6], input[7]);
        uint32_t drawn = batcher[46] == (uint32_t)(uintptr_t)(vertices + 18);
        uint32_t selected = 0;
        if (drawn) {
            selected = batcher[44] == (uint32_t)(uintptr_t)shaders[0] ? 1u : 2u;
            if (batcher[41] != 1 || batcher[43] ||
                batcher[44] != (uint32_t)(uintptr_t)shaders[selected - 1] ||
                batcher[52] != (uint32_t)(uintptr_t)(colors + 24)) {
                fail("Unexpected original fill batch output\n");
            }
        } else if (batcher[46] != batcher[45] || batcher[52] != batcher[51]) {
            fail("Original fill batch exceeded controlled buffers\n");
        }
        uint32_t output[] = {drawn, selected, context[(0xec + 0x40) / 4]};
        print_words(input, 8);
        print_words(output, 3);
        if (drawn) {
            print_words(vertices, 18);
            print_words(colors, 24);
        }
    }
    semihost(4, "AB fill rectangle reference complete\n");
}

#endif
