#ifndef AB_ASSET_GROUPS_H
#define AB_ASSET_GROUPS_H

#include "ab/lua_data.h"

typedef struct {
    AbSpan *loaded;
    size_t count, capacity;
    AbSpan *scratch;
    size_t scratch_capacity;
    bool failed;
} AbAssetGroups;

typedef enum {
    AB_ASSET_SELECT_PROFILE = 1,
    AB_ASSET_LOAD_LIST,
    AB_ASSET_CREATE_SHEET,
    AB_ASSET_RELEASE_SHEET,
    AB_ASSET_CREATE_COMPOSITES,
    AB_ASSET_RELEASE_COMPOSITES,
} AbAssetOperation;

typedef struct {
    AbAssetOperation operation;
    AbSpan group, profile, image_root, filename;
    /* LOAD_LIST replies with the selected folder's group table, after its
     * native load-list cache/merge operation. SELECT_PROFILE replies in profile. */
    const AbDataTable *groups;
} AbAssetCall;

typedef bool (*AbAssetHost)(void *context, AbAssetCall *call, AbError *error);

/* manageAssets' typed group-list/resource dispatch. Loading appends a previously
 * unlisted group before selecting/loading its profile; repeated loads still
 * dispatch resource calls. Unloading snapshots the list into scratch first,
 * dispatches resources, then removes matching entries with original forward
 * table.remove iteration (adjacent duplicates can remain). Missing group data
 * performs no resource calls but retains these list effects. Resource rows use
 * numeric keys 1=filename and 2=kind: zero is a sheet, one a composite set;
 * other kinds are ignored without reading the filename. Numeric-string filename
 * coercion is outside this boundary. Profile selection/cache ownership, bundle
 * expansion, diagnostics and actual resource/GPU work belong to the host.
 *
 * Borrow disjoint fixed arrays and names; new names must outlive their loaded
 * membership. Host data stays stable through this pass and must not mutate or
 * reenter this state. No allocation. Invalid inputs/capacity reject before work;
 * scratch may change, and later errors retain prior list/host effects and mark
 * state failed. Stop/discard on those errors. */
bool ab_asset_group_manage(AbAssetGroups *state, AbSpan group, bool loading,
                           AbSpan image_root, AbAssetHost host, void *context,
                           AbError *error);

/* Expand typed request/profile tables once, after the host has resolved
 * platform.Profiles, then manage each resulting group above. Borrow all data
 * through dispatch and retain names for loaded membership. No recursive bundle
 * expansion or allocation. Failure retains prior effects and marks state failed;
 * requireFile and loadAssets' global aux are outside this boundary. */
bool ab_asset_group_manage_many(AbAssetGroups *state, const AbDataTable *requested,
                                const AbDataTable *profiles, bool loading,
                                AbSpan image_root, AbAssetHost host, void *context,
                                AbError *error);

/* Join the three resource path spans with literal '/' separators and a trailing
 * NUL for an archive/platform boundary. No normalization or allocation. Typed
 * spans must not contain NUL bytes and must be disjoint from output/length.
 * Invalid input preserves output/length. */
bool ab_asset_resource_path(const AbAssetCall *call, char *output, size_t capacity,
                            size_t *length, AbError *error);

/* Derive Resources create/release sheet/composite keys by applying PathName
 * basename extraction twice. Support forward-slash paths up to 255 bytes, including the
 * native dot-sequence normalization quirks; drive/backslash paths are outside
 * this APK-facing boundary. The key excludes up to two extensions. It may be
 * empty. No allocation; append NUL and preserve output/length on failure.
 * Input/output/length must be disjoint. This does not resolve/open a file. */
bool ab_asset_sheet_key(AbSpan path, char *output, size_t capacity, size_t *length,
                        AbError *error);

#endif
