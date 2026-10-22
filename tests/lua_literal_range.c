#include "ab/lua_data.h"

#include <stdio.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Literal range check failed at %d: %s\n", __LINE__,        \
                    #condition);                                                       \
            return 1;                                                                  \
        }                                                                              \
    } while (0)

static void word(uint8_t *bytes, uint32_t value) {
    for (unsigned index = 0; index < 4; ++index)
        bytes[index] = (uint8_t)(value >> (index * 8));
}

int main(void) {
    uint8_t bytes[24];
    word(bytes, 36);    /* Closure outside the literal window. */
    word(bytes + 4, 5); /* Global access outside the literal window. */
    word(bytes + 8, 10 | (1u << 6) | (1u << 14));
    word(bytes + 12, 1 | (2u << 6));
    word(bytes + 16, 9 | (1u << 6) | (257u << 23) | (2u << 14));
    word(bytes + 20, 28 | (1u << 23) | (1u << 14));
    AbLuaConstant constants[] = {
        {.kind = AB_LUA_NUMBER, .bits = UINT32_C(0x42280000)},
        {.kind = AB_LUA_STRING, .string = {(const uint8_t *)"answer", 6}}};
    AbLuaPrototype child = {0};
    AbLuaChunk chunk = {.input = {bytes, sizeof(bytes)},
                        .root = {.instructions = {bytes, sizeof(bytes)},
                                 .stack_size = 3,
                                 .constants = constants,
                                 .constant_count = 2,
                                 .children = &child,
                                 .child_count = 1}};
    AbLuaData data = {0};
    CHECK(ab_lua_data_literal_table_range(&chunk, 2, 3, 1, 4096, &data, NULL));
    const AbDataValue *answer =
        ab_data_get(data.returned.table, (AbDataValue){.kind = AB_DATA_STRING,
                                                       .string = constants[1].string});
    CHECK(answer && answer->kind == AB_DATA_NUMBER &&
          answer->bits == constants[0].bits);
    ab_lua_data_close(&data);
    CHECK(!ab_lua_data_literal_table(&chunk, 5, 1, 4096, &data, NULL) && !data.owned);
    CHECK(!ab_lua_data_literal_table_range(&chunk, 1, 4, 1, 4096, &data, NULL) &&
          !data.owned);
    CHECK(!ab_lua_data_literal_table_range(&chunk, 2, 4, 1, 4096, &data, NULL) &&
          !data.owned);
    CHECK(!ab_lua_data_literal_table_range(&chunk, SIZE_MAX, 2, 1, 4096, &data, NULL));
    CHECK(!ab_lua_data_literal_table_range(&chunk, 2, SIZE_MAX, 1, 4096, &data, NULL));
    /* A selected read cannot import a register from before the window. */
    word(bytes + 12, 0 | (2u << 6));
    CHECK(!ab_lua_data_literal_table_range(&chunk, 2, 3, 1, 4096, &data, NULL) &&
          !data.owned);
    puts("Literal data windows reject behavioral words, external registers and "
         "overflow.");
    return 0;
}
