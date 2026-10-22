#ifndef AB_ORACLE_UI_TEXT_VISIBILITY_H
#define AB_ORACLE_UI_TEXT_VISIBILITY_H

static int32_t ui_text_screen_height;

static int32_t ui_text_context_height(void *context) {
    (void)context;
    return ui_text_screen_height;
}

static void probe_ui_text_visibility(void) {
    typedef struct {
        int32_t vertical, horizontal;
    } Anchor;
    int (*visible)(const void *, const void *, float, float, float, float, Anchor) =
        (int (*)(const void *, const void *, float, float, float, float, Anchor))
            find_function("_ZNK7GameLua16isStringOnScreenERKSsffffN4game6AnchorE");
    uint32_t font[18] = {0}, resources[0x2c / 4] = {0};
    uint32_t game[0x54 / 4] = {0}, context_vtable[29] = {0};
    context_vtable[28] = (uint32_t)(uintptr_t)ui_text_context_height;
    uint32_t context = (uint32_t)(uintptr_t)context_vtable;
    font[0] = (uint32_t)find_symbol("_ZTVN4game10BitmapFontE", 1) + 8;
    resources[0x28 / 4] = (uint32_t)(uintptr_t)font;
    game[0x50 / 4] = (uint32_t)(uintptr_t)resources;
    game[0x44 / 4] = (uint32_t)(uintptr_t)&context;
    uint32_t string_storage[4] = {0, 0, UINT32_MAX, 0};
    const char *text = (const char *)(string_storage + 3);
    static const uint32_t edges[] = {0,          0x80000000, 1,          0x80000001,
                                     0x3f800000, 0xbf800000, 0x43a00000, 0xc3a00000,
                                     0x7f7fffff, 0xff7fffff, 0x7f800000, 0xff800000,
                                     0x7fc00000, 0xffc00000, 0x7f800001, 0xff800001};
    uint32_t random = 0x5207c111;
    semihost(4, "AB_ANDROID_UI_TEXT_VISIBILITY_V1 8192\n");
    for (size_t test = 0; test < 8192; ++test) {
        uint32_t input[9];
        input[0] = (uint32_t)((int32_t)(random_word(&random) % 8193) - 4096);
        input[1] = (uint32_t)((int32_t)(random_word(&random) % 8193) - 4096);
        for (size_t field = 2; field < 6; ++field) {
            input[field] = test % 3 ? edges[random_word(&random) % 16]
                                    : random_float(&random, 115, 25, 1);
        }
        input[6] = (uint32_t)(test % 8);
        input[7] = random_word(&random);
        input[8] = test % 7 ? random_word(&random) % 2049 : UINT32_MAX;
        if (test % 16 == 0) {
            input[0] = input[1] = 0;
            FloatBits height = {.number = (float)(int32_t)input[8]};
            input[3] = height.bits;
            input[5] = 0x3f800000;
        } else if (test % 16 == 1) {
            input[0] = input[1] = input[3] = 0;
            input[5] = 0x3f800000;
        } else if (test % 16 == 2) {
            input[0] = 0x7fffffff;
            input[1] = 0x80000000;
        }
        font[11] = input[0];
        font[12] = input[1];
        ui_text_screen_height = (int32_t)input[8];
        FloatBits x = {.bits = input[2]}, y = {.bits = input[3]};
        FloatBits sx = {.bits = input[4]}, sy = {.bits = input[5]};
        Anchor anchor = {(int32_t)input[6], (int32_t)input[7]};
        print_words(input, 9);
        uint32_t result = (uint32_t)visible(game, &text, x.number, y.number, sx.number,
                                            sy.number, anchor);
        print_words(&result, 1);
    }
    semihost(4, "AB UI text visibility reference complete\n");
}

#endif
