#include "ab/lua_text_write.h"
#include "lua_text_write_cases.h"

#include <stdio.h>
#include <string.h>

static AbDataValue string(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)value, strlen(value)}};
}

static AbDataValue number(float value) {
    AbDataValue output = {.kind = AB_DATA_NUMBER};
    memcpy(&output.bits, &value, sizeof(value));
    return output;
}

static bool invalid_next(void *context, const AbDataTable *table, size_t *cursor,
                         AbDataEntry *output, bool *present, AbError *error) {
    (void)context;
    (void)cursor;
    (void)error;
    *output = table->entries[0];
    *present = true;
    return true;
}

static bool edges(void) {
    uint8_t bytes[64];
    memset(bytes, 0xa5, sizeof(bytes));
    AbDataEntry entry = {string("x"), number(1)};
    AbDataTable root = {&entry, 1, 1, NULL};
    AbLuaTextWriter writer = {.data = bytes, .capacity = 6};
    if (!ab_lua_text_write(&root, (AbLuaTextWriteOrder){0}, &writer, NULL) ||
        writer.size != 6 || memcmp(bytes, "x = 1\n", 6) || bytes[6] != 0xa5)
        return false;
    writer = (AbLuaTextWriter){.data = bytes, .capacity = 5};
    if (ab_lua_text_write(&root, (AbLuaTextWriteOrder){0}, &writer, NULL) ||
        !writer.failed || writer.size > 5 || bytes[6] != 0xa5)
        return false;
    writer =
        (AbLuaTextWriter){.data = bytes, .capacity = sizeof(bytes), .writing = true};
    if (ab_lua_text_write(&root, (AbLuaTextWriteOrder){0}, &writer, NULL) ||
        writer.failed || !writer.writing || writer.size)
        return false;
    writer = (AbLuaTextWriter){.data = bytes, .capacity = sizeof(bytes)};
    entry.value = (AbDataValue){.kind = AB_DATA_TABLE, .table = &root};
    if (ab_lua_text_write(&root, (AbLuaTextWriteOrder){0}, &writer, NULL) ||
        !writer.failed || writer.size != 6)
        return false;
    writer = (AbLuaTextWriter){.data = bytes, .capacity = sizeof(bytes)};
    entry.value = (AbDataValue){.kind = AB_DATA_OBJECT, .object = &root};
    if (!ab_lua_text_write(&root, (AbLuaTextWriteOrder){0}, &writer, NULL) ||
        writer.size != 1 || bytes[0] != '\n')
        return false;
    writer = (AbLuaTextWriter){.data = bytes, .capacity = sizeof(bytes)};
    entry.value = number(1);
    if (ab_lua_text_write(&root, (AbLuaTextWriteOrder){.next = invalid_next}, &writer,
                          NULL) ||
        !writer.failed || writer.size)
        return false;
    puts("Saved-table writer capacity, cycles, reentry, type gates and iteration "
         "bounds pass.");
    return true;
}

static bool expected(FILE *file, uint8_t *bytes, size_t capacity, size_t *size) {
    char line[8192];
    if (!fgets(line, sizeof(line), file))
        return false;
    size_t count = strcspn(line, "\n");
    if (line[count] != '\n' || count % 2 || count / 2 > capacity)
        return false;
    for (size_t index = 0; index < count / 2; ++index) {
        unsigned word;
        if (sscanf(line + index * 2, "%2x", &word) != 1)
            return false;
        bytes[index] = (uint8_t)word;
    }
    *size = count / 2;
    return true;
}

static bool compare(FILE *file) {
    char header[256];
    size_t cases = 0;
    for (size_t index = 0; index < 8 && fgets(header, sizeof(header), file); ++index)
        if (sscanf(header, "AB_ANDROID_LUA_WRITE_V1 %zu", &cases) == 1)
            break;
    if (cases != LUA_WRITE_CASES)
        return false;
    for (size_t index = 0; index < cases; ++index) {
        LuaWriteCase recipe = lua_write_case(index);
        AbDataEntry array_fields[] = {
            {number(1), {.kind = AB_DATA_NUMBER, .bits = recipe.number}},
            {number(2), {.kind = AB_DATA_BOOLEAN, .bits = (uint32_t)(index % 2)}},
            {string("tail"), string(recipe.string)}};
        AbDataTable array = {array_fields, recipe.array ? 2 + index % 2 : 0, 3, NULL};
        AbDataValue value = {0};
        if (recipe.kind == 0)
            value = (AbDataValue){.kind = AB_DATA_NUMBER, .bits = recipe.number};
        else if (recipe.kind == 1)
            value =
                (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = (uint32_t)(index % 2)};
        else if (recipe.kind == 2)
            value = string(recipe.string);
        else
            value = (AbDataValue){.kind = AB_DATA_TABLE, .table = &array};
        AbDataEntry leaf = {string(recipe.key), value};
        AbDataTable nested = {&leaf, 1, 1, NULL};
        AbDataEntry top = recipe.nested
                              ? (AbDataEntry){string("outer"),
                                              {.kind = AB_DATA_TABLE, .table = &nested}}
                              : leaf;
        AbDataTable root = {&top, 1, 1, NULL};
        uint8_t output[4096], native[4096];
        AbLuaTextWriter writer = {.data = output, .capacity = sizeof(output)};
        AbError error = {0};
        size_t size = 0;
        if (!expected(file, native, sizeof(native), &size) ||
            !ab_lua_text_write(&root, (AbLuaTextWriteOrder){0}, &writer, &error) ||
            size != writer.size || memcmp(output, native, size)) {
            fprintf(stderr, "Original saved-table write differs at case%zu: %s\n",
                    index, error.message);
            return false;
        }
    }
    if (!fgets(header, sizeof(header), file) ||
        strcmp(header, "AB Lua write reference complete\n"))
        return false;
    printf("Original saved-table serializer: all %zu outputs match exactly.\n", cases);
    return true;
}

int main(int argc, char **argv) {
    if (argc == 1)
        return edges() ? 0 : 1;
    if (argc != 2)
        return 2;
    FILE *file = fopen(argv[1], "rb");
    if (!file)
        return 1;
    bool okay = compare(file);
    return fclose(file) == 0 && okay ? 0 : 1;
}
