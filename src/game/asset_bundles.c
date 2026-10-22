#include "ab/asset_bundles.h"

#include <string.h>

static bool valid_table(const AbDataTable *table) {
    return table && table->count <= table->capacity &&
           (!table->count || table->entries) && table->count < UINT32_C(16777216);
}

static const AbDataValue *array_value(const AbDataTable *table, size_t index) {
    float number = (float)index;
    AbDataValue key = {.kind = AB_DATA_NUMBER};
    memcpy(&key.bits, &number, sizeof(key.bits));
    return ab_data_get(table, key);
}

static bool visit_array(const AbDataTable *array, bool loading,
                        AbAssetGroupVisitor visit, void *context, AbError *error) {
    for (size_t index = 1; index <= array->count + 1; ++index) {
        const AbDataValue *value = array_value(array, index);
        if (!value || value->kind == AB_DATA_NIL) {
            return true;
        }
        if (value->kind != AB_DATA_STRING ||
            (value->string.size && !value->string.data)) {
            return ab_fail(error, "Asset bundle group is not a typed string");
        }
        if (!visit(context, value->string, loading, error)) {
            return false;
        }
    }
    return ab_fail(error, "Asset bundle array exceeds its entry count");
}

bool ab_asset_bundle_dispatch(const AbDataTable *requested, const AbDataTable *profiles,
                              bool loading, AbAssetGroupVisitor visit, void *context,
                              AbError *error) {
    if (!valid_table(requested) || !valid_table(profiles) || !visit) {
        return ab_fail(error, "Invalid asset bundle dispatch tables/visitor");
    }
    for (size_t index = 1; index <= requested->count + 1; ++index) {
        const AbDataValue *group = array_value(requested, index);
        if (!group || group->kind == AB_DATA_NIL) {
            return true;
        }
        if (group->kind != AB_DATA_STRING ||
            (group->string.size && !group->string.data)) {
            return ab_fail(error, "Requested asset group is not a typed string");
        }
        const AbDataValue *bundle = ab_data_get(profiles, *group);
        if (!bundle || bundle->kind == AB_DATA_NIL) {
            if (!visit(context, group->string, loading, error)) {
                return false;
            }
        } else if (bundle->kind != AB_DATA_TABLE || !valid_table(bundle->table)) {
            return ab_fail(error, "Named asset bundle is not a table");
        } else if (!visit_array(bundle->table, loading, visit, context, error)) {
            return false;
        }
    }
    return ab_fail(error, "Requested asset group array exceeds its entry count");
}
