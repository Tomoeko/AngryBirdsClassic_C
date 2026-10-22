#include "ab/theme_assets.h"

#include <string.h>

static AbDataValue index_key(size_t index) {
    float number = (float)index;
    AbDataValue key = {.kind = AB_DATA_NUMBER};
    memcpy(&key.bits, &number, sizeof(number));
    return key;
}

static bool dense_strings(const AbDataTable *table, size_t capacity, AbError *error) {
    if (!table || table->count > table->capacity || table->count > capacity ||
        table->count > 16777216 || (table->count && !table->entries)) {
        return ab_fail(error, "Invalid theme asset list or capacity");
    }
    for (size_t index = 1; index <= table->count; ++index) {
        const AbDataValue *value = ab_data_get(table, index_key(index));
        if (!value || value->kind != AB_DATA_STRING ||
            (value->string.size && !value->string.data)) {
            return ab_fail(error, "Theme assets require dense string arrays");
        }
    }
    return true;
}

bool ab_theme_assets_init(AbDataTable *current, AbSpan *scratch, size_t capacity,
                          AbThemeAssets *output, AbError *error) {
    if (!output || !scratch || !capacity || capacity > SIZE_MAX / 2 ||
        !dense_strings(current, capacity, error)) {
        return ab_fail(error, "Invalid theme asset transition storage");
    }
    *output = (AbThemeAssets){.current = current,
                              .loads = scratch,
                              .releases = scratch + capacity,
                              .capacity = capacity};
    for (size_t index = 0; index < 2; ++index) {
        output->wrapped[index] =
            (AbDataTable){output->wrapped_entries[index], 0, 2, NULL};
    }
    return true;
}

static AbSpan member(const AbDataTable *table, size_t index) {
    return ab_data_get(table, index_key(index + 1))->string;
}

static bool contains(const AbDataTable *table, AbSpan name) {
    for (size_t index = 0; index < table->count; ++index) {
        AbSpan other = member(table, index);
        if (name.size == other.size &&
            (!name.size || memcmp(name.data, other.data, name.size) == 0)) {
            return true;
        }
    }
    return false;
}

static size_t difference(const AbDataTable *left, const AbDataTable *right,
                         AbSpan *output) {
    size_t count = 0;
    for (size_t index = 0; index < left->count; ++index) {
        AbSpan name = member(left, index);
        if (!contains(right, name)) {
            output[count++] = name;
        }
    }
    return count;
}

static bool dispatch(AbThemeAssets *assets, const AbSpan *groups, size_t count,
                     bool loading, AbThemeAssetHost host, void *context,
                     AbError *error) {
    for (size_t index = 0; index < count; ++index) {
        if (!host(context, groups[index], loading, error)) {
            assets->failed = true;
            assets->busy = false;
            return false;
        }
    }
    return true;
}

bool ab_theme_assets_transition(AbThemeAssets *assets, AbDataValue graphic_set,
                                bool show_editor, AbThemeAssetHost host, void *context,
                                AbError *error) {
    if (!assets || !assets->current || assets->busy || assets->failed || !host ||
        !assets->loads || !assets->releases ||
        !dense_strings(assets->current, assets->capacity, error)) {
        return ab_fail(error, "Invalid or failed theme asset transition");
    }
    AbDataTable *target;
    if (graphic_set.kind == AB_DATA_STRING) {
        if ((graphic_set.string.size && !graphic_set.string.data) ||
            assets->capacity < (show_editor ? 2u : 1u)) {
            return ab_fail(error, "Theme asset string exceeds storage boundary");
        }
        size_t index = assets->current == assets->wrapped ? 1 : 0;
        target = assets->wrapped + index;
        target->entries[0] = (AbDataEntry){index_key(1), graphic_set};
        target->count = 1;
    } else if (graphic_set.kind == AB_DATA_TABLE &&
               dense_strings(graphic_set.table, assets->capacity, error)) {
        target = graphic_set.table;
        if (show_editor &&
            (target->count >= assets->capacity || target->count >= target->capacity ||
             target->count >= 16777216 || !target->entries)) {
            return ab_fail(error, "Theme editor append exceeds list capacity");
        }
    } else {
        return ab_fail(error, "Unsupported theme graphic-set value");
    }
    assets->busy = true;
    if (show_editor) {
        static const uint8_t editor_group[] = "INGAME_EPISODE_RED_BIRD";
        target->entries[target->count] =
            (AbDataEntry){index_key(target->count + 1),
                          {.kind = AB_DATA_STRING,
                           .string = {editor_group, sizeof(editor_group) - 1}}};
        ++target->count;
    }
    size_t load_count = difference(target, assets->current, assets->loads);
    size_t release_count = difference(assets->current, target, assets->releases);
    if (!dispatch(assets, assets->releases, release_count, false, host, context,
                  error)) {
        return false;
    }
    assets->current = target;
    if (!dispatch(assets, assets->loads, load_count, true, host, context, error)) {
        return false;
    }
    assets->busy = false;
    return true;
}
