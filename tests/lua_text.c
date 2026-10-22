#include "ab/lua_text.h"
#include "ab/lua_data_json.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Failed at line %d: %s\n", __LINE__, #condition);          \
            return false;                                                              \
        }                                                                              \
    } while (0)

static const AbDataValue *field(const AbDataTable *table, const char *name) {
    return ab_data_get(table,
                       (AbDataValue){.kind = AB_DATA_STRING,
                                     .string = {(const uint8_t *)name, strlen(name)}});
}

static bool literals(void) {
    char source[] =
        "Level1 = { completed = true, score = 31480, nested = { [1] = false, "
        "[true] = 'yes', ['spaced key'] = \"a\\000b\\255\\n\\t\\\\\\\"\", }, "
        "score = nil; score = 31480, negativeZero = -0, rounded = 16777217, "
        "fraction = -.125e+2, utf8 = '\xc3\xa9', [0] = 1, [-0] = 2 }; "
        "discard = 1 discard = nil; empty = {}";
    AbLuaTextData parsed = {0};
    AbError error = {0};
    CHECK(ab_lua_text_data((AbSpan){(const uint8_t *)source, strlen(source)}, 65536,
                           &parsed, &error));
    memset(source, 0, sizeof(source));
    const AbDataValue *level = field(parsed.data.globals, "Level1");
    CHECK(level && level->kind == AB_DATA_TABLE);
    const AbDataValue *score = field(level->table, "score");
    CHECK(score && score->kind == AB_DATA_NUMBER && score->bits == 0x46f5f000);
    CHECK(field(level->table, "completed")->bits == 1);
    CHECK(field(level->table, "negativeZero")->bits == 0x80000000);
    CHECK(field(level->table, "rounded")->bits == 0x4b800000);
    CHECK(field(level->table, "fraction")->bits == 0xc1480000);
    CHECK(field(level->table, "utf8")->string.size == 2);
    CHECK(field(parsed.data.globals, "discard") == NULL);
    CHECK(field(parsed.data.globals, "empty")->table->count == 0);
    const AbDataValue *zero =
        ab_data_get(level->table, (AbDataValue){.kind = AB_DATA_NUMBER, .bits = 0});
    CHECK(zero && zero->bits == 0x40000000);
    const AbDataTable *nested = field(level->table, "nested")->table;
    const AbDataValue *string = field(nested, "spaced key");
    const uint8_t expected[] = {'a', 0, 'b', 255, '\n', '\t', '\\', '"'};
    CHECK(string && string->kind == AB_DATA_STRING &&
          string->string.size == sizeof(expected) &&
          memcmp(string->string.data, expected, sizeof(expected)) == 0);
    const AbDataValue *yes =
        ab_data_get(nested, (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1});
    CHECK(yes && yes->string.size == 3 && memcmp(yes->string.data, "yes", 3) == 0);
    CHECK(parsed.data.table_count == 4 && parsed.data.allocated_bytes <= 65536);
    ab_lua_text_data_close(&parsed);
    ab_lua_text_data_close(&parsed);
    CHECK(!parsed.text.data && !parsed.data.owned);
    return true;
}

static bool malformed(void) {
    static const char *const invalid[] = {
        "x =",
        "x = {",
        "x = { a = 1 b = 2 }",
        "x = { [nil] = 1 }",
        "x = { [{}] = 1 }",
        "x = { true = 1 }",
        "return {}",
        "local x = {}",
        "x = other",
        "x = function() end",
        "x = 1 + 2",
        "x = -- comment\n1",
        "x = 1e",
        "x = 1e+",
        "x = .",
        "x = -",
        "x = 1e1000",
        "x = 1e39",
        "x = 0x10",
        "x = 1.2.3",
        "x = 1a",
        "x = 'unclosed",
        "x = 'raw\nline'",
        "x = '\\256'",
        "x = '\\q'",
        "true = 1",
        "x = truefalse",
        "x = {};;",
        "x = { a = 1,, }",
    };
    for (size_t index = 0; index < sizeof(invalid) / sizeof(invalid[0]); ++index) {
        AbLuaTextData parsed = {0};
        AbError error = {0};
        CHECK(!ab_lua_text_data(
            (AbSpan){(const uint8_t *)invalid[index], strlen(invalid[index])}, 65536,
            &parsed, &error));
        CHECK(error.message[0] && !parsed.text.data && !parsed.data.owned);
    }
    const uint8_t embedded_zero[] = {'x', '=', '1', 0, 'y', '=', '2'};
    AbLuaTextData parsed = {0};
    AbError error = {0};
    CHECK(!ab_lua_text_data((AbSpan){embedded_zero, sizeof(embedded_zero)}, 65536,
                            &parsed, &error));
    CHECK(!ab_lua_text_data((AbSpan){NULL, 1}, 65536, &parsed, &error));
    CHECK(!ab_lua_text_data((AbSpan){NULL, SIZE_MAX}, SIZE_MAX, &parsed, &error));
    CHECK(!ab_lua_text_data((AbSpan){NULL, 0}, 0, &parsed, &error));
    CHECK(!ab_lua_text_data((AbSpan){NULL, 0}, 65536, NULL, &error));
    return true;
}

