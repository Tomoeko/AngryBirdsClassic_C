#ifndef AB_ORACLE_MP3_GAIN_H
#define AB_ORACLE_MP3_GAIN_H

static void probe_mp3_gain(void) {
    /* This original initializer ignores its first argument and returns the
     * binary32 conversion of pow(2, -(index + 210)/4) in ARM r0. */
    float (*gain)(void *, int) =
        (float (*)(void *, int))find_function("INT123_init_layer3_gainpow2");
    semihost(4, "AB_ANDROID_MP3_GAIN_V1 378\n");
    for (int quarter_steps = -46; quarter_steps <= 331; ++quarter_steps) {
        union {
            float number;
            uint32_t bits;
        } value = {.number = gain(NULL, quarter_steps - 210)};
        uint32_t words[] = {(uint32_t)quarter_steps, value.bits};
        print_words(words, 2);
    }
    semihost(4, "AB MP3 gain reference complete\n");
}

#endif
