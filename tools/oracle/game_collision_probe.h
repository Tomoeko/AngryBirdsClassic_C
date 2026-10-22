#ifndef AB_ORACLE_GAME_COLLISION_PROBE_H
#define AB_ORACLE_GAME_COLLISION_PROBE_H

static void probe_game_collision(void) {
    typedef unsigned (*GameFilter)(void *, const void *, const void *);
    GameFilter filter =
        (GameFilter)find_function("_ZN7GameLua13ShouldCollideEP9b2FixtureS1_");
    uint32_t state = UINT32_C(0x392acb61);
    static uint8_t game[0x1784];
    const uint32_t groups[] = {0, 1, 0xffff, 2, 0xfffe, 0x7fff, 0x8000};
    semihost(4, "AB_ANDROID_GAME_COLLISION_V1 4096\n");
    for (size_t index = 0; index < 4096; ++index) {
        uint32_t input[24], fixtures[2][11] = {0}, objects[2][44] = {0};
        for (size_t endpoint = 0; endpoint < 2; ++endpoint) {
            input[endpoint * 3] = random_word(&state) & 0xffff;
            input[endpoint * 3 + 1] = random_word(&state) & 0xffff;
            input[endpoint * 3 + 2] = groups[(index + endpoint * (index % 3)) % 7];
            if (index % 8 == 0) {
                input[endpoint * 3] = 0;
                input[endpoint * 3 + 1] = 0;
            }
            fixtures[endpoint][8] =
                input[endpoint * 3] | (input[endpoint * 3 + 1] << 16);
            fixtures[endpoint][9] = input[endpoint * 3 + 2];
            input[6 + endpoint] = (uint32_t)(index / (endpoint ? 12 : 1) % 12);
            input[8 + endpoint] = random_word(&state) % 4 != 0;
            input[10 + endpoint] = (index + endpoint) % 5 != 0;
            objects[endpoint][0] = input[6 + endpoint];
            ((uint8_t *)objects[endpoint])[0xa2] = (uint8_t)input[8 + endpoint];
            fixtures[endpoint][10] =
                input[10 + endpoint] ? (uint32_t)(uintptr_t)objects[endpoint] : 0;
        }
        for (size_t first = 0; first < 12; ++first) {
            input[12 + first] = 0;
            for (size_t second = 0; second < 12; ++second) {
                uint8_t allowed = (uint8_t)(random_word(&state) % 3 != 0);
                game[0x104 + first * 0x1e0 + second * 0x28] = allowed;
                input[12 + first] |= (uint32_t)allowed << second;
            }
        }
        print_words(input, 24);
        uint32_t allowed = filter(game, fixtures[0], fixtures[1]) ? 1u : 0u;
        print_words(&allowed, 1);
    }
    semihost(4, "AB game collision reference complete\n");
}

#endif
