#include "ab/lua_data.h"

#include "data_internal.h"

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

_Static_assert(sizeof(float) == 4 && FLT_RADIX == 2 && FLT_MANT_DIG == 24 &&
                   FLT_MAX_EXP == 128,
               "Static Lua arithmetic requires binary32 float");

typedef struct {
    const AbLuaPrototype *prototype;
    AbLuaData *data;
    const AbDataTable *external_globals;
    AbDataTable *nil_globals;
    AbDataValue registers[256];
    bool defined[256];
    AbError *error;
} DataRecovery;

static bool key_equal(AbDataValue left, AbDataValue right) {
    if (left.kind != right.kind) {
        return false;
    }
    if (left.kind == AB_DATA_STRING) {
        return left.string.size == right.string.size &&
               (!left.string.size ||
                memcmp(left.string.data, right.string.data, left.string.size) == 0);
    }
    if (left.kind == AB_DATA_NUMBER) {
        return left.bits == right.bits || (!(left.bits & UINT32_C(0x7fffffff)) &&
                                           !(right.bits & UINT32_C(0x7fffffff)));
    }
    return left.kind == AB_DATA_BOOLEAN && left.bits == right.bits;
}

const AbDataValue *ab_data_get(const AbDataTable *table, AbDataValue key) {
    if (!table) {
        return NULL;
    }
    for (size_t index = 0; index < table->count; ++index) {
        if (key_equal(table->entries[index].key, key)) {
            return &table->entries[index].value;
        }
    }
    return NULL;
}

AbDataTable *ab_data_table_new(AbLuaData *data, AbError *error) {
    if (sizeof(AbDataTable) > data->memory_limit - data->allocated_bytes) {
        ab_fail(error, "Static Lua data exceeds memory limit");
        return NULL;
    }
    AbDataTable *table = calloc(1, sizeof(*table));
    if (!table) {
        ab_fail(error, "Cannot allocate static Lua table");
        return NULL;
    }
    table->next_owned = data->owned;
    data->owned = table;
    data->allocated_bytes += sizeof(*table);
    ++data->table_count;
    return table;
}

bool ab_data_table_put(AbLuaData *data, AbDataTable *table, AbDataValue key,
                       AbDataValue value, AbError *error) {
    if (!table || (key.kind != AB_DATA_STRING && key.kind != AB_DATA_NUMBER &&
                   key.kind != AB_DATA_BOOLEAN)) {
        return ab_fail(error, "Unsupported static Lua table or key type");
    }
    if (value.kind == AB_DATA_NIL) {
        ab_data_layout_nil_assignment(data, table);
    }
    for (size_t index = 0; index < table->count; ++index) {
        if (key_equal(table->entries[index].key, key)) {
            if (value.kind == AB_DATA_NIL) {
                memmove(table->entries + index, table->entries + index + 1,
                        (table->count - index - 1) * sizeof(*table->entries));
                --table->count;
            } else {
                table->entries[index].value = value;
            }
            return true;
        }
    }
    if (value.kind == AB_DATA_NIL) {
        return true;
    }
    if (table->count == table->capacity) {
        size_t capacity = table->capacity ? table->capacity * 2 : 8;
        size_t added = capacity - table->capacity;
        if (capacity < table->capacity ||
            added > (data->memory_limit - data->allocated_bytes) /
                        sizeof(*table->entries)) {
            return ab_fail(error, "Static Lua table entries exceed memory limit");
        }
        AbDataEntry *entries = realloc(table->entries, capacity * sizeof(*entries));
        if (!entries) {
            return ab_fail(error, "Cannot allocate static Lua table entries");
        }
        table->entries = entries;
        table->capacity = capacity;
        data->allocated_bytes += added * sizeof(*entries);
    }
    table->entries[table->count++] = (AbDataEntry){key, value};
    return true;
}

