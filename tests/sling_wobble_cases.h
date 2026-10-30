#ifndef AB_TEST_SLING_WOBBLE_CASES_H
#define AB_TEST_SLING_WOBBLE_CASES_H

enum { SLING_WOBBLE_CASES = 64 };

static void sling_wobble_input(unsigned sample, float output[3]) {
    static const float seconds[] = {0,     -0.1f,        0.001f, 0x1.111112p-6f,
                                    0.05f, 0.050000004f, 0.1f,   0.25f};
    output[0] = (float)((int)(sample % 9) - 4) * 1.25f;
    output[1] = (float)((int)(sample / 9) - 3) * 0.75f;
    output[2] = seconds[sample % 8];
}

#endif
