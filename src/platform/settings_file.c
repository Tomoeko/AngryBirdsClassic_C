#include "ab/settings_file.h"

#include "ab/aes.h"
#include "ab/lua_text.h"
#include "ab/lua_text_write.h"
#include "console_common/support/atomic_file.h"
#include "console_common/support/regular_file.h"

#include <stdlib.h>
#include <string.h>

struct AbSettingsFile {
    char *path;
    uint8_t key[32];
    AbBuffer storage;
    AbLuaTextData restored;
    bool loaded, imported, failed;
};

void ab_settings_file_close(AbSettingsFile *file) {
    if (!file)
        return;
    ab_lua_text_data_close(&file->restored);
    ab_buffer_free(&file->storage);
    free(file->path);
    memset(file->key, 0, sizeof(file->key));
    free(file);
}

bool ab_settings_file_open(const char *path, AbSpan key, AbSettingsFileLimits limits,
                           AbSettingsFile **output, AbError *error) {
    if (!output)
        return ab_fail(error, "Missing settings-file owner output");
    *output = NULL;
    if (!path || !*path || !key.data || key.size != 32 || !limits.plaintext_bytes ||
        limits.plaintext_bytes > SIZE_MAX - 16 || !limits.parsed_bytes)
        return ab_fail(error, "Invalid settings-file configuration");
    AbSettingsFile *file = calloc(1, sizeof(*file));
    if (!file)
        return ab_fail(error, "Cannot allocate settings-file owner");
    AbBuffer encrypted = {0}, plaintext = {0};
    bool okay = false;
    size_t path_size = strlen(path);
    file->path = malloc(path_size + 1);
    file->storage.data = malloc(limits.plaintext_bytes);
    if (!file->path || !file->storage.data) {
        ab_fail(error, "Cannot allocate settings-file path/storage");
        goto release;
    }
    memcpy(file->path, path, path_size + 1);
    memcpy(file->key, key.data, sizeof(file->key));
    file->storage.size = limits.plaintext_bytes;
    CcRegularFileStatus status = cc_regular_file_read_bytes(
        path, 16, limits.plaintext_bytes + 16, &encrypted.data, &encrypted.size);
    if (status == CC_REGULAR_FILE_MISSING) {
        okay = true;
    } else if (status != CC_REGULAR_FILE_OK) {
        ab_fail(error, "Cannot read a bounded regular settings file");
    } else if (ab_asset_decrypt((AbSpan){encrypted.data, encrypted.size}, key,
                                &plaintext, error)) {
        if (plaintext.size > limits.plaintext_bytes)
            ab_fail(error, "Decrypted settings exceed the plaintext limit");
        else if (ab_lua_text_data((AbSpan){plaintext.data, plaintext.size},
                                  limits.parsed_bytes, &file->restored, error)) {
            file->loaded = true;
            okay = true;
        }
    }
release:
    ab_buffer_free(&encrypted);
    ab_buffer_free(&plaintext);
    if (!okay) {
        ab_settings_file_close(file);
        return false;
    }
    *output = file;
    return true;
}

bool ab_settings_file_loaded(const AbSettingsFile *file) {
    return file && file->loaded;
}

static bool array(const AbDataTable *table) {
    if (!table || !table->count)
        return false;
    for (size_t index = 0; index < table->count; ++index) {
        AbDataValue key = table->entries[index].key;
        float number = 0;
        memcpy(&number, &key.bits, sizeof(number));
        if (key.kind != AB_DATA_NUMBER || number != (float)(index + 1))
            return false;
    }
    return true;
}

static bool import_table(AbSettingsStatistics *settings, const AbDataTable *table,
                         AbSpan path[3], size_t depth, AbError *error) {
    if (!table || table->count > table->capacity || (table->count && !table->entries))
        return ab_fail(error, "Invalid restored settings table");
    bool dense = array(table);
    if (dense) {
        if (!depth)
            return ab_fail(error, "Restored settings root must be a dictionary");
        for (size_t index = 0; index < table->count; ++index)
            if (!ab_settings_statistics_append(settings, path, depth,
                                               table->entries[index].value, error))
                return false;
        return true;
    }
    if (depth >= 3 && table->count)
        return ab_fail(error, "Restored settings exceed the supported storage depth");
    for (size_t index = 0; index < table->count; ++index) {
        const AbDataEntry *entry = table->entries + index;
        if (entry->key.kind != AB_DATA_STRING)
            return ab_fail(error,
                           "Mixed/foreign restored settings keys are unsupported");
        path[depth] = entry->key.string;
        if (entry->value.kind == AB_DATA_TABLE) {
            const AbDataTable *child = entry->value.table;
            if (!child)
                return ab_fail(error, "Missing restored settings child table");
            size_t hint = array(child) ? 0 : child->count;
            if (!ab_settings_statistics_create_table(settings, path, depth + 1, hint,
                                                     error) ||
                !import_table(settings, child, path, depth + 1, error))
                return false;
        } else if (!ab_settings_statistics_write(settings, path, depth + 1,
                                                 entry->value, error)) {
            return false;
        }
    }
    return true;
}

