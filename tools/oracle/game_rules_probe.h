#ifndef AB_ORACLE_GAME_RULES_PROBE_H
#define AB_ORACLE_GAME_RULES_PROBE_H

static void print_game_rules(const uint8_t *game) {
    uint32_t rows[12] = {0};
    for (size_t first = 0; first < 12; ++first) {
        for (size_t second = 0; second < 12; ++second) {
            rows[first] |= (uint32_t)game[0x104 + first * 0x1e0 + second * 0x28]
                           << second;
        }
    }
    print_words(rows, 12);
}

static void probe_game_rules(void) {
    typedef void (*Setup)(void *);
    typedef void (*SetPair)(void *, int, int, unsigned);
    Setup setup = (Setup)find_function("_ZN7GameLua19setupCollisionTableEv");
    SetPair set_pair =
        (SetPair)find_function("_ZN7GameLua14setDoesCollideE12ColliderTypeS0_b");
    static uint8_t game[0x1784];
    uint32_t state = UINT32_C(0xc12a6e03);
    semihost(4, "AB_ANDROID_GAME_RULES_V1 4096\n");
    for (size_t index = 0; index < 4096; ++index) {
        uint32_t input[15];
        for (size_t first = 0; first < 12; ++first) {
            input[first] = index ? random_word(&state) & 0xfff : 0xfff;
            for (size_t second = 0; second < 12; ++second) {
                game[0x104 + first * 0x1e0 + second * 0x28] =
                    (uint8_t)((input[first] >> second) & 1);
            }
        }
        input[12] = (uint32_t)(index % 12);
        input[13] = (uint32_t)((index / 12) % 12);
        input[14] = random_word(&state) & 1;
        print_words(input, 15);
        setup(game);
        print_game_rules(game);
        set_pair(game, (int)input[12], (int)input[13], input[14]);
        print_game_rules(game);
    }
    semihost(4, "AB game rules reference complete\n");
}

#endif
