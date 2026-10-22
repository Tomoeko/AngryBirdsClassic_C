#ifndef AB_TEST_COMPOSITE_PART_CASES_H
#define AB_TEST_COMPOSITE_PART_CASES_H

#include <stddef.h>
#include <stdint.h>

static const char *const composite_part_sprites[] = {"sprite", "sprite#x", "a.b", ""};
static const char *const composite_part_tags[] = {"", "part", "same#part", "1"};

static uint32_t composite_part_random(uint32_t *state) {
    *state = *state * UINT32_C(1664525) + UINT32_C(1013904223);
    return *state >> 8;
}

static void composite_part_case(uint32_t index, uint32_t input[15]) {
    uint32_t state = UINT32_C(0x520c0a91) ^ index;
    input[0] = index & 1u;
    input[1] = (index >> 1) & 3u;
    input[2] = (index >> 3) & 3u;
    input[3] = (index >> 5) & 3u;
    input[4] = composite_part_random(&state) % 128 + 1;
    input[5] = composite_part_random(&state) % 128 + 1;
    for (size_t field = 6; field < 10; ++field) {
        input[field] = (uint32_t)((int32_t)(composite_part_random(&state) % 129) - 64);
    }
    for (size_t field = 10; field < 13; ++field) {
        union {
            float number;
            uint32_t bits;
        } value = {.number = (float)((int32_t)(composite_part_random(&state) %
                                               (field == 12 ? 23041u : 257u)) -
                                     (field == 12 ? 11520 : 128)) /
                             16.0f};
        input[field] = value.bits;
    }
    input[13] = index & 128u ? composite_part_random(&state) & 255u : 0;
    input[14] = index & 256u ? composite_part_random(&state) & 255u : 0;
    if (index % 17 == 0) {
        input[10] = input[11] = UINT32_C(0x80000000);
        input[12] = UINT32_C(0x80000000);
    }
}

static void composite_part_put16(uint8_t *bytes, uint16_t value) {
    bytes[0] = (uint8_t)(value >> 8);
    bytes[1] = (uint8_t)value;
}

static void composite_part_put32(uint8_t *bytes, uint32_t value) {
    for (unsigned index = 0; index < 4; ++index) {
        bytes[index] = (uint8_t)(value >> (24 - index * 8));
    }
}

static size_t composite_part_put_string(uint8_t *bytes, const char *value) {
    size_t count = 0;
    while (value[count]) {
        bytes[count + 2] = (uint8_t)value[count];
        ++count;
    }
    composite_part_put16(bytes, (uint16_t)count);
    return count + 2;
}

/* Authored single-part metadata, encoded through the target's documented
 * container/record fields. Shared inputs are not an expected-behavior model. */
static size_t composite_part_data(const uint32_t input[15], uint8_t bytes[256]) {
    const char *magic = input[0] ? "RVIO" : "KA3D";
    for (size_t index = 0; index < 4; ++index) {
        bytes[index] = (uint8_t)magic[index];
        bytes[index + 8] = (uint8_t)"COMP"[index];
    }
    composite_part_put16(bytes + 16, 1);
    composite_part_put16(bytes + 18, 1);
    size_t offset = 20 + composite_part_put_string(bytes + 20, "composite");
    composite_part_put16(bytes + offset, 1);
    offset += 2;
    offset +=
        composite_part_put_string(bytes + offset, composite_part_sprites[input[2]]);
    if (input[0]) {
        offset +=
            composite_part_put_string(bytes + offset, composite_part_tags[input[3]]);
    }
    composite_part_put16(bytes + offset, (uint16_t)input[8]);
    composite_part_put16(bytes + offset + 2, (uint16_t)input[9]);
    offset += 4;
    if (input[0]) {
        for (size_t field = 0; field < 3; ++field) {
            composite_part_put32(bytes + offset, input[10 + field]);
            offset += 4;
        }
        bytes[offset++] = (uint8_t)input[13];
        bytes[offset++] = (uint8_t)input[14];
    }
    composite_part_put32(bytes + 4, (uint32_t)offset - 8);
    composite_part_put32(bytes + 12, (uint32_t)offset - 16);
    return offset;
}

#endif
