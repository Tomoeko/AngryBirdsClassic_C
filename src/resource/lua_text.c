#include "ab/lua_text.h"

#include "data_internal.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    AbLuaTextData *output;
    size_t position;
    AbError *error;
    uint32_t zero_bits;
    bool has_zero;
} TextReader;

static bool text_error(TextReader *reader, const char *message) {
    return ab_fail(reader->error, "%s at serialized-table byte %zu", message,
                   reader->position);
}

static bool space_byte(uint8_t byte) {
    return byte == ' ' || byte == '\t' || byte == '\r' || byte == '\n' ||
           byte == '\f' || byte == '\v';
}

static bool name_start(uint8_t byte) {
    return (byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z') || byte == '_';
}

static bool decimal_digit(uint8_t byte) {
    return byte >= '0' && byte <= '9';
}

static uint8_t current(const TextReader *reader) {
    return reader->position < reader->output->text.size
               ? reader->output->text.data[reader->position]
               : 0;
}

static void skip_space(TextReader *reader) {
    while (reader->position < reader->output->text.size &&
           space_byte(current(reader))) {
        ++reader->position;
    }
}

static bool take(TextReader *reader, uint8_t byte) {
    skip_space(reader);
    if (reader->position >= reader->output->text.size || current(reader) != byte) {
        return text_error(reader, "Missing literal-table delimiter");
    }
    ++reader->position;
    return true;
}

static bool span_word(AbSpan span, const char *word) {
    size_t length = strlen(word);
    return span.size == length && memcmp(span.data, word, length) == 0;
}

static bool reserved_word(AbSpan span) {
    static const char *const words[] = {
        "and", "break",    "do",     "else", "elseif", "end",   "false",
        "for", "function", "if",     "in",   "local",  "nil",   "not",
        "or",  "repeat",   "return", "then", "true",   "until", "while",
    };
    for (size_t index = 0; index < sizeof(words) / sizeof(words[0]); ++index) {
        if (span_word(span, words[index])) {
            return true;
        }
    }
    return false;
}

static bool read_name(TextReader *reader, AbSpan *name) {
    skip_space(reader);
    if (!name_start(current(reader))) {
        return text_error(reader, "Expected a serialized global or field name");
    }
    size_t start = reader->position++;
    while (name_start(current(reader)) || decimal_digit(current(reader))) {
        ++reader->position;
    }
    *name = (AbSpan){reader->output->text.data + start, reader->position - start};
    return true;
}

static bool read_string(TextReader *reader, AbDataValue *value) {
    uint8_t quote = current(reader);
    size_t start = ++reader->position;
    size_t destination = start;
    uint8_t *text = reader->output->text.data;
    while (reader->position < reader->output->text.size) {
        uint8_t byte = text[reader->position++];
        if (byte == quote) {
            *value = (AbDataValue){.kind = AB_DATA_STRING,
                                   .string = {text + start, destination - start}};
            return true;
        }
        if (byte == '\n' || byte == '\r' || !byte) {
            return text_error(reader, "Unescaped line ending or zero in string");
        }
        if (byte == '\\') {
            if (reader->position == reader->output->text.size) {
                break;
            }
            byte = text[reader->position++];
            if (decimal_digit(byte)) {
                unsigned number = (unsigned)(byte - '0');
                for (unsigned count = 1; count < 3 && decimal_digit(current(reader));
                     ++count) {
                    number = number * 10 + (unsigned)(current(reader) - '0');
                    ++reader->position;
                }
                if (number > 255) {
                    return text_error(reader, "Decimal string escape exceeds a byte");
                }
                byte = (uint8_t)number;
            } else {
                switch (byte) {
                    case 'a':
                        byte = '\a';
                        break;
                    case 'b':
                        byte = '\b';
                        break;
                    case 'f':
                        byte = '\f';
                        break;
                    case 'n':
                        byte = '\n';
                        break;
                    case 'r':
                        byte = '\r';
                        break;
                    case 't':
                        byte = '\t';
                        break;
                    case 'v':
                        byte = '\v';
                        break;
                    case '\\':
                    case '\'':
                    case '"':
                        break;
                    case '\n':
                    case '\r': {
                        uint8_t other = byte == '\n' ? '\r' : '\n';
                        if (current(reader) == other) {
                            ++reader->position;
                        }
                        byte = '\n';
                        break;
                    }
                    default:
                        return text_error(reader,
                                          "Unsupported serialized string escape");
                }
            }
        }
        text[destination++] = byte;
    }
    return text_error(reader, "Unterminated serialized string");
}

static bool read_number(TextReader *reader, AbDataValue *value) {
    size_t start = reader->position;
    if (current(reader) == '-') {
        ++reader->position;
    }
    size_t digits = 0;
    while (decimal_digit(current(reader))) {
        ++reader->position;
        ++digits;
    }
    if (current(reader) == '.') {
        ++reader->position;
        while (decimal_digit(current(reader))) {
            ++reader->position;
            ++digits;
        }
    }
    if (!digits) {
        return text_error(reader, "Missing serialized number digits");
    }
    if (current(reader) == 'e' || current(reader) == 'E') {
        ++reader->position;
        if (current(reader) == '+' || current(reader) == '-') {
            ++reader->position;
        }
        size_t exponent_start = reader->position;
        while (decimal_digit(current(reader))) {
            ++reader->position;
        }
        if (reader->position == exponent_start) {
            return text_error(reader, "Missing serialized exponent digits");
        }
    }
    if (reader->position - start > 128) {
        return text_error(reader, "Serialized number exceeds token limit");
    }
    if (name_start(current(reader)) || current(reader) == '.') {
        return text_error(reader, "Invalid suffix on serialized number");
    }
    uint8_t *text = reader->output->text.data;
    uint8_t delimiter = text[reader->position];
    text[reader->position] = 0;
    char *end = NULL;
    double parsed = strtod((const char *)text + start, &end);
    float number = (float)parsed;
    text[reader->position] = delimiter;
    if (!isfinite(parsed) || !isfinite(number) ||
        end != (char *)text + reader->position) {
        return text_error(reader, "Unsupported nonfinite or malformed number");
    }
    *value = (AbDataValue){.kind = AB_DATA_NUMBER};
    memcpy(&value->bits, &number, sizeof(number));
    /* Numeric constant interning in the target treats both zero signs as equal.
     * All literal assignments share one compiled chunk and retain its first sign. */
    if (number == 0) {
        if (reader->has_zero) {
            value->bits = reader->zero_bits;
        } else {
            reader->zero_bits = value->bits;
            reader->has_zero = true;
        }
    }
    return true;
}

static bool read_value(TextReader *reader, size_t depth, AbDataValue *value);

static bool read_field(TextReader *reader, size_t depth, AbDataValue *key,
                       AbDataValue *value, bool *list_field) {
    skip_space(reader);
    *list_field = false;
    if (current(reader) == '[') {
        ++reader->position;
        return read_value(reader, depth, key) && take(reader, ']') &&
               take(reader, '=') && read_value(reader, depth, value);
    }
    size_t start = reader->position;
    if (name_start(current(reader))) {
        AbSpan name;
        if (!read_name(reader, &name)) {
            return false;
        }
        skip_space(reader);
        if (current(reader) == '=') {
            if (reserved_word(name)) {
                return text_error(reader, "Reserved name used as an unquoted key");
            }
            *key = (AbDataValue){.kind = AB_DATA_STRING, .string = name};
            ++reader->position;
            return read_value(reader, depth, value);
        }
    }
    reader->position = start;
    *list_field = true;
    return read_value(reader, depth, value);
}

static bool store_list(TextReader *reader, AbDataTable *table,
                       const AbDataValue *pending, size_t count, size_t total) {
    for (size_t index = 0; index < count; ++index) {
        float number = (float)(total - count + index + 1);
        AbDataValue key = {.kind = AB_DATA_NUMBER};
        memcpy(&key.bits, &number, sizeof(number));
        if (!ab_data_table_put(&reader->output->data, table, key, pending[index],
                               reader->error)) {
            return false;
        }
    }
    return true;
}

static bool read_table(TextReader *reader, size_t depth, AbDataValue *value) {
    if (depth >= 64) {
        return text_error(reader, "Serialized table nesting exceeds limit");
    }
    ++reader->position;
    AbDataTable *table = ab_data_table_new(&reader->output->data, reader->error);
    if (!table) {
        return false;
    }
    /* The target's literal constructors publish list fields in groups of 50.
     * Preserve that ordering when bracketed numeric keys overlap a group. */
    AbDataValue pending[50];
    size_t pending_count = 0, list_count = 0;
    skip_space(reader);
    while (current(reader) != '}') {
        if (reader->position == reader->output->text.size) {
            return text_error(reader, "Unterminated serialized table");
        }
        AbDataValue key = {0}, entry = {0};
        bool list_field;
        if (!read_field(reader, depth + 1, &key, &entry, &list_field)) {
            return false;
        }
        if (list_field) {
            if (list_count == 16777216) {
                return text_error(reader, "Serialized list exceeds exact index range");
            }
            ++list_count;
            pending[pending_count++] = entry;
            if (pending_count == 50) {
                if (!store_list(reader, table, pending, pending_count, list_count)) {
                    return false;
                }
                pending_count = 0;
            }
        } else if (!ab_data_table_put(&reader->output->data, table, key, entry,
                                      reader->error)) {
            return false;
        }
        skip_space(reader);
        uint8_t delimiter = current(reader);
        if (delimiter == ',' || delimiter == ';') {
            ++reader->position;
            skip_space(reader);
        } else if (delimiter != '}') {
            return text_error(reader, "Expected serialized table separator");
        }
    }
    ++reader->position;
    if (!store_list(reader, table, pending, pending_count, list_count)) {
        return false;
    }
    *value = (AbDataValue){.kind = AB_DATA_TABLE, .table = table};
    return true;
}

static bool read_value(TextReader *reader, size_t depth, AbDataValue *value) {
    skip_space(reader);
    uint8_t byte = current(reader);
    if (byte == '{') {
        return read_table(reader, depth, value);
    }
    if (byte == '\'' || byte == '"') {
        return read_string(reader, value);
    }
    if (byte == '-' || byte == '.' || decimal_digit(byte)) {
        return read_number(reader, value);
    }
    AbSpan name;
    if (!read_name(reader, &name)) {
        return false;
    }
    *value = (AbDataValue){0};
    if (span_word(name, "true") || span_word(name, "false")) {
        value->kind = AB_DATA_BOOLEAN;
        value->bits = span_word(name, "true") ? 1u : 0u;
        return true;
    }
    if (span_word(name, "nil")) {
        return true;
    }
    return text_error(reader, "Only literal data values are supported");
}

static bool read_globals(TextReader *reader) {
    AbLuaData *data = &reader->output->data;
    data->globals = ab_data_table_new(data, reader->error);
    if (!data->globals) {
        return false;
    }
    skip_space(reader);
    while (reader->position < reader->output->text.size) {
        AbSpan name;
        AbDataValue value;
        if (!read_name(reader, &name)) {
            return false;
        }
        if (reserved_word(name)) {
            return text_error(reader, "Reserved name used as a serialized global");
        }
        if (!take(reader, '=') || !read_value(reader, 0, &value) ||
            !ab_data_table_put(data, data->globals,
                               (AbDataValue){.kind = AB_DATA_STRING, .string = name},
                               value, reader->error)) {
            return false;
        }
        skip_space(reader);
        if (current(reader) == ';') {
            ++reader->position;
            skip_space(reader);
        }
    }
    return true;
}

void ab_lua_text_data_close(AbLuaTextData *data) {
    if (data) {
        ab_lua_data_close(&data->data);
        ab_buffer_free(&data->text);
    }
}

bool ab_lua_text_data(AbSpan input, size_t memory_limit, AbLuaTextData *output,
                      AbError *error) {
    if (!output) {
        return ab_fail(error, "Missing serialized-table output");
    }
    *output = (AbLuaTextData){0};
    if ((!input.data && input.size) || input.size == SIZE_MAX ||
        input.size + 1 > memory_limit) {
        return ab_fail(error, "Serialized table text exceeds memory limit");
    }
    output->text.data = malloc(input.size + 1);
    if (!output->text.data) {
        return ab_fail(error, "Cannot allocate serialized table text");
    }
    output->text.size = input.size;
    if (input.size) {
        memcpy(output->text.data, input.data, input.size);
    }
    output->text.data[input.size] = 0;
    output->data.memory_limit = memory_limit;
    output->data.allocated_bytes = input.size + 1;
    TextReader reader = {.output = output, .error = error};
    if (!read_globals(&reader)) {
        ab_lua_text_data_close(output);
        return false;
    }
    return true;
}
