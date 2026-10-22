#ifndef AB_ASSET_LOAD_LISTS_H
#define AB_ASSET_LOAD_LISTS_H

#include "ab/lua_data.h"
#include "ab/zip.h"

typedef struct AbAssetLoadLists AbAssetLoadLists;

/* Own a bounded lazy cache of assets/data/images/<profile>/loadlist.lua.
 * Borrow an open archive until close and copy the supplied 32-byte resource key
 * only into this owner; it is cleared on close. Profiles/data/path storage count
 * toward allocation_limit, while codec workspace has its own bounds. Fixed
 * profile capacity preserves returned views. No original code/VM, image decoding,
 * GPU work or profile choice occurs. Output is NULL on failure. */
bool ab_asset_load_lists_open(const AbZip *archive, AbSpan resource_key,
                              size_t profile_capacity, size_t allocation_limit,
                              AbAssetLoadLists **output, AbError *error);
void ab_asset_load_lists_close(AbAssetLoadLists *lists);
size_t ab_asset_load_lists_bytes(const AbAssetLoadLists *lists);
size_t ab_asset_load_lists_count(const AbAssetLoadLists *lists);

/* Query the immutable group-name table for one profile, decoding once on a miss.
 * This projects loadLoadList's merged group lookups from its static assetLoadList
 * data; internal Lua cache-table layout/traversal and mutable globals are separate.
 * The view and all strings remain stable until close. Profile names are copied
 * and must not contain NUL bytes; paths are joined without normalization. No
 * fallback directory is chosen. Failure preserves output and existing cache.
 * Calls and close must be serialized; input/output must not alias owned state. */
bool ab_asset_load_lists_get(AbAssetLoadLists *lists, AbSpan profile,
                             const AbDataTable **output, AbError *error);

#endif
