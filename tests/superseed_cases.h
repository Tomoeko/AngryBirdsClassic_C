#ifndef AB_SUPERSEED_CASES_H
#define AB_SUPERSEED_CASES_H

#include <stddef.h>
#include <stdint.h>

enum { AB_SUPERSEED_CASES = 2048 };

typedef struct {
    float base, target, timer;
    unsigned timer_present;
} AbSuperseedCase;

static AbSuperseedCase ab_superseed_case(size_t index) {
    static const float radii[] = {0.6f, 0.8f, 0.85f, 1, 1.15f, 1.5f, 2.25f, 0.125f};
    static const float factors[] = {1.4f, 1.5f, 1.6f, 2};
    AbSuperseedCase value = {.base = radii[index % 8],
                             .timer = (float)((int)(index % 131) - 5) / 240,
                             .timer_present = index % 13 != 0};
    value.target = value.base * factors[(index / 8) % 4];
    switch (index % 17) {
        case 0:
            value.timer = 0;
            break;
        case 1:
            value.timer = 0.5f;
            break;
        case 2:
            value.timer = 0x1.fffffep-2f;
            break;
        case 3:
            value.timer = 0x1.000002p-1f;
            break;
        default:
            break;
    }
    return value;
}

#endif
