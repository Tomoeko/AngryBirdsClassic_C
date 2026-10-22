#ifndef AB_TEST_SCENE_MEMBERSHIP_CASES_H
#define AB_TEST_SCENE_MEMBERSHIP_CASES_H

#include <stdint.h>

typedef struct {
    uint32_t kind, object;
    int32_t id;
    float number;
} SceneMembershipOperation;

static inline uint32_t scene_membership_random(uint32_t value) {
    value ^= UINT32_C(0x52031c48);
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    return value;
}

static inline int32_t scene_membership_id(uint32_t sample, uint32_t object) {
    const int32_t ids[] = {0,        1,        -1,        16777216,
                           16777217, 16777218, INT32_MAX, INT32_MIN};
    uint32_t index = sample % 3 ? object + sample : object % 3;
    return ids[index % 8];
}

static inline int32_t scene_membership_layer(uint32_t sample, uint32_t object) {
    const int32_t layers[] = {-3, 0, 2, 4};
    return layers[scene_membership_random(sample * 17 + object) % 4];
}

static inline SceneMembershipOperation scene_membership_operation(uint32_t sample,
                                                                  uint32_t step) {
    uint32_t random = scene_membership_random(sample * 48 + step);
    SceneMembershipOperation operation = {.kind = random % 4,
                                          .object = (random >> 8) % 8};
    if (step < 16) {
        operation.kind = 0;
        operation.object = step % 8;
    } else if (step < 24) {
        const uint32_t kinds[] = {1, 2, 3, 2, 3, 3, 3, 1};
        operation.kind = kinds[step - 16];
        operation.object = step == 19 ? 8 : sample % 8;
    }
    operation.id = scene_membership_id(sample, operation.object);
    operation.number = (float)operation.id;
    if (operation.kind == 3 && step >= 24)
        operation.number += random & 16 ? 0.75f : -0.75f;
    union {
        uint32_t bits;
        float number;
    } special = {0};
    if (step >= 20 && step <= 22) {
        const uint32_t bits[] = {UINT32_C(0x7fc12345), UINT32_C(0x7f800000),
                                 UINT32_C(0xff800000)};
        special.bits = bits[step - 20];
        operation.number = special.number;
    }
    if (step == 23)
        operation.id = 123456;
    return operation;
}

#endif
