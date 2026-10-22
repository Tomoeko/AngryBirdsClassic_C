#ifndef AB_ASSET_BUNDLES_H
#define AB_ASSET_BUNDLES_H

#include "ab/lua_data.h"

typedef bool (*AbAssetGroupVisitor)(void *context, AbSpan group, bool loading,
                                    AbError *error);

/* Dispatch loadAssets/releaseAssets after the host's requireFile boundary has
 * resolved platform.Profiles. Visit requested numeric entries until the first
 * nil; a non-nil named profile expands its numeric entries exactly once. Empty
 * bundles dispatch nothing; duplicates and self names remain ordinary visits.
 * Borrow immutable tables/strings through all visits. Only typed strings/tables
 * are supported, without VM coercion, metatables or loadAssets' global aux.
 * No allocation. Failures retain completed visits; callers must stop/discard any
 * partially changed host state. Neither resource loading nor requireFile runs. */
bool ab_asset_bundle_dispatch(const AbDataTable *requested, const AbDataTable *profiles,
                              bool loading, AbAssetGroupVisitor visit, void *context,
                              AbError *error);

#endif
