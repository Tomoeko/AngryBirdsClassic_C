#ifndef AB_LUA_DATA_JSON_H
#define AB_LUA_DATA_JSON_H

#include "ab/lua_data.h"

#include <stdio.h>

/* Research export with explicit key types and original binary32 number bits. */
bool ab_lua_data_json(const AbLuaData *data, FILE *file, AbError *error);

#endif
