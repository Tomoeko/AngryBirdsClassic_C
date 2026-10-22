#ifndef AB_TEST_OBJECT_DEFINITION_LOOKUP_CASES_H
#define AB_TEST_OBJECT_DEFINITION_LOOKUP_CASES_H

#include <stdint.h>

enum {
    DEFINITION_LOOKUP_CASES = 4096,
    DEFINITION_LOOKUP_INPUT_WORDS = 13,
    DEFINITION_LOOKUP_OUTPUT_WORDS = 6,
    DEFINITION_LOOKUP_EVENT_WORDS = 12,
    DEFINITION_LOOKUP_TABLES = 7,
};

typedef struct {
    const char *data;
    uint32_t size;
} DefinitionLookupName;

static DefinitionLookupName definition_lookup_name(uint32_t index) {
    static const DefinitionLookupName names[] = {
        {"", 0},       {"bird", 4},    {"A\0B", 3}, {"\xc3\xa9\xe9\xb3\xa5", 5},
        {"ground", 6}, {"stone_1", 7}, {"same", 4}, {"missing", 7}};
    return names[index % 8u];
}

/* Kinds: nil, false, true, number, string, table. Most cases keep enclosing
 * tables valid; rotate one malformed container to retain both success/error
 * coverage. Fields: case; objects/world/object/blockTable/blocks kinds;
 * definition-key kind; result kind; name; diagnostic flags; key/result bits;
 * stored-key mode (exact, different string, opposite zero sign, different table).
 * Flags: unavailable print, rejection, objects rebinding, non-callable Boolean rather
 * than nil print. */
static void object_definition_lookup_case(uint32_t item, uint32_t output[13]) {
    static const uint32_t numbers[] = {0,
                                       UINT32_C(0x80000000),
                                       UINT32_C(0x3f800000),
                                       UINT32_C(0xbf800000),
                                       UINT32_C(0x7f800000),
                                       UINT32_C(0xff800000),
                                       UINT32_C(0x7fc12345),
                                       UINT32_C(0x7f812345),
                                       1,
                                       UINT32_C(0x80000001)};
    output[0] = item;
    output[1] = output[2] = output[4] = output[5] = 5;
    output[3] = item % 5u ? 5u : item / 5u % 6u;
    if (item % 4u == 0) {
        static const uint32_t containers[] = {1, 2, 4, 5};
        output[containers[item / 4u % 4u]] = item / 16u % 6u;
    }
    output[6] = item / 3u % 6u;
    output[7] = item / 7u % 6u;
    output[8] = item % 8u;
    output[9] = item / 4u % 16u;
    output[10] = numbers[item / 11u % 10u];
    output[11] = numbers[item / 13u % 10u];
    output[12] = item % 7u == 0;
    if (item >= DEFINITION_LOOKUP_CASES - 64u) {
        output[1] = output[2] = output[3] = output[4] = output[5] = 5;
        output[6] = item % 4u < 2u ? 3u : 5u;
        output[7] = 3;
        output[9] = 0;
        output[10] = item % 2u ? UINT32_C(0x80000000) : 0;
        output[11] = UINT32_C(0x42280000);
        output[12] = item % 4u < 2u ? 2u : item % 4u == 2u ? 0u : 3u;
    }
}

static inline int definition_lookup_invalid_key(uint32_t kind, uint32_t bits) {
    return kind == 0 ||
           (kind == 3 && (bits & UINT32_C(0x7fffffff)) > UINT32_C(0x7f800000));
}

static void definition_lookup_message(DefinitionLookupName name, uint32_t output[8]) {
    const char prefix[] = "Object not found:";
    uint32_t length = (uint32_t)sizeof(prefix) - 1u + name.size;
    for (uint32_t index = 0; index < 8; ++index)
        output[index] = 0;
    output[0] = length;
    for (uint32_t index = 0; index < length; ++index) {
        uint8_t byte = (uint8_t)(index < sizeof(prefix) - 1u
                                     ? prefix[index]
                                     : name.data[index - (sizeof(prefix) - 1u)]);
        output[1 + index / 4u] |= (uint32_t)byte << (index % 4u * 8u);
    }
}

#endif
