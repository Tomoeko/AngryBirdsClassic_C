#include "ab/lua_text_write.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    AbLuaTextWriter *output;
    AbLuaTextWriteOrder order;
    const AbDataTable *ancestors[32];
    AbError *error;
} Writer;

static bool append(Writer *writer, const void *bytes, size_t size) {
    AbLuaTextWriter *output = writer->output;
    if (size > output->capacity - output->size)
        return ab_fail(writer->error, "Serialized table exceeds output capacity");
    if (size)
        memcpy(output->data + output->size, bytes, size);
    output->size += size;
    return true;
}

static bool text(Writer *writer, const char *bytes) {
    return append(writer, bytes, strlen(bytes));
}

static bool margin(Writer *writer, size_t depth) {
    static const char spaces[] = "    ";
    for (size_t index = 0; index < depth; ++index)
        if (!append(writer, spaces, 4))
            return false;
    return true;
}

static AbSpan cstring(AbSpan input) {
    if (input.size && input.data) {
        const uint8_t *zero = memchr(input.data, 0, input.size);
        if (zero)
            input.size = (size_t)(zero - input.data);
    }
    return input;
}

static bool named(AbSpan name, const char *word) {
    return name.size == strlen(word) &&
           (!name.size || !memcmp(name.data, word, name.size));
}