static bool list_order(void) {
    char source[2048] = "x = {[1] = 100,";
    size_t length = strlen(source);
    for (unsigned index = 1; index <= 50; ++index) {
        int count = snprintf(source + length, sizeof(source) - length, "%u,", index);
        CHECK(count > 0 && (size_t)count < sizeof(source) - length);
        length += (size_t)count;
    }
    const char *ending = "[1]=300,51,[51]=400,nil,[52]=500}; a=-0 b=0";
    CHECK(strlen(ending) < sizeof(source) - length);
    memcpy(source + length, ending, strlen(ending));
    length += strlen(ending);
    AbLuaTextData parsed = {0};
    AbError error = {0};
    CHECK(ab_lua_text_data((AbSpan){(const uint8_t *)source, length}, 65536, &parsed,
                           &error));
    const AbDataTable *table = field(parsed.data.globals, "x")->table;
    const AbDataValue *first =
        ab_data_get(table, (AbDataValue){.kind = AB_DATA_NUMBER, .bits = 0x3f800000});
    const AbDataValue *last =
        ab_data_get(table, (AbDataValue){.kind = AB_DATA_NUMBER, .bits = 0x424c0000});
    CHECK(table->count == 51 && first && first->bits == 0x43960000);
    CHECK(last && last->bits == 0x424c0000);
    CHECK(
        !ab_data_get(table, (AbDataValue){.kind = AB_DATA_NUMBER, .bits = 0x42500000}));
    CHECK(field(parsed.data.globals, "a")->bits == 0x80000000 &&
          field(parsed.data.globals, "b")->bits == 0x80000000);
    ab_lua_text_data_close(&parsed);
    return true;
}

static bool bounded(void) {
    const char *source = "x = { a = { b = 'text' } }";
    AbSpan input = {(const uint8_t *)source, strlen(source)};
    AbLuaTextData parsed = {0};
    AbError error = {0};
    CHECK(ab_lua_text_data(input, 65536, &parsed, &error));
    size_t required = parsed.data.allocated_bytes;
    ab_lua_text_data_close(&parsed);
    CHECK(ab_lua_text_data(input, required, &parsed, &error));
    ab_lua_text_data_close(&parsed);
    for (size_t budget = 0; budget < required; ++budget) {
        CHECK(!ab_lua_text_data(input, budget, &parsed, &error));
        CHECK(!parsed.text.data && !parsed.data.owned);
    }
    for (size_t length = 1; length < input.size; ++length) {
        CHECK(!ab_lua_text_data((AbSpan){input.data, length}, 65536, &parsed, &error));
    }
    char deep[1024] = "x=";
    size_t length = 2;
    for (size_t index = 0; index < 65; ++index) {
        memcpy(deep + length, "{a=", 3);
        length += 3;
    }
    deep[length++] = '1';
    for (size_t index = 0; index < 65; ++index) {
        deep[length++] = '}';
    }
    CHECK(!ab_lua_text_data((AbSpan){(const uint8_t *)deep, length}, 65536, &parsed,
                            &error));
    CHECK(strstr(error.message, "nesting") != NULL);
    return true;
}

int main(int argc, char **argv) {
    if (argc > 2) {
        fprintf(stderr, "Usage: ab_lua_text_tests [SERIALIZED_LITERAL_FILE]\n");
        return 2;
    }
    if (!literals() || !malformed() || !bounded() || !list_order()) {
        return 1;
    }
    if (argc == 1) {
        return 0;
    }
    AbBuffer input = {0};
    AbLuaTextData parsed = {0};
    AbError error = {0};
    size_t budget = 32 * 1024 * 1024;
    bool okay = ab_file_read(argv[1], budget, &input, &error) &&
                ab_lua_text_data((AbSpan){input.data, input.size}, budget - input.size,
                                 &parsed, &error) &&
                ab_lua_data_json(&parsed.data, stdout, &error);
    if (fflush(stdout) != 0 || ferror(stdout)) {
        okay = ab_fail(&error, "Cannot flush literal test export");
    }
    if (!okay) {
        fprintf(stderr, "%s\n", error.message);
    }
    ab_lua_text_data_close(&parsed);
    ab_buffer_free(&input);
    return okay ? 0 : 1;
}
