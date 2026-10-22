#ifndef AB_DATA_INTERNAL_H
#define AB_DATA_INTERNAL_H

#include "ab/lua_data.h"

AbDataTable *ab_data_table_new(AbLuaData *data, AbError *error);
bool ab_data_record_layout(AbLuaData *data, const AbDataTable *table,
                           unsigned array_hint, unsigned hash_hint, AbError *error);
void ab_data_layout_nil_assignment(AbLuaData *data, const AbDataTable *table);
bool ab_data_table_put(AbLuaData *data, AbDataTable *table, AbDataValue key,
                       AbDataValue value, AbError *error);

/* Resource-owner variant of the literal window reader. Export several tables
 * still pending assignment in a constructor. Every selected register must be
 * defined by that same window and contain a table; no earlier register imports,
 * globals, calls or control flow. Values borrow data until close. */
bool ab_data_literal_tables(const AbLuaChunk *chunk, size_t first_word,
                            size_t word_count, const unsigned *registers, size_t count,
                            size_t memory_limit, AbDataValue *tables, AbLuaData *data,
                            AbError *error);

#endif
