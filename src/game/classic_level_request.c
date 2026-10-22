#include "ab/classic_level_request.h"

#include <math.h>
#include <string.h>

static AbDataValue number(float value) {
    AbDataValue result = {.kind = AB_DATA_NUMBER};
    memcpy(&result.bits, &value, sizeof(value));
    return result;
}

static AbDataValue get(const AbDataTable *table, AbDataValue key) {
    const AbDataValue *value = ab_data_get(table, key);
    return value ? *value : (AbDataValue){0};
}

static AbDataValue field(const AbDataTable *table, const char *key) {
    return get(table, (AbDataValue){.kind = AB_DATA_STRING,
                                    .string = {(const uint8_t *)key, strlen(key)}});
}

static bool text_equal(AbDataValue left, AbDataValue right) {
    return left.kind == AB_DATA_STRING && right.kind == AB_DATA_STRING &&
           left.string.size == right.string.size &&
           (!left.string.size ||
            (left.string.data && right.string.data &&
             !memcmp(left.string.data, right.string.data, left.string.size)));
}

static bool table(AbDataValue value, const AbDataTable **output, AbError *error) {
    if (value.kind != AB_DATA_TABLE || !value.table ||
        value.table->count > value.table->capacity ||
        (value.table->count && !value.table->entries))
        return ab_fail(error, "Level request indexes an invalid catalog table");
    *output = value.table;
    return true;
}

static bool index_value(AbDataValue value, float *output, AbError *error) {
    float result = 0;
    memcpy(&result, &value.bits, sizeof(result));
    if (value.kind != AB_DATA_NUMBER || !isfinite(result) || result < 1 ||
        result > 16384 || truncf(result) != result)
        return ab_fail(error, "Level request requires positive catalog indexes");
    *output = result;
    return true;
}

static size_t length(const AbDataTable *array) {
    size_t count = 0;
    while (count < array->count &&
           get(array, number((float)(count + 1))).kind != AB_DATA_NIL)
        ++count;
    return count;
}

static bool level_number(const AbDataTable *episodes, AbDataValue name, float *output,
                         AbError *error) {
    float result = 0;
    for (size_t episode = 0; episode < episodes->count; ++episode) {
        AbDataValue value = episodes->entries[episode].value;
        if (value.kind == AB_DATA_NIL)
            continue;
        const AbDataTable *record = NULL, *pages = NULL;
        if (!table(value, &record, error) ||
            !table(field(record, "pages"), &pages, error))
            return false;
        size_t page_count = length(pages);
        for (size_t page = 1; page <= page_count; ++page) {
            const AbDataTable *current = NULL, *levels = NULL;
            if (!table(get(pages, number((float)page)), &current, error) ||
                !table(field(current, "levels"), &levels, error))
                return false;
            size_t count = length(levels);
            for (size_t level = 1; level <= count; ++level) {
                if (!table(get(levels, number((float)level)), &record, error))
                    return false;
                if (!text_equal(field(record, "name"), name))
                    continue;
                if (result != 0 && result != (float)level)
                    return ab_fail(error,
                                   "Repeated level name needs native episode order");
                result = (float)level;
            }
        }
    }
    *output = result;
    return true;
}

static bool path_piece(AbDataValue value, bool folder) {
    if (value.kind != AB_DATA_STRING || !value.string.size || !value.string.data)
        return false;
    for (size_t index = 0; index < value.string.size; ++index) {
        uint8_t byte = value.string.data[index];
        if ((byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z') ||
            (byte >= '0' && byte <= '9') || byte == '_' || byte == '-' ||
            (folder && byte == '/' && index + 1 < value.string.size &&
             (!index || value.string.data[index - 1] != '/')))
            continue;
        return false;
    }
    return true;
}

static bool resource_path(AbDataValue folder, AbDataValue name, char *output,
                          size_t capacity, AbError *error) {
    const char prefix[] = "assets/data";
    const char level_folder[] = "/levels/";
    const char suffix[] = ".lua";
    if (!path_piece(folder, true) || !path_piece(name, false) ||
        folder.string.size <= sizeof(level_folder) - 1 ||
        memcmp(folder.string.data, level_folder, sizeof(level_folder) - 1))
        return ab_fail(error, "Level catalog path needs a supported resource name");
    size_t used = sizeof(prefix) - 1;
    if (used >= capacity || folder.string.size >= capacity - used)
        return ab_fail(error, "Level resource folder exceeds its path bound");
    memcpy(output, prefix, used);
    memcpy(output + used, folder.string.data, folder.string.size);
    used += folder.string.size;
    output[used++] = '/';
    if (name.string.size >= capacity - used ||
        sizeof(suffix) > capacity - used - name.string.size)
        return ab_fail(error, "Level resource name exceeds its path bound");
    memcpy(output + used, name.string.data, name.string.size);
    memcpy(output + used + name.string.size, suffix, sizeof(suffix));
    return true;
}

bool ab_classic_level_request_resolve(const AbDataTable *episodes,
                                      const AbDataTable *metadata,
                                      AbClassicLevelRequest *output, AbError *error) {
    if (!output || !episodes || episodes->count > episodes->capacity ||
        (episodes->count && !episodes->entries) || !metadata ||
        metadata->count > metadata->capacity || (metadata->count && !metadata->entries))
        return ab_fail(error, "Invalid level request input/output");
    AbClassicLevelRequest result = {.episode_id = field(metadata, "episode"),
                                    .page_number = field(metadata, "page"),
                                    .level_index = field(metadata, "level"),
                                    .name = field(metadata, "levelName")};
    const AbDataTable *pages = NULL, *levels = NULL;
    float page = 0, level = 0;
    if (!index_value(result.page_number, &page, error) ||
        !index_value(result.level_index, &level, error) ||
        !table(get(episodes, result.episode_id), &result.episode, error) ||
        !table(field(result.episode, "pages"), &pages, error) ||
        !table(get(pages, result.page_number), &result.page, error) ||
        !table(field(result.page, "levels"), &levels, error) ||
        !table(get(levels, result.level_index), &result.level, error))
        return false;
    if (!text_equal(result.name, field(result.level, "name")))
        return ab_fail(error, "Level request name differs from canonical metadata");
    result.world_number = field(result.page, "world_number");
    float prefix = 0;
    for (float index = 1; index <= page - 1; index += 1)
        prefix += (float)length(levels);
    result.current_level_number = prefix + level;
    if (!level_number(episodes, result.name, &result.current_level_number_in_theme,
                      error) ||
        !resource_path(field(result.page, "folder_name"), result.name,
                       result.resource_path, sizeof(result.resource_path), error))
        return false;
    *output = result;
    return true;
}
