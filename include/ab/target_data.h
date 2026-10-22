#ifndef AB_TARGET_DATA_H
#define AB_TARGET_DATA_H

#include "ab/lua_data.h"
#include "ab/target_resources.h"

typedef struct {
    AbBuffer bytes;
    AbLuaChunk chunk;
    AbLuaData data;
} AbTargetData;

/* Decode a packed APK data chunk and recover its restricted static constructors.
 * No Lua VM or dynamic script calls run. Imported globals and their strings stay
 * borrowed and must outlive this owner. Decoded bytes, chunk structures and new
 * data tables are owned until close. The archive/key are borrowed only during
 * open. allocation_limit bounds retained data and each payload stage; codec
 * workspace remains separately bounded. Output starts empty, including on failure. */
bool ab_target_data_open(const AbZip *archive, const char *path, AbSpan key,
                         size_t allocation_limit, const AbDataTable *globals,
                         AbTargetData *output, AbError *error);
void ab_target_data_close(AbTargetData *data);

#endif