static bool plan_table(const AbDataTable *table, size_t depth, size_t minimum,
                       size_t *capacities, size_t capacity, size_t *count,
                       AbError *error) {
    if (!table || table->count > table->capacity || (table->count && !table->entries))
        return ab_fail(error, "Invalid restored settings capacity table");
    if (*count == capacity)
        return ab_fail(error, "Restored settings exceed planned table count");
    size_t entries = minimum;
    while (entries < table->count) {
        if (entries > SIZE_MAX / 2)
            return ab_fail(error, "Restored settings entry count exceeds capacity");
        entries *= 2;
    }
    capacities[(*count)++] = entries;
    bool dense = array(table);
    if (dense) {
        if (!depth)
            return ab_fail(error, "Restored settings root must be a dictionary");
        for (size_t index = 0; index < table->count; ++index) {
            AbDataKind kind = table->entries[index].value.kind;
            if (kind < AB_DATA_BOOLEAN || kind > AB_DATA_STRING)
                return ab_fail(error, "Restored settings arrays require scalar leaves");
        }
        return true;
    }
    if (depth >= 3 && table->count)
        return ab_fail(error, "Restored settings exceed the supported storage depth");
    for (size_t index = 0; index < table->count; ++index) {
        AbDataEntry row = table->entries[index];
        if (row.key.kind != AB_DATA_STRING)
            return ab_fail(error, "Restored settings require dictionary keys");
        if (row.value.kind == AB_DATA_TABLE &&
            !plan_table(row.value.table, depth + 1, minimum, capacities, capacity,
                        count, error))
            return false;
    }
    return true;
}

bool ab_settings_file_capacities(const AbSettingsFile *file, size_t default_entries,
                                 size_t *capacities, size_t capacity, size_t *count,
                                 AbError *error) {
    if (!file || file->failed || file->imported || !default_entries ||
        (default_entries & (default_entries - 1)) || !capacities || !capacity || !count)
        return ab_fail(error, "Invalid/unavailable settings capacity plan");
    size_t used = 0;
    if (file->loaded) {
        if (!plan_table(file->restored.data.globals, 0, default_entries, capacities,
                        capacity, &used, error))
            return false;
    } else {
        capacities[used++] = default_entries;
    }
    *count = used;
    return true;
}

bool ab_settings_file_restore(AbSettingsFile *file, AbSettingsStatistics *settings,
                              AbError *error) {
    const AbDataTable *root = ab_settings_statistics_root(settings);
    if (!file || file->failed || file->imported || !root || root->count)
        return ab_fail(error, "Settings restore requires fresh live owners");
    AbSpan path[3] = {0};
    bool okay = !file->loaded ||
                import_table(settings, file->restored.data.globals, path, 0, error);
    file->imported = okay;
    file->failed = !okay;
    return okay;
}

static bool ordered(void *context, const AbDataTable *table, size_t *cursor,
                    AbDataEntry *output, bool *present, AbError *error) {
    return ab_settings_statistics_table_next(context, table, cursor, output, present,
                                             error);
}

bool ab_settings_file_save(AbSettingsFile *file, const AbSettingsStatistics *settings,
                           AbError *error) {
    if (!file || file->failed || !file->imported || !settings)
        return ab_fail(error, "Invalid or unrestored settings-file owner");
    AbLuaTextWriter writer = {.data = file->storage.data,
                              .capacity = file->storage.size};
    AbBuffer encrypted = {0};
    bool okay =
        ab_lua_text_write(
            ab_settings_statistics_root(settings),
            (AbLuaTextWriteOrder){.context = (void *)settings, .next = ordered},
            &writer, error) &&
        ab_asset_encrypt((AbSpan){writer.data, writer.size},
                         (AbSpan){file->key, sizeof(file->key)}, &encrypted, error);
    if (okay && !cc_atomic_file_replace(file->path, encrypted.data, encrypted.size))
        okay = ab_fail(error, "Cannot atomically replace the settings file");
    ab_buffer_free(&encrypted);
    file->failed = !okay;
    if (okay)
        file->loaded = true;
    return okay;
}
