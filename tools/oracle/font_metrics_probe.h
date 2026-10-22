#ifndef AB_ORACLE_FONT_METRICS_H
#define AB_ORACLE_FONT_METRICS_H

static uint32_t metric_nodes[7][6], metric_sprites[7][16];
static const uint32_t metric_codes[] = {32, 48, 65, 66, 97, 0x4e00, 0x1f600};

static uint32_t metric_tree(size_t begin, size_t end, uint32_t parent) {
    if (begin == end) {
        return 0;
    }
    size_t middle = begin + (end - begin) / 2;
    uint32_t address = (uint32_t)(uintptr_t)metric_nodes[middle];
    metric_nodes[middle][0] = 1;
    metric_nodes[middle][1] = parent;
    metric_nodes[middle][2] = metric_tree(begin, middle, address);
    metric_nodes[middle][3] = metric_tree(middle + 1, end, address);
    metric_nodes[middle][4] = metric_codes[middle];
    metric_nodes[middle][5] = (uint32_t)(uintptr_t)metric_sprites[middle];
    return address;
}

#if !defined(AB_ORACLE_FONT_LAYOUT_ONLY)
static void probe_font_metrics(void) {
    typedef int32_t (*RangeMetric)(const void *, const void *, int32_t, int32_t);
    typedef int32_t (*FontHeight)(const void *);
    RangeMetric width = (RangeMetric)find_function(
        "_ZNK4game10BitmapFont14getStringWidthERKSbIDiSt11char_traitsIDiESaIDiEEii");
    RangeMetric height = (RangeMetric)find_function(
        "_ZNK4game10BitmapFont15getStringHeightERKSbIDiSt11char_traitsIDiESaIDiEEii");
    FontHeight font_height =
        (FontHeight)find_function("_ZNK4game10BitmapFont9getHeightEv");
    uint32_t *vtable = (uint32_t *)find_symbol("_ZTVN4game10BitmapFontE", 1) + 2;
    if (vtable[8] != (uint32_t)(uintptr_t)font_height ||
        vtable[11] != (uint32_t)find_function("_ZNK4game10BitmapFont10getLeadingEv")) {
        fail("Unexpected native bitmap-font virtual slots\n");
    }
    uint32_t font[18] = {0};
    font[0] = (uint32_t)(uintptr_t)vtable;
    uint32_t text_storage[20] = {0};
    uint32_t *text = text_storage + 3;
    uint32_t random = UINT32_C(0x520f04e7);
    semihost(4, "AB_ANDROID_FONT_METRICS_V1 4096\n");
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[31];
        input[0] = (uint32_t)item % 8;
        int32_t tracking = (int32_t)(random_word(&random) % 65) - 32;
        int32_t ascending = (int32_t)(random_word(&random) % 81) - 40;
        int32_t descending = (int32_t)(random_word(&random) % 81) - 40;
        input[1] = (uint32_t)tracking;
        input[2] = (uint32_t)ascending;
        input[3] = (uint32_t)descending;
        input[4] = (uint32_t)((int32_t)(random_word(&random) % 241) - 120);
        input[5] = (uint32_t)(item / 8) % 17;
        input[6] = random_word(&random) % 20;
        input[7] = random_word(&random) % 24;
        for (size_t glyph = 0; glyph < 7; ++glyph) {
            input[8 + glyph] = random_word(&random);
            metric_sprites[glyph][5] = input[8 + glyph];
        }
        for (size_t character = 0; character < 16; ++character) {
            uint32_t selected = random_word(&random) % 10;
            input[15 + character] = selected < 7    ? metric_codes[selected]
                                    : selected == 7 ? 10
                                    : selected == 8 ? 0
                                                    : UINT32_C(0x10ffff);
            text[character] = input[15 + character];
        }
        text_storage[0] = text_storage[1] = input[5];
        text_storage[2] = 0;
        font[4] = 0;
        font[5] = metric_tree(0, input[0], (uint32_t)(uintptr_t)(font + 4));
        font[6] = input[0] ? (uint32_t)(uintptr_t)metric_nodes[0]
                           : (uint32_t)(uintptr_t)(font + 4);
        font[7] = input[0] ? (uint32_t)(uintptr_t)metric_nodes[input[0] - 1]
                           : (uint32_t)(uintptr_t)(font + 4);
        font[8] = input[0];
        font[10] = input[1];
        font[11] = input[2];
        font[12] = input[3];
        font[13] = input[4];
        print_words(input, 31);
        uint32_t output[3] = {
            (uint32_t)width(font, &text, (int32_t)input[6], (int32_t)input[7]),
            (uint32_t)height(font, &text, (int32_t)input[6], (int32_t)input[7]),
            (uint32_t)font_height(font)};
        print_words(output, 3);
    }
    semihost(4, "AB font metrics reference complete\n");
}
#endif

#endif
