#ifndef AB_TEST_SCENE_ID_CASES_H
#define AB_TEST_SCENE_ID_CASES_H

#include <stdint.h>

static inline uint32_t scene_id_case(uint32_t sample) {
    const uint32_t edges[] = {0,
                              1,
                              2,
                              UINT32_C(0xfffffffe),
                              UINT32_MAX,
                              UINT32_C(0x7ffffffe),
                              UINT32_C(0x7fffffff),
                              UINT32_C(0x80000000),
                              UINT32_C(0x00fffffe),
                              UINT32_C(0x00ffffff),
                              UINT32_C(0x01000000),
                              UINT32_C(0x01000001),
                              UINT32_C(0x01000002),
                              UINT32_C(0xfeffffff),
                              UINT32_C(0xff000000)};
    if (sample < sizeof(edges) / sizeof(*edges))
        return edges[sample];
    uint32_t value = sample ^ UINT32_C(0x5201dc00);
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    return value;
}

#endif