static bool letter(uint8_t byte) {
    return (byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z') || byte == '_';
}

static bool digit(uint8_t byte) {
    return byte >= '0' && byte <= '9';
}

static bool key_name(AbSpan key) {
    if (key.size && !letter(key.data[0]))
        return false;
    for (size_t index = 1; index < key.size; ++index)
        if (!letter(key.data[index]) && !digit(key.data[index]))
            return false;
    static const char *const reserved[] = {
        "and", "break",    "do",     "else", "elseif", "end",   "false",
        "for", "function", "if",     "in",   "local",  "nil",   "not",
        "or",  "repeat",   "return", "then", "true",   "until", "while"};
    for (size_t index = 0; index < sizeof(reserved) / sizeof(reserved[0]); ++index)
        if (named(key, reserved[index]))
            return false;
    return true;
}

static bool string_literal(Writer *writer, AbSpan string) {
    if (string.size && !string.data)
        return ab_fail(writer->error, "Missing serialized string data");
    string = cstring(string);
    if (!text(writer, "\""))
        return false;
    for (size_t index = 0; index < string.size; ++index) {
        uint8_t byte = string.data[index];
        const char *escape = NULL;
        switch (byte) {
            case 7:
                escape = "\\a";
                break;
            case 8:
                escape = "\\b";
                break;
            case 9:
                escape = "\\t";
                break;
            case 10:
                escape = "\\n";
                break;
            case 11:
                escape = "\\v";
                break;
            case 12:
                escape = "\\f";
                break;
            case 13:
                escape = "\\r";
                break;
            case '\"':
                escape = "\\\"";
                break;
            case '\'':
                escape = "\\'";
                break;
            case '\\':
                escape = "\\\\";
                break;
            default:
                break;
        }
        if (escape) {
            if (!text(writer, escape))
                return false;
        } else if (byte >= 32 && byte <= 126) {
            if (!append(writer, &byte, 1))
                return false;
        } else {
            bool full = index + 1 < string.size && digit(string.data[index + 1]);
            char bytes[4] = {'\\', 0, 0, 0};
            size_t used = 1;
            if (full || byte >= 100)
                bytes[used++] = (char)('0' + byte / 100);
            if (full || byte >= 10)
                bytes[used++] = (char)('0' + (byte / 10) % 10);
            bytes[used++] = (char)('0' + byte % 10);
            if (!append(writer, bytes, used))
                return false;
        }
    }
    return text(writer, "\"");
}

static bool number(Writer *writer, uint32_t bits) {
    float value;
    memcpy(&value, &bits, sizeof(value));
    if (!isfinite(value))
        return ab_fail(writer->error, "Nonfinite saved-table number is unsupported");
    char bytes[32];
    int length = snprintf(bytes, sizeof(bytes), "%g", (double)value);
    if (length < 1 || (size_t)length >= sizeof(bytes) || strchr(bytes, ','))
        return ab_fail(writer->error, "Cannot format saved-table number in C locale");
    char *exponent = strchr(bytes, 'e');
    if (exponent) {
        char *digits = exponent + 1;
        if (*digits == '-' || *digits == '+')
            ++digits;
        size_t count = strlen(digits);
        while (count > 2 && *digits == '0') {
            memmove(digits, digits + 1, count);
            --count;
            --length;
        }
    }
    return append(writer, bytes, (size_t)length);
}

static bool write_table(Writer *writer, const AbDataTable *table, size_t depth,
                        bool nested);

static bool value(Writer *writer, AbDataValue item, size_t depth) {
    if (item.kind == AB_DATA_BOOLEAN)
        return text(writer, item.bits ? "true" : "false");
    if (item.kind == AB_DATA_NUMBER)
        return number(writer, item.bits);
    if (item.kind == AB_DATA_STRING)
        return string_literal(writer, item.string);
    if (item.kind == AB_DATA_TABLE)
        return text(writer, "{\n") &&
               write_table(writer, item.table, depth + 1, true) &&
               margin(writer, depth) && text(writer, "}");
    return ab_fail(writer->error, "Unsupported serialized table value");
}

static bool next(Writer *writer, const AbDataTable *table, size_t *cursor,
                 AbDataEntry *output, bool *present) {
    if (writer->order.next)
        return writer->order.next(writer->order.context, table, cursor, output, present,
                                  writer->error);
    *present = *cursor < table->count;
    if (*present)
        *output = table->entries[(*cursor)++];
    return true;
}

static bool serializable(AbDataKind kind) {
    return kind == AB_DATA_BOOLEAN || kind == AB_DATA_NUMBER ||
           kind == AB_DATA_STRING || kind == AB_DATA_TABLE;
}

static bool write_table(Writer *writer, const AbDataTable *table, size_t depth,
                        bool nested) {
    if (!table || depth >= 32 || table->count > table->capacity ||
        (table->count && !table->entries))
        return ab_fail(writer->error, "Invalid/deep saved table graph");
    for (size_t index = 0; index < depth; ++index)
        if (writer->ancestors[index] == table)
            return ab_fail(writer->error, "Cyclic saved table is unsupported");
    writer->ancestors[depth] = table;
    bool prefix = true, first = true;
    size_t cursor = 0, visited = 0, last = 0;
    for (;;) {
        AbDataEntry entry;
        bool present = false;
        size_t previous_cursor = cursor;
        if (!next(writer, table, &cursor, &entry, &present))
            return false;
        if (!present)
            break;
        if (cursor <= previous_cursor || visited++ >= table->count)
            return ab_fail(writer->error, "Invalid saved-table iteration progress");
        AbDataValue key = entry.key;
        if (!serializable(entry.value.kind) ||
            (key.kind != AB_DATA_BOOLEAN && key.kind != AB_DATA_NUMBER &&
             key.kind != AB_DATA_STRING))
            continue;
        if (key.kind == AB_DATA_STRING) {
            if (key.string.size && !key.string.data)
                return ab_fail(writer->error, "Missing saved-table key data");
            key.string = cstring(key.string);
        }
        bool valid = key.kind == AB_DATA_STRING && key_name(key.string);
        if (key.kind == AB_DATA_STRING && !nested && !valid)
            continue;
        if (prefix && key.kind == AB_DATA_NUMBER) {
            float numeric;
            memcpy(&numeric, &key.bits, sizeof(numeric));
            if (numeric == (float)(last + 1)) {
                if ((first && !margin(writer, depth)) ||
                    !value(writer, entry.value, depth) || !text(writer, ","))
                    return false;
                first = false;
                ++last;
                continue;
            }
        }
        if (prefix && !first && !text(writer, "\n"))
            return false;
        if (key.kind == AB_DATA_STRING &&
            (named(key.string, "_G") || named(key.string, "this"))) {
            prefix = false;
            continue;
        }
        if (!nested && key.kind != AB_DATA_STRING)
            return ab_fail(writer->error,
                           "Non-prefix numeric/Boolean root save key is unsupported");
        if (!margin(writer, depth))
            return false;
        if (nested && !valid) {
            if (!text(writer, "[") || !value(writer, key, depth) || !text(writer, "]"))
                return false;
        } else if (!append(writer, key.string.data, key.string.size)) {
            return false;
        }
        if (!text(writer, " = ") || !value(writer, entry.value, depth) ||
            (nested && !text(writer, ",")) || !text(writer, "\n"))
            return false;
        prefix = first = false;
    }
    return !prefix || text(writer, "\n");
}

bool ab_lua_text_write(const AbDataTable *table, AbLuaTextWriteOrder order,
                       AbLuaTextWriter *output, AbError *error) {
    if (!output || output->writing || output->failed ||
        output->size > output->capacity || (output->capacity && !output->data))
        return ab_fail(error, "Invalid or active saved-table writer");
    output->writing = true;
    Writer writer = {.output = output, .order = order, .error = error};
    bool okay = write_table(&writer, table, 0, false);
    output->writing = false;
    output->failed = !okay;
    return okay;
}