static bool constant_value(DataRecovery *recovery, size_t index, AbDataValue *output) {
    if (index >= recovery->prototype->constant_count) {
        return ab_fail(recovery->error, "Static Lua constant index exceeds table");
    }
    const AbLuaConstant *constant = recovery->prototype->constants + index;
    *output = (AbDataValue){0};
    output->bits = constant->bits;
    output->string = constant->string;
    switch (constant->kind) {
        case AB_LUA_NIL:
            output->kind = AB_DATA_NIL;
            break;
        case AB_LUA_BOOLEAN:
            output->kind = AB_DATA_BOOLEAN;
            break;
        case AB_LUA_STRING:
            output->kind = AB_DATA_STRING;
            break;
        case AB_LUA_NUMBER:
            if ((constant->bits & UINT32_C(0x7f800000)) == UINT32_C(0x7f800000)) {
                return ab_fail(recovery->error,
                               "Nonfinite static Lua number is unsupported");
            }
            output->kind = AB_DATA_NUMBER;
            break;
    }
    return true;
}

static bool register_value(DataRecovery *recovery, unsigned index,
                           AbDataValue *output) {
    if (index >= recovery->prototype->stack_size || !recovery->defined[index]) {
        return ab_fail(recovery->error, "Static Lua input register is unavailable");
    }
    *output = recovery->registers[index];
    return true;
}

static bool operand_value(DataRecovery *recovery, unsigned operand,
                          AbDataValue *output) {
    if (operand & 256) {
        return constant_value(recovery, operand & 255, output);
    }
    return register_value(recovery, operand, output);
}

static bool writable_table(DataRecovery *recovery, const AbDataTable *table) {
    if (!recovery->external_globals) {
        return true;
    }
    for (const AbDataTable *owned = recovery->data->owned; owned;
         owned = owned->next_owned) {
        if (owned == table) {
            return true;
        }
    }
    return ab_fail(recovery->error,
                   "Static Lua mutation of imported data is unsupported");
}

static bool recover_assignment(DataRecovery *recovery, unsigned opcode, unsigned a,
                               unsigned b, unsigned c, size_t extended) {
    AbDataValue key = {0};
    AbDataValue value = {0};
    AbDataValue table = {0};
    if (opcode == 7) {
        if (!constant_value(recovery, extended, &key) ||
            !register_value(recovery, a, &value)) {
            return false;
        }
        if (value.kind == AB_DATA_NIL && !recovery->nil_globals) {
            recovery->nil_globals = ab_data_table_new(recovery->data, recovery->error);
            if (!recovery->nil_globals) {
                return false;
            }
        }
        if (recovery->nil_globals) {
            AbDataValue marker = {.kind = AB_DATA_BOOLEAN, .bits = 1};
            if (!ab_data_table_put(recovery->data, recovery->nil_globals, key,
                                   value.kind == AB_DATA_NIL ? marker
                                                             : (AbDataValue){0},
                                   recovery->error)) {
                return false;
            }
        }
        return ab_data_table_put(recovery->data, recovery->data->globals, key, value,
                                 recovery->error);
    }
    if (opcode == 9) {
        if (!register_value(recovery, a, &table)) {
            return false;
        }
        if (table.kind != AB_DATA_TABLE) {
            return ab_fail(recovery->error, "Static Lua assignment requires a table");
        }
        return writable_table(recovery, table.table) &&
               operand_value(recovery, b, &key) && operand_value(recovery, c, &value) &&
               ab_data_table_put(recovery->data, table.table, key, value,
                                 recovery->error);
    }
    if (opcode == 5) {
        if (!constant_value(recovery, extended, &key)) {
            return false;
        }
        table = (AbDataValue){.kind = AB_DATA_TABLE, .table = recovery->data->globals};
    } else if (!register_value(recovery, b, &table) ||
               !operand_value(recovery, c, &key)) {
        return false;
    }
    if (table.kind != AB_DATA_TABLE) {
        return ab_fail(recovery->error, "Static Lua lookup requires a recovered table");
    }
    const AbDataValue *found = ab_data_get(table.table, key);
    if (!found && opcode == 5 && !ab_data_get(recovery->nil_globals, key)) {
        return ab_fail(recovery->error,
                       "Static Lua data depends on an external global");
    }
    recovery->registers[a] = found ? *found : (AbDataValue){0};
    recovery->defined[a] = true;
    return true;
}

