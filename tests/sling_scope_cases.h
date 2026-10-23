#ifndef AB_SLING_SCOPE_CASES_H
#define AB_SLING_SCOPE_CASES_H

#include <stddef.h>
#include <stdint.h>

enum { AB_SLING_SCOPE_CASES = 256 };

typedef struct {
    float vx, vy, x, y, timer, gravity, force, mass, length, alpha;
    float tx, ty, sx, sy;
    int pivot_x, pivot_y;
    unsigned body, decay, decay_present, alpha_kind, length_kind;
} AbSlingScopeCase;

static uint32_t ab_scope_word(uint32_t *state) {
    *state = *state * UINT32_C(1664525) + UINT32_C(1013904223);
    return *state;
}

static AbSlingScopeCase ab_sling_scope_case(size_t index) {
    uint32_t random = UINT32_C(0x5205c07e) + (uint32_t)index;
    static const float forces[] = {0, 0.005f, 0.01f, 0.010001f, 12, -12, 80};
    static const float gravity[] = {20, 0, -10, 9.81f};
    static const float lengths[] = {11, 3, 1, 0.2f, 0.05f};
    static const float scales[] = {1, 0.75f, 2, 0.33333334f};
    AbSlingScopeCase value = {0};
    value.vx = (float)((int)(ab_scope_word(&random) % 2401) - 1200) / 8;
    value.vy = (float)((int)(ab_scope_word(&random) % 1601) - 800) / 8;
    value.x = (float)((int)(ab_scope_word(&random) % 161) - 80) / 4;
    value.y = (float)((int)(ab_scope_word(&random) % 81) - 40) / 4;
    value.timer = index % 5 == 0 ? 0 : (float)((int)(index % 93) - 12) / 16;
    value.gravity = gravity[index % 4];
    value.force = forces[(index / 4) % 7];
    value.mass = (float)(1 + index % 13) * 0.75f;
    value.length = lengths[(index / 7) % 5];
    value.alpha = (float)(1 + index % 12) / 10;
    value.tx = (float)((int)(ab_scope_word(&random) % 1025) - 512) / 4;
    value.ty = (float)((int)(ab_scope_word(&random) % 1025) - 512) / 4;
    value.sx = scales[(index / 3) % 4];
    value.sy = scales[(index / 5) % 4];
    value.pivot_x = (int)(index % 17) - 3;
    value.pivot_y = (int)((index / 2) % 19) - 4;
    value.body = (unsigned)(index / 9) % 2;
    value.decay = (unsigned)(index / 3) % 2;
    value.decay_present = index % 7 != 0;
    value.alpha_kind = (unsigned)(index / 11) % 3;
    value.length_kind = (unsigned)(index / 13) % 3;
    return value;
}

#endif
