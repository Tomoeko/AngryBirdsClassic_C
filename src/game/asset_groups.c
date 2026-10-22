#include "ab/asset_groups.h"
#include "ab/asset_bundles.h"

#include <string.h>

static bool valid_span(AbSpan value) {
    return !value.size || value.data;
}

static bool valid_state(const AbAssetGroups *state) {
    return state && !state->failed && state->count <= state->capacity &&
           state->capacity <= SIZE_MAX / sizeof(*state->loaded) &&
           state->scratch_capacity <= SIZE_MAX / sizeof(*state->scratch) &&
           (!state->capacity || state->loaded) &&
           (!state->scratch_capacity || state->scratch);
}

static bool equal(AbSpan first, AbSpan second) {
    return first.size == second.size &&
           (!first.size || memcmp(first.data, second.data, first.size) == 0);
}

static bool valid_table(const AbDataTable *table) {
    return table && table->count <= table->capacity &&
           (!table->count || table->entries) && table->count < UINT32_C(16777216);
}

static const AbDataValue *array_value(const AbDataTable *table, size_t index) {
    float numeric = (float)index;
    uint32_t bits;
    memcpy(&bits, &numeric, sizeof(bits));
    return ab_data_get(table, (AbDataValue){.kind = AB_DATA_NUMBER, .bits = bits});
}

static bool dispatch_resources(const AbDataTable *groups, AbAssetCall call,
                               bool loading, AbAssetHost host, void *context,
                               AbError *error) {
    if (!valid_table(groups)) {
        return ab_fail(error, "Invalid asset folder group table");
    }
    const AbDataValue *group = ab_data_get(
        groups, (AbDataValue){.kind = AB_DATA_STRING, .string = call.group});
    if (!group || group->kind == AB_DATA_NIL ||
        (group->kind == AB_DATA_BOOLEAN && !group->bits)) {
        return true;
    }
    if (group->kind != AB_DATA_TABLE || !valid_table(group->table)) {
        return ab_fail(error, "Invalid asset group resource array");
    }
    const AbDataTable *resources = group->table;
    for (size_t index = 1; index <= resources->count + 1; ++index) {
        const AbDataValue *row = array_value(resources, index);
        if (!row || row->kind == AB_DATA_NIL) {
            return true;
        }
        if (row->kind != AB_DATA_TABLE || !valid_table(row->table)) {
            return ab_fail(error, "Invalid asset resource row");
        }
        const AbDataValue *kind = array_value(row->table, 2);
        if (!kind || kind->kind != AB_DATA_NUMBER) {
            continue;
        }
        float numeric_kind;
        memcpy(&numeric_kind, &kind->bits, sizeof(numeric_kind));
        if (numeric_kind != 0 && numeric_kind != 1) {
            continue;
        }
        const AbDataValue *filename = array_value(row->table, 1);
        if (!filename || filename->kind != AB_DATA_STRING ||
            !valid_span(filename->string)) {
            return ab_fail(error, "Asset resource filename is not a string");
        }
        call.filename = filename->string;
        call.operation =
            numeric_kind == 0
                ? (loading ? AB_ASSET_CREATE_SHEET : AB_ASSET_RELEASE_SHEET)
                : (loading ? AB_ASSET_CREATE_COMPOSITES : AB_ASSET_RELEASE_COMPOSITES);
        if (!host(context, &call, error)) {
            return false;
        }
    }
    return ab_fail(error, "Asset resource array exceeds its entry count");
}

bool ab_asset_group_manage(AbAssetGroups *state, AbSpan group, bool loading,
                           AbSpan image_root, AbAssetHost host, void *context,
                           AbError *error) {
    if (!valid_state(state) || !valid_span(group) || !valid_span(image_root) || !host) {
        return ab_fail(error, "Invalid asset group state/input");
    }
    bool listed = false;
    for (size_t index = 0; index < state->count; ++index) {
        if (!valid_span(state->loaded[index])) {
            return ab_fail(error, "Invalid loaded asset group name");
        }
        listed = listed || equal(state->loaded[index], group);
    }
    if ((loading && !listed && state->count == state->capacity) ||
        (!loading && state->scratch_capacity < state->count)) {
        return ab_fail(error, "Asset group list/snapshot capacity exceeded");
    }
    size_t saved_count = state->count;
    if (loading) {
        if (!listed) {
            state->loaded[state->count++] = group;
        }
    } else if (saved_count) {
        memcpy(state->scratch, state->loaded, saved_count * sizeof(*state->loaded));
    }
    AbAssetCall call = {
        .operation = AB_ASSET_SELECT_PROFILE, .group = group, .image_root = image_root};
    if (!host(context, &call, error)) {
        state->failed = true;
        return false;
    }
    if (!valid_span(call.profile)) {
        state->failed = true;
        return ab_fail(error, "Asset group profile selection failed");
    }
    call.operation = AB_ASSET_LOAD_LIST;
    if (!host(context, &call, error) ||
        !dispatch_resources(call.groups, call, loading, host, context, error)) {
        state->failed = true;
        return false;
    }
    if (!loading) {
        for (size_t index = 0; index < saved_count; ++index) {
            if (equal(state->scratch[index], group)) {
                --saved_count;
                memmove(state->scratch + index, state->scratch + index + 1,
                        (saved_count - index) * sizeof(*state->scratch));
            }
        }
        if (saved_count) {
            memcpy(state->loaded, state->scratch, saved_count * sizeof(*state->loaded));
        }
        state->count = saved_count;
    }
    return true;
}

typedef struct {
    AbAssetGroups *state;
    AbSpan image_root;
    AbAssetHost host;
    void *context;
} AssetBatch;

static bool manage_expanded(void *context, AbSpan group, bool loading, AbError *error) {
    AssetBatch *batch = context;
    return ab_asset_group_manage(batch->state, group, loading, batch->image_root,
                                 batch->host, batch->context, error);
}

bool ab_asset_group_manage_many(AbAssetGroups *state, const AbDataTable *requested,
                                const AbDataTable *profiles, bool loading,
                                AbSpan image_root, AbAssetHost host, void *context,
                                AbError *error) {
    if (!valid_state(state) || !valid_span(image_root) || !host) {
        return ab_fail(error, "Invalid asset bundle management state/input");
    }
    AssetBatch batch = {state, image_root, host, context};
    if (!ab_asset_bundle_dispatch(requested, profiles, loading, manage_expanded, &batch,
                                  error)) {
        state->failed = true;
        return false;
    }
    return true;
}

bool ab_asset_resource_path(const AbAssetCall *call, char *output, size_t capacity,
                            size_t *length, AbError *error) {
    if (!call || !output || !length || capacity < 3) {
        return ab_fail(error, "Invalid asset resource path output");
    }
    const AbSpan parts[] = {call->image_root, call->profile, call->filename};
    size_t size = 2;
    for (size_t index = 0; index < 3; ++index) {
        if (!valid_span(parts[index]) || parts[index].size > capacity - 1 - size ||
            (parts[index].size && memchr(parts[index].data, 0, parts[index].size))) {
            return ab_fail(error, "Asset resource path exceeds output/domain bounds");
        }
        size += parts[index].size;
    }
    size_t offset = 0;
    for (size_t index = 0; index < 3; ++index) {
        if (index) {
            output[offset++] = '/';
        }
        if (parts[index].size) {
            memcpy(output + offset, parts[index].data, parts[index].size);
            offset += parts[index].size;
        }
    }
    output[offset] = 0;
    *length = size;
    return true;
}
