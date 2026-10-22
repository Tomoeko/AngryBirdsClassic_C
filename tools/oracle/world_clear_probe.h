#ifndef AB_ORACLE_WORLD_CLEAR_H
#define AB_ORACLE_WORLD_CLEAR_H

/* Only ClearForces is invoked on these bounded raw ABI records. Its body-list
 * links and three force words are independent of constructors and fixtures. */
static void probe_world_clear(void) {
    typedef void (*ClearForces)(void *);
    ClearForces clear = (ClearForces)find_function("_ZN7b2World11ClearForcesEv");
    static uint32_t world[0x1922c / 4], bodies[4][38], before[4][38];
    static const uint32_t special[] = {
        0,
        UINT32_C(0x80000000),
        UINT32_C(0x7f800000),
        UINT32_C(0xff800000),
        UINT32_C(0x7fc12345),
        1,
        UINT32_C(0x80000001),
        UINT32_C(0x7f7fffff),
    };
    uint32_t state = UINT32_C(0x520cf04c);
    semihost(4, "AB_ANDROID_WORLD_CLEAR_V1 4096\n");
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[17] = {(uint32_t)(item & 15), 0, 1, 2, 3};
        for (size_t count = 4; count > 1; --count) {
            size_t swap = random_word(&state) % count;
            uint32_t saved = input[count];
            input[count] = input[swap + 1];
            input[swap + 1] = saved;
        }
        uint32_t next = 0;
        for (size_t cursor = 4; cursor > 0;) {
            size_t body = input[--cursor + 1];
            for (size_t field = 0; field < 38; ++field) {
                bodies[body][field] = random_word(&state);
            }
            for (size_t field = 0; field < 3; ++field) {
                uint32_t value =
                    item < 128 ? special[(item / 16 + field) % 8] : random_word(&state);
                input[5 + body * 3 + field] = value;
                bodies[body][19 + field] = value;
            }
            bodies[body][24] = next;
            if (input[0] & (1u << body)) {
                next = (uint32_t)(uintptr_t)bodies[body];
            }
            for (size_t field = 0; field < 38; ++field) {
                before[body][field] = bodies[body][field];
            }
        }
        world[0x19228 / 4] = next;
        print_words(input, 17);
        clear(world);
        uint32_t output[12];
        for (size_t body = 0; body < 4; ++body) {
            for (size_t field = 0; field < 38; ++field) {
                if ((field < 19 || field > 21) &&
                    bodies[body][field] != before[body][field]) {
                    fail("Native force clear changed unrelated body state\n");
                }
            }
            for (size_t field = 0; field < 3; ++field) {
                output[body * 3 + field] = bodies[body][19 + field];
            }
        }
        print_words(output, 12);
    }
    semihost(4, "AB world clear reference complete\n");
}

#endif
