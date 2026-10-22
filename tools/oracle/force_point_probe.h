#ifndef AB_ORACLE_FORCE_POINT_H
#define AB_ORACLE_FORCE_POINT_H

static void probe_force_point(void) {
    typedef void *(*GetBody)(void *, const char **);
    typedef void (*ApplyForce)(void *, const char **, float, float, float, float);
    GetBody get_body = (GetBody)find_function("_ZN7GameLua7getBodyERKSs");
    ApplyForce apply = (ApplyForce)find_function("_ZN7GameLua10applyForceESsffff");
    static uint32_t game[0x1800 / 4], object[50], body[38], node[6];
    static const struct {
        uint32_t length, capacity, references;
        char bytes[8];
    } name = {5, 5, UINT32_MAX, "first"};
    const char *identifier = name.bytes;
    node[0] = 1;
    node[1] = (uint32_t)(uintptr_t)((uint8_t *)game + 0x17a8);
    node[4] = (uint32_t)(uintptr_t)name.bytes;
    node[5] = (uint32_t)(uintptr_t)object;
    game[0x17ac / 4] = game[0x17b0 / 4] = game[0x17b4 / 4] = (uint32_t)(uintptr_t)node;
    game[0x17b8 / 4] = 1;
    object[11] = (uint32_t)(uintptr_t)body;
    if (get_body(game, &identifier) != body) {
        fail("Controlled direct-force body lookup failed\n");
    }
    uint32_t random = UINT32_C(0x520f0a71);
    semihost(4, "AB_ANDROID_FORCE_POINT_V1 4096\n");
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[12] = {(uint32_t)(item % 3), (uint32_t)((item / 3) & 1)};
        for (size_t field = 2; field < 12; ++field) {
            input[field] = random_float(&random, 112, 30, 1);
            if (item < 256) {
                input[field] = item & (1u << (field % 8)) ? UINT32_C(0x80000000) : 0;
            }
        }
        for (size_t field = 0; field < 38; ++field) {
            body[field] = 0;
        }
        body[0] = input[0];
        body[1] = 36 | (input[1] ? 2u : 0u);
        body[36] = input[2];
        body[11] = input[3];
        body[12] = input[4];
        body[19] = input[5];
        body[20] = input[6];
        body[21] = input[7];
        uint32_t original[38];
        for (size_t field = 0; field < 38; ++field) {
            original[field] = body[field];
        }
        FloatBits fx = {.bits = input[8]}, fy = {.bits = input[9]},
                  x = {.bits = input[10]}, y = {.bits = input[11]};
        apply(game, &identifier, fx.number, fy.number, x.number, y.number);
        for (size_t field = 0; field < 38; ++field) {
            if (field != 1 && field != 19 && field != 20 && field != 21 &&
                field != 36 && original[field] != body[field]) {
                fail("Unexpected direct force body mutation\n");
            }
        }
        uint32_t output[] = {body[1], body[19], body[20], body[21], body[36]};
        print_words(input, 12);
        print_words(output, 5);
    }
    semihost(4, "AB force point reference complete\n");
}

#endif
