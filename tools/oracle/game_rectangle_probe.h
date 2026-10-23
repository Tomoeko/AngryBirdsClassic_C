#ifndef AB_ORACLE_GAME_RECTANGLE_H
#define AB_ORACLE_GAME_RECTANGLE_H

static uint32_t game_rectangle_state[39], game_rectangle_result[8];
static uint32_t game_rectangle_queries, game_rectangle_calls;

static void *game_rectangle_get_state(void *context) {
    (void)context;
    ++game_rectangle_queries;
    return game_rectangle_state;
}

static int game_rectangle_fill(void *context, uint32_t color, int32_t left, int32_t top,
                               int32_t width, int32_t height, uint32_t blend) {
    (void)context;
    if (++game_rectangle_calls != 1 || blend) {
        fail("Unexpected native game rectangle boundary\n");
    }
    game_rectangle_result[0] = color;
    game_rectangle_result[1] = (uint32_t)left;
    game_rectangle_result[2] = (uint32_t)top;
    game_rectangle_result[3] = (uint32_t)width;
    game_rectangle_result[4] = (uint32_t)height;
    game_rectangle_result[5] = blend == 0;
    game_rectangle_result[6] = game_rectangle_queries;
    return 0;
}

#if defined(AB_ORACLE_GAME_RECTANGLE_ONLY)
static void probe_game_rectangle(void) {
    typedef void (*Draw)(void *, float, float, float, float, float, float, float, float,
                         int);
    Draw draw = (Draw)find_function("_ZN7GameLua8drawRectEffffffffb");
    uint32_t game[18] = {0}, vtable[25] = {0};
    uint32_t context[] = {(uint32_t)(uintptr_t)vtable};
    game[0x44 / 4] = (uint32_t)(uintptr_t)context;
    vtable[16] = (uint32_t)(uintptr_t)game_rectangle_fill;
    vtable[24] = (uint32_t)(uintptr_t)game_rectangle_get_state;
    uint32_t random = UINT32_C(0x5207ec71);
    semihost(4, "AB_ANDROID_GAME_RECTANGLE_V1 8192\n");
    for (size_t item = 0; item < 8192; ++item) {
        uint32_t input[9];
        FloatBits values[8];
        for (size_t index = 0; index < 8; ++index) {
            values[index].number =
                (float)((int32_t)(random_word(&random) % 8193) - 4096) / 16;
            if (index < 4) {
                values[index].number /= 64;
            }
            if ((item + index) % 13 == 0) {
                values[index].number = (float)((int32_t)(item % 33) - 16) + 0.5f;
            }
            input[index] = values[index].bits;
        }
        input[8] = (uint32_t)(item % 2);
        game_rectangle_queries = game_rectangle_calls = 0;
        draw(game, values[0].number, values[1].number, values[2].number,
             values[3].number, values[4].number, values[5].number, values[6].number,
             values[7].number, (int)input[8]);
        if (game_rectangle_calls != 1) {
            fail("Original game rectangle did not call fill provider\n");
        }
        game_rectangle_result[7] = game_rectangle_queries;
        print_words(input, 9);
        print_words(game_rectangle_result, 8);
    }
    semihost(4, "AB game rectangle reference complete\n");
}
#endif

#endif