static bool recover_arithmetic(DataRecovery *recovery, unsigned opcode, unsigned b,
                               unsigned c, AbDataValue *output) {
    AbDataValue left = {0};
    AbDataValue right = {0};
    if (!operand_value(recovery, b, &left) || !operand_value(recovery, c, &right)) {
        return false;
    }
    if (left.kind != AB_DATA_NUMBER || right.kind != AB_DATA_NUMBER) {
        return ab_fail(recovery->error,
                       "Static Lua arithmetic requires numeric operands");
    }
    if (opcode == 15 && !(right.bits & UINT32_C(0x7fffffff))) {
        return ab_fail(recovery->error, "Static Lua division by zero is unsupported");
    }
    float first;
    float second;
    memcpy(&first, &left.bits, sizeof(first));
    memcpy(&second, &right.bits, sizeof(second));
    float result;
    switch (opcode) {
        case 12:
            result = first + second;
            break;
        case 13:
            result = first - second;
            break;
        case 14:
            result = first * second;
            break;
        default:
            result = first / second;
            break;
    }
    if (!isfinite(result)) {
        return ab_fail(recovery->error,
                       "Nonfinite static Lua arithmetic is unsupported");
    }
    *output = (AbDataValue){.kind = AB_DATA_NUMBER};
    memcpy(&output->bits, &result, sizeof(result));
    return true;
}

static uint32_t integer_bits(uint32_t value) {
    unsigned exponent = 0;
    for (uint32_t shifted = value; shifted > 1; shifted >>= 1) {
        ++exponent;
    }
    uint32_t significand =
        exponent <= 23 ? value << (23 - exponent) : value >> (exponent - 23);
    return (uint32_t)(exponent + 127) << 23 | (significand & UINT32_C(0x007fffff));
}

static bool recover_list(DataRecovery *recovery, unsigned a, unsigned count,
                         uint32_t block) {
    AbDataValue table = {0};
    if (!register_value(recovery, a, &table)) {
        return false;
    }
    if (!count || !block || table.kind != AB_DATA_TABLE ||
        count >= (unsigned)recovery->prototype->stack_size - a) {
        return ab_fail(recovery->error,
                       "Static Lua list requires a bounded constructor");
    }
    if (!writable_table(recovery, table.table)) {
        return false;
    }
    uint64_t start = ((uint64_t)block - 1) * 50;
    if (start + count > UINT32_C(16777216)) {
        return ab_fail(recovery->error,
                       "Static Lua list index exceeds exact binary32 range");
    }
    for (unsigned index = 1; index <= count; ++index) {
        AbDataValue value = {0};
        if (!register_value(recovery, a + index, &value)) {
            return false;
        }
        AbDataValue key = {.kind = AB_DATA_NUMBER,
                           .bits = integer_bits((uint32_t)start + index)};
        if (!ab_data_table_put(recovery->data, table.table, key, value,
                               recovery->error)) {
            return false;
        }
    }
    return true;
}

