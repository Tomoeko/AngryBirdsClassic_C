#ifndef AB_TEST_NATIVE_DATA_FIXTURE_H
#define AB_TEST_NATIVE_DATA_FIXTURE_H

#include "ab/lua_data.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    FILE *file;
    size_t values, tables;
} AbTestNativeData;

static bool native_data_line(FILE *file, char output[1024]) {
    if (!fgets(output, 1024, file)) {
        return false;
    }
    size_t length = strlen(output);
    if (!length || output[length - 1] != '\n') {
        return false;
    }
    output[length - 1] = 0;
    return true;
}

static int native_data_hex(char value) {
    if (value >= '0' && value <= '9') {
        return value - '0';
    }
    return value >= 'a' && value <= 'f' ? value - 'a' + 10 : -1;
}

static bool native_data_primitive(const char *line, uint8_t text[512],
                                  AbDataValue *output) {
    size_t length = strlen(line);
    AbDataValue value = {0};
    if (length == 1 && (line[0] == 'Y' || line[0] == 'F')) {
        value.kind = AB_DATA_BOOLEAN;
        value.bits = line[0] == 'Y';
    } else if (length == 1 && line[0] == 'N') {
        value.kind = AB_DATA_NIL;
    } else if (length == 9 && line[0] == 'D') {
        value.kind = AB_DATA_NUMBER;
        for (size_t index = 1; index < 9; ++index) {
            int digit = native_data_hex(line[index]);
            if (digit < 0) {
                return false;
            }
            value.bits = (value.bits << 4) | (uint32_t)digit;
        }
    } else if (length && line[0] == 'S' && length % 2 == 1) {
        value.kind = AB_DATA_STRING;
        value.string = (AbSpan){text, (length - 1) / 2};
        for (size_t index = 0; index < value.string.size; ++index) {
            int high = native_data_hex(line[1 + index * 2]);
            int low = native_data_hex(line[2 + index * 2]);
            if (high < 0 || low < 0) {
                return false;
            }
            text[index] = (uint8_t)(high * 16 + low);
        }
    } else {
        return false;
    }
    *output = value;
    return true;
}

static bool native_data_value(AbTestNativeData *reader, const AbDataValue *expected,
                              size_t depth);

static bool native_data_table(AbTestNativeData *reader, const AbDataTable *table,
                              size_t depth) {
    if (!table || table->count > 128 || table->count > table->capacity ||
        (table->count && !table->entries) || depth >= 64) {
        return false;
    }
    ++reader->tables;
    bool seen[128] = {0};
    for (;;) {
        char line[1024];
        if (!native_data_line(reader->file, line)) {
            return false;
        }
        if (strcmp(line, "E") == 0) {
            break;
        }
        uint8_t text[512];
        AbDataValue key;
        if (!native_data_primitive(line, text, &key) || key.kind == AB_DATA_NIL) {
            return false;
        }
        const AbDataValue *value = ab_data_get(table, key);
        size_t index = 0;
        while (index < table->count && &table->entries[index].value != value) {
            ++index;
        }
        if (index == table->count || seen[index] || !value ||
            !native_data_value(reader, value, depth + 1)) {
            return false;
        }
        seen[index] = true;
        ++reader->values;
    }
    for (size_t index = 0; index < table->count; ++index) {
        if (!seen[index] && table->entries[index].value.kind != AB_DATA_NIL) {
            return false;
        }
    }
    return true;
}

static bool native_data_value(AbTestNativeData *reader, const AbDataValue *expected,
                              size_t depth) {
    char line[1024];
    if (!expected || !native_data_line(reader->file, line)) {
        return false;
    }
    ++reader->values;
    if (strcmp(line, "T") == 0) {
        return expected->kind == AB_DATA_TABLE &&
               native_data_table(reader, expected->table, depth);
    }
    uint8_t text[512];
    AbDataValue value;
    if (!native_data_primitive(line, text, &value) || value.kind != expected->kind) {
        return false;
    }
    if (value.kind == AB_DATA_STRING) {
        return value.string.size == expected->string.size &&
               (!value.string.size || memcmp(value.string.data, expected->string.data,
                                             value.string.size) == 0);
    }
    return value.bits == expected->bits;
}

static bool native_data_compare(AbTestNativeData *reader, const AbDataTable *globals) {
    AbDataValue root = {.kind = AB_DATA_TABLE, .table = (AbDataTable *)globals};
    char marker[1024];
    return native_data_value(reader, &root, 0) &&
           native_data_line(reader->file, marker) &&
           strcmp(marker, "AB serialized reference complete") == 0 &&
           fgetc(reader->file) == EOF && !ferror(reader->file);
}

#endif
