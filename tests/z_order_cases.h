#ifndef AB_Z_ORDER_CASES_H
#define AB_Z_ORDER_CASES_H

#include <stdint.h>

enum { AB_Z_ORDER_CASES = 2048, AB_Z_ORDER_MAXIMUM = 128 };

static uint32_t z_order_case(uint32_t sample, uint32_t keys[AB_Z_ORDER_MAXIMUM]) {
    uint32_t count = sample % (AB_Z_ORDER_MAXIMUM + 1);
    uint32_t state = sample + UINT32_C(0x12520f89);
    for (uint32_t index = 0; index < count; ++index) {
        state = state * UINT32_C(1664525) + UINT32_C(1013904223);
        float z;
        switch ((sample / (AB_Z_ORDER_MAXIMUM + 1)) % 8) {
            case 0:
                z = 1;
                break;
            case 1:
                z = (float)index;
                break;
            case 2:
                z = (float)(count - index);
                break;
            case 3:
                z = (float)(index % 3);
                break;
            case 4:
                z = (float)((int32_t)(state % 7) - 3);
                break;
            case 5:
                z = (float)((int32_t)(state % 129) - 64) / 8;
                break;
            case 6:
                z = index & 1 ? -0.0f : 0.0f;
                break;
            default:
                z = (float)((int32_t)(state % 1024) - 512);
                break;
        }
        union {
            float real;
            uint32_t bits;
        } value = {.real = z};
        keys[index] = value.bits;
    }
    return count;
}

#endif
