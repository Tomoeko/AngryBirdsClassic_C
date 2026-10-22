#ifndef AB_LUA_TEXT_H
#define AB_LUA_TEXT_H

#include "ab/lua_data.h"

typedef struct {
    AbLuaData data;
    AbBuffer text;
} AbLuaTextData;

/* Serialized literal tables only: global assignments, nested constructors,
 * finite numbers, strings, booleans, and nil, with named, bracketed, or list keys.
 * No calls, expressions, comments, or Lua VM. Decimal conversion requires the
 * C numeric locale and round-to-nearest floating-point mode.
 * Successful output owns its text and tables; input can be released immediately.
 * The memory budget includes the text copy, tables, and entry allocations.
 */
bool ab_lua_text_data(AbSpan input, size_t memory_limit, AbLuaTextData *output,
                      AbError *error);
void ab_lua_text_data_close(AbLuaTextData *data);

#endif
