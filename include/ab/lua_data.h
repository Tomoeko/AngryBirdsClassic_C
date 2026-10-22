#ifndef AB_LUA_DATA_H
#define AB_LUA_DATA_H

#include "ab/lua_chunk.h"

typedef enum {
    AB_DATA_NIL,
    AB_DATA_BOOLEAN,
    AB_DATA_NUMBER,
    AB_DATA_STRING,
    AB_DATA_TABLE,
    /* Borrowed identity of an owned C runtime instance. Literal resource readers
     * never produce this kind; event receivers access the actual typed owner. */
    AB_DATA_OBJECT,
} AbDataKind;

typedef struct AbDataTable AbDataTable;

typedef struct {
    AbDataKind kind;
    uint32_t bits;
    AbSpan string;
    union {
        AbDataTable *table;
        const void *object;
    };
} AbDataValue;

typedef struct {
    AbDataValue key;
    AbDataValue value;
} AbDataEntry;

struct AbDataTable {
    AbDataEntry *entries;
    size_t count;
    size_t capacity;
    AbDataTable *next_owned;
};

typedef struct AbDataTableLayout {
    const AbDataTable *table;
    size_t array_hint;
    size_t hash_capacity;
    bool no_nil_assignments;
    struct AbDataTableLayout *next;
} AbDataTableLayout;

typedef struct {
    /* Strings and imported tables are borrowed. The owned tables, entries and
     * layout records are released by ab_lua_data_close. */
    AbDataTable *globals;
    AbDataTable *owned;
    AbDataValue returned;
    size_t allocated_bytes;
    size_t memory_limit;
    size_t table_count;
    AbDataTableLayout *layouts;
} AbLuaData;

/* Restricted static constructor recovery. No calls, loops, branches, or VM. */
bool ab_lua_data(const AbLuaChunk *chunk, size_t memory_limit, AbLuaData *output,
                 AbError *error);
/* Recover a table held in register_index after word_count constructor words.
 * Only literal loads, moves, new tables and local table lookups/assignments are
 * accepted; no global access, arithmetic, calls, returns or control flow. Extended
 * SETLIST words count toward word_count. Trailing words are not interpreted. The caller
 * validates their resource-specific registration shape separately. The selected
 * table is output.returned; strings borrow chunk bytes until close. On failure
 * output is empty. This is a restricted data reader, not a script evaluator. */
bool ab_lua_data_literal_table(const AbLuaChunk *chunk, size_t word_count,
                               unsigned register_index, size_t memory_limit,
                               AbLuaData *output, AbError *error);
/* The same restricted reader over one explicit root instruction window. A
 * window may follow behavior/closure definitions, but starts with no defined
 * registers and may not read globals, parameters, upvalues or earlier state.
 * Children outside the window are never entered. Resource-specific owners must
 * establish the window and validate surrounding registration independently.
 * first_word/word_count include extended SETLIST words; failure empties output. */
bool ab_lua_data_literal_table_range(const AbLuaChunk *chunk, size_t first_word,
                                     size_t word_count, unsigned register_index,
                                     size_t memory_limit, AbLuaData *output,
                                     AbError *error);
/* Initialize globals by shallow copy. Output retains imported bindings as well
 * as the chunk's assignments. Imported graphs and string bytes must outlive
 * output; assignments into imported tables are rejected. Only new data is owned.
 */
bool ab_lua_data_with_globals(const AbLuaChunk *chunk, size_t memory_limit,
                              const AbDataTable *globals, AbLuaData *output,
                              AbError *error);
const AbDataValue *ab_data_get(const AbDataTable *table, AbDataValue key);
/* Initial NEWTABLE allocation metadata, owned by data. Imported tables and
 * synthetic/global tables have no record. Hash capacity is rounded from the
 * encoded constructor hint; it does not simulate later growth. no_nil_assignments
 * becomes false on any nil assignment; non-nil replacements retain key slots. The
 * caller must also verify string-only keys and sufficient original capacity before
 * ordering.
 */
const AbDataTableLayout *ab_lua_table_layout(const AbLuaData *data,
                                             const AbDataTable *table);
void ab_lua_data_close(AbLuaData *data);

#endif
