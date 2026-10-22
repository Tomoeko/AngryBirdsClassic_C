#ifndef AB_REFERENCE_ANGER_H
#define AB_REFERENCE_ANGER_H

/* Authored inputs only: maximum/level/charges/queued and optional timer dates. */
static void reference_anger(unsigned profile, unsigned reload, float values[4],
                            unsigned kinds[4], float date[6], unsigned timers[2]) {
    values[0] = profile == 2 ? 0 : profile == 3 ? 3 : 2;
    values[1] = profile == 2 ? 1 : profile == 3 ? 2.5f : 0;
    values[2] = (float)(profile % 4) * 0.5f;
    values[3] = reload ? 0 : (float)profile - 3;
    for (unsigned index = 0; index < 4; ++index)
        kinds[index] = 3;
    if (profile == 1) {
        kinds[0] = kinds[3] = 1;
        values[0] = values[3] = 0;
    }
    if (reload) {
        values[0] = 2;
        values[1] = 0;
        kinds[0] = kinds[3] = 3;
    }
    const float base[] = {2026, 12, 31, 23, 59, 0};
    for (unsigned index = 0; index < 6; ++index)
        date[index] = base[index];
    date[5] -= reload ? 0 : 600;
    timers[0] = reload || (profile & 1);
    timers[1] = !reload && (profile & 2);
}

static float reference_anger_elapsed(unsigned step) {
    static const float elapsed[] = {0,   0,   0,   1800, 0,   300,  601,
                                    601, 601, 601, 601,  601, 3600, 3600};
    return elapsed[step];
}

#endif