static bool recover_word(DataRecovery *recovery, uint32_t word, uint32_t list_block,
                         bool *returned) {
    unsigned opcode = word & 63;
    unsigned a = (word >> 6) & 255;
    unsigned b = (word >> 23) & 511;
    unsigned c = (word >> 14) & 511;
    size_t extended = word >> 14;
    if (a >= recovery->prototype->stack_size) {
        return ab_fail(recovery->error,
                       "Static Lua destination exceeds register capacity");
    }
    AbDataValue value = {0};
    switch (opcode) {
        case 0:
            if (!register_value(recovery, b, &value)) {
                return false;
            }
            break;
        case 1:
            if (!constant_value(recovery, extended, &value)) {
                return false;
            }
            break;
        case 2:
            if (b > 1 || c) {
                return ab_fail(recovery->error,
                               "Static Lua boolean skip is unsupported");
            }
            value = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = b};
            break;
        case 3:
            if (b < a || b >= recovery->prototype->stack_size) {
                return ab_fail(recovery->error,
                               "Static Lua nil range exceeds registers");
            }
            for (unsigned index = a; index <= b; ++index) {
                recovery->registers[index] = (AbDataValue){0};
                recovery->defined[index] = true;
            }
            return true;
        case 5:
        case 6:
        case 7:
        case 9:
            return recover_assignment(recovery, opcode, a, b, c, extended);
        case 10:
            value.kind = AB_DATA_TABLE;
            value.table = ab_data_table_new(recovery->data, recovery->error);
            if (!value.table || !ab_data_record_layout(recovery->data, value.table, b,
                                                       c, recovery->error)) {
                return false;
            }
            break;
        case 12:
        case 13:
        case 14:
        case 15:
            if (!recover_arithmetic(recovery, opcode, b, c, &value)) {
                return false;
            }
            break;
        case 18:
            if (!register_value(recovery, b, &value) || value.kind != AB_DATA_NUMBER) {
                return ab_fail(recovery->error,
                               "Static Lua negation requires a number");
            }
            value.bits ^= UINT32_C(0x80000000);
            break;
        case 30:
            if (b == 2) {
                if (!register_value(recovery, a, &recovery->data->returned)) {
                    return false;
                }
            } else if (b != 1) {
                return ab_fail(recovery->error,
                               "Static Lua multiple returns are unsupported");
            }
            *returned = true;
            return true;
        case 34:
            return recover_list(recovery, a, b, list_block);
        default:
            return ab_fail(recovery->error,
                           "Lua opcode %u requires behavioral recovery", opcode);
    }
    recovery->registers[a] = value;
    recovery->defined[a] = true;
    return true;
}

void ab_lua_data_close(AbLuaData *data) {
    if (!data) {
        return;
    }
    AbDataTableLayout *layout = data->layouts;
    while (layout) {
        AbDataTableLayout *next = layout->next;
        free(layout);
        layout = next;
    }
    AbDataTable *table = data->owned;
    while (table) {
        AbDataTable *next = table->next_owned;
        free(table->entries);
        free(table);
        table = next;
    }
    *data = (AbLuaData){0};
}

bool ab_data_literal_tables(const AbLuaChunk *chunk, size_t first_word,
                            size_t word_count, const unsigned *registers, size_t count,
                            size_t memory_limit, AbDataValue *tables, AbLuaData *output,
                            AbError *error) {
    if (!output) {
        return ab_fail(error, "Missing literal table output");
    }
    *output = (AbLuaData){.memory_limit = memory_limit};
    if (!count || count > 256 || !registers || !tables) {
        ab_lua_data_close(output);
        return ab_fail(error, "Invalid literal table selection storage");
    }
    memset(tables, 0, count * sizeof(*tables));
    if (!chunk || !chunk->input.data || chunk->root.upvalue_count ||
        chunk->root.parameter_count || !chunk->root.instructions.data ||
        chunk->root.instructions.size % 4 || !word_count ||
        first_word > chunk->root.instructions.size / 4 ||
        word_count > chunk->root.instructions.size / 4 - first_word) {
        ab_lua_data_close(output);
        return ab_fail(error, "Invalid closed literal table constructor bounds");
    }
    DataRecovery recovery = {.prototype = &chunk->root, .data = output, .error = error};
    size_t end_word = first_word + word_count;
    for (size_t index = first_word; index < end_word; ++index) {
        uint32_t word = ab_u32le(chunk->root.instructions.data + index * 4);
        unsigned opcode = word & 63;
        if (opcode != 0 && opcode != 1 && opcode != 2 && opcode != 3 && opcode != 6 &&
            opcode != 9 && opcode != 10 && opcode != 34) {
            ab_fail(error, "Nonliteral opcode %u in table constructor", opcode);
            goto release_data;
        }
        uint32_t list_block = (word >> 14) & 511;
        if (opcode == 34 && !list_block) {
            if (++index == end_word) {
                ab_fail(error, "Literal table list crosses constructor boundary");
                goto release_data;
            }
            list_block = ab_u32le(chunk->root.instructions.data + index * 4);
        }
        bool returned = false;
        if (!recover_word(&recovery, word, list_block, &returned)) {
            goto release_data;
        }
    }
    AbDataValue selected[256] = {0};
    for (size_t index = 0; index < count; ++index) {
        if (!register_value(&recovery, registers[index], selected + index) ||
            selected[index].kind != AB_DATA_TABLE) {
            ab_fail(error, "Literal constructor does not leave selected tables");
            goto release_data;
        }
    }
    memcpy(tables, selected, count * sizeof(*tables));
    output->returned = tables[0];
    return true;

release_data:
    ab_lua_data_close(output);
    return false;
}

