#ifndef AB_ORACLE_GAME_ANGLE_H
#define AB_ORACLE_GAME_ANGLE_H

extern float fmodf(float, float);

static void probe_game_angles(void) {
    static const uint32_t edges[] = {
        0,          0x80000000, 1,          0x80000001, 0x007fffff, 0x807fffff,
        0x00800000, 0x80800000, 0x3f800000, 0xbf800000, 0x40490fdb, 0xc0490fdb,
        0x40c90fda, 0xc0c90fda, 0x40c90fdb, 0xc0c90fdb, 0x40c90fdc, 0xc0c90fdc,
        0x41490fdb, 0xc1490fdb, 0x7f7fffff, 0xff7fffff,
    };
    uint32_t state = UINT32_C(0x520a991e);
    semihost(4, "AB_ANDROID_GAME_ANGLE_V1 65536\n");
    for (size_t index = 0; index < 65536; ++index) {
        FloatBits input = {.bits = index < sizeof(edges) / sizeof(*edges)
                                       ? edges[index]
                                       : random_word(&state)};
        if ((input.bits & UINT32_C(0x7f800000)) == UINT32_C(0x7f800000)) {
            input.bits &= ~UINT32_C(0x00800000);
        }
        float pi = 0x1.921fb6p+1f;
        float period = pi + pi;
        FloatBits output = {.number = fmodf(input.number, period)};
        if (output.number < 0) {
            output.number = output.number + period;
        }
        const uint32_t words[] = {input.bits, output.bits};
        print_words(words, 2);
    }
    semihost(4, "AB game angle reference complete\n");
}

#endif
