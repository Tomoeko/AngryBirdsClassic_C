#include "ab/lua_data_json.h"

#include "ab/json.h"

#include <float.h>
#include <string.h>

_Static_assert(sizeof(float) == 4 && FLT_RADIX == 2 && FLT_MANT_DIG == 24 &&
                   FLT_MAX_EXP == 128,
               "Target data export requires IEEE binary32 floats");

typedef struct {
    FILE *file;
    const AbDataTable *ancestors[64];
    size_t value_count;
    AbError *error;
} DataWriter;

static void indent(FILE *file, size_t depth) {
    for (size_t index = 0; index < depth * 2; ++index) {
        fputc(' ', file);
    }
}

static bool write_value(DataWriter *writer, AbDataValue value, size_t depth) {
    if (depth >= 64 || ++writer->value_count > 1000000) {
        return ab_fail(writer->error,
                       "Static data export exceeds depth or value limit");
    }
    FILE *file = writer->file;
    switch (value.kind) {
        case AB_DATA_OBJECT:
            return ab_fail(writer->error,
                           "Cannot export a C runtime object as static data");
        case AB_DATA_NIL:
            fputs("{\"type\": \"nil\"}", file);
            break;
        case AB_DATA_BOOLEAN:
            fprintf(file, "{\"type\": \"boolean\", \"value\": %s}",
                    value.bits ? "true" : "false");
            break;
        case AB_DATA_NUMBER: {
            float number = 0;
            memcpy(&number, &value.bits, sizeof(number));
            fprintf(file, "{\"type\": \"number\", \"bits\": \"%08x\", \"value\": %.9g}",
                    value.bits, (double)number);
            break;
        }
        case AB_DATA_STRING:
            fputs("{\"type\": \"string\", \"bytes\": ", file);
            if (!ab_json_bytes(file, value.string)) {
                return ab_fail(writer->error, "Cannot write static data string");
            }
            fputc('}', file);
            break;
        case AB_DATA_TABLE: {
            if (!value.table) {
                return ab_fail(writer->error, "Static data export has a null table");
            }
            for (size_t index = 0; index < depth; ++index) {
                if (writer->ancestors[index] == value.table) {
                    return ab_fail(writer->error,
                                   "Static data export contains a table cycle");
                }
            }
            writer->ancestors[depth] = value.table;
            fputs("{\n", file);
            indent(file, depth + 1);
            fputs("\"type\": \"table\",\n", file);
            indent(file, depth + 1);
            fputs("\"entries\": [\n", file);
            for (size_t index = 0; index < value.table->count; ++index) {
                const AbDataEntry *entry = value.table->entries + index;
                indent(file, depth + 2);
                fputs("{\n", file);
                indent(file, depth + 3);
                fputs("\"key\": ", file);
                if (!write_value(writer, entry->key, depth + 3)) {
                    return false;
                }
                fputs(",\n", file);
                indent(file, depth + 3);
                fputs("\"value\": ", file);
                if (!write_value(writer, entry->value, depth + 3)) {
                    return false;
                }
                fputc('\n', file);
                indent(file, depth + 2);
                fputs(index + 1 == value.table->count ? "}\n" : "},\n", file);
            }
            indent(file, depth + 1);
            fputs("]\n", file);
            indent(file, depth);
            fputc('}', file);
            writer->ancestors[depth] = NULL;
            break;
        }
    }
    return !ferror(file) || ab_fail(writer->error, "Cannot write static data export");
}

bool ab_lua_data_json(const AbLuaData *data, FILE *file, AbError *error) {
    if (!data || !data->globals || !file) {
        return ab_fail(error, "Invalid static data export arguments");
    }
    DataWriter writer = {.file = file, .error = error};
    fputs("{\n  \"globals\": ", file);
    AbDataValue globals = {.kind = AB_DATA_TABLE, .table = data->globals};
    if (!write_value(&writer, globals, 1)) {
        return false;
    }
    fputs(",\n  \"returned\": ", file);
    if (!write_value(&writer, data->returned, 1)) {
        return false;
    }
    fputs("\n}\n", file);
    return !ferror(file) || ab_fail(error, "Cannot write static data export");
}