bool ab_lua_data_literal_table_range(const AbLuaChunk *chunk, size_t first_word,
                                     size_t word_count, unsigned register_index,
                                     size_t memory_limit, AbLuaData *output,
                                     AbError *error) {
    AbDataValue selected;
    return ab_data_literal_tables(chunk, first_word, word_count, &register_index, 1,
                                  memory_limit, &selected, output, error);
}

bool ab_lua_data_literal_table(const AbLuaChunk *chunk, size_t word_count,
                               unsigned register_index, size_t memory_limit,
                               AbLuaData *output, AbError *error) {
    if (chunk && chunk->root.child_count) {
        if (output)
            *output = (AbLuaData){0};
        return ab_fail(error, "Closed literal constructor has child functions");
    }
    return ab_lua_data_literal_table_range(chunk, 0, word_count, register_index,
                                           memory_limit, output, error);
}

bool ab_lua_data_with_globals(const AbLuaChunk *chunk, size_t memory_limit,
                              const AbDataTable *globals, AbLuaData *output,
                              AbError *error) {
    if (!output) {
        return ab_fail(error, "Missing static Lua data output");
    }
    *output = (AbLuaData){.memory_limit = memory_limit};
    if (!chunk || !chunk->input.data || chunk->root.child_count ||
        chunk->root.upvalue_count || chunk->root.parameter_count) {
        return ab_fail(error,
                       "Static Lua recovery requires a closed constructor chunk");
    }
    output->globals = ab_data_table_new(output, error);
    if (!output->globals) {
        return false;
    }
    if (globals) {
        for (size_t index = 0; index < globals->count; ++index) {
            const AbDataEntry *entry = globals->entries + index;
            if (!ab_data_table_put(output, output->globals, entry->key, entry->value,
                                   error)) {
                ab_lua_data_close(output);
                return false;
            }
        }
    }
    DataRecovery recovery = {.prototype = &chunk->root,
                             .data = output,
                             .external_globals = globals,
                             .error = error};
    size_t count = chunk->root.instructions.size / 4;
    bool returned = false;
    for (size_t index = 0; index < count; ++index) {
        uint32_t word = ab_u32le(chunk->root.instructions.data + index * 4);
        uint32_t list_block = (word >> 14) & 511;
        if ((word & 63) == 34 && !list_block) {
            if (++index == count) {
                ab_lua_data_close(output);
                return ab_fail(error, "Static Lua list lacks an extended block index");
            }
            list_block = ab_u32le(chunk->root.instructions.data + index * 4);
        }
        if (!recover_word(&recovery, word, list_block, &returned)) {
            ab_lua_data_close(output);
            return false;
        }
        if (returned && index + 1 != count) {
            ab_lua_data_close(output);
            return ab_fail(error,
                           "Static Lua chunk contains unreachable trailing code");
        }
    }
    if (!returned) {
        ab_lua_data_close(output);
        return ab_fail(error, "Static Lua chunk has no final return");
    }
    return true;
}

bool ab_lua_data(const AbLuaChunk *chunk, size_t memory_limit, AbLuaData *output,
                 AbError *error) {
    return ab_lua_data_with_globals(chunk, memory_limit, NULL, output, error);
}
