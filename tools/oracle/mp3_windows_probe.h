#ifndef AB_ORACLE_MP3_WINDOWS_H
#define AB_ORACLE_MP3_WINDOWS_H

static void probe_mp3_windows(void) {
    int (*initialize)(void) = (int (*)(void))find_function("mpg123_init");
    if (initialize() != 0) {
        fail("Cannot initialize the original MP3 window tables\n");
    }
    uintptr_t function = find_function("INT123_do_layer3");
    /* The original long-transform caller loads its shared constant base with
     * an LDR literal at ELF 0x8fd074 and ADD PC at ELF 0x8fd548. */
    uint32_t displacement =
        *(const uint32_t *)(function + (UINT32_C(0x8fd074) - UINT32_C(0x8fc60c)));
    uintptr_t base =
        function + (UINT32_C(0x8fd550) - UINT32_C(0x8fc60c)) + displacement;
    semihost(4, "AB_ANDROID_MP3_WINDOWS_V1 288\n");
    for (size_t parity = 0; parity < 2; ++parity) {
        uintptr_t window_base = base + (parity ? 1760u : 392u);
        for (size_t block = 0; block < 4; ++block) {
            uint32_t meta[] = {(uint32_t)parity, (uint32_t)block};
            const uint32_t *window = (const uint32_t *)(window_base + block * 144);
            print_words(meta, 2);
            print_words(window, 36);
        }
    }
    semihost(4, "AB MP3 windows reference complete\n");
}

#endif
