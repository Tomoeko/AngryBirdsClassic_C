#include "ab/asset_load_lists.h"
#include "ab/target_data.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    char *path;
    AbSpan profile;
    AbTargetData data;
    const AbDataTable *groups;
} AssetListEntry;

struct AbAssetLoadLists {
    const AbZip *archive;
    AssetListEntry *entries;
    size_t count, capacity, bytes, limit;
    uint8_t key[32];
};

bool ab_asset_load_lists_open(const AbZip *archive, AbSpan resource_key,
                              size_t profile_capacity, size_t allocation_limit,
                              AbAssetLoadLists **output, AbError *error) {
    if (!output) {
        return ab_fail(error, "Missing asset load-list owner output");
    }
    *output = NULL;
    if (!archive || !archive->input.data || resource_key.size != 32 ||
        !resource_key.data || allocation_limit < sizeof(AbAssetLoadLists) ||
        profile_capacity >
            (allocation_limit - sizeof(AbAssetLoadLists)) / sizeof(AssetListEntry)) {
        return ab_fail(error, "Invalid asset load-list archive/key/storage bounds");
    }
    AbAssetLoadLists *lists = calloc(1, sizeof(*lists));
    if (!lists) {
        return ab_fail(error, "Cannot allocate asset load-list owner");
    }
    lists->archive = archive;
    lists->capacity = profile_capacity;
    lists->limit = allocation_limit;
    lists->bytes = sizeof(*lists) + profile_capacity * sizeof(*lists->entries);
    memcpy(lists->key, resource_key.data, sizeof(lists->key));
    if (profile_capacity) {
        lists->entries = calloc(profile_capacity, sizeof(*lists->entries));
        if (!lists->entries) {
            ab_fail(error, "Cannot allocate asset load-list cache entries");
            ab_asset_load_lists_close(lists);
            return false;
        }
    }
    *output = lists;
    return true;
}

void ab_asset_load_lists_close(AbAssetLoadLists *lists) {
    if (lists) {
        for (size_t index = 0; index < lists->count; ++index) {
            ab_target_data_close(&lists->entries[index].data);
            free(lists->entries[index].path);
        }
        free(lists->entries);
        volatile uint8_t *key = lists->key;
        for (size_t index = 0; index < sizeof(lists->key); ++index) {
            key[index] = 0;
        }
        free(lists);
    }
}

size_t ab_asset_load_lists_bytes(const AbAssetLoadLists *lists) {
    return lists ? lists->bytes : 0;
}

size_t ab_asset_load_lists_count(const AbAssetLoadLists *lists) {
    return lists ? lists->count : 0;
}

static const AbDataTable *profile_groups(const AbTargetData *data, AbSpan profile) {
    const AbDataValue *root =
        ab_data_get(data->data.globals,
                    (AbDataValue){.kind = AB_DATA_STRING,
                                  .string = {(const uint8_t *)"assetLoadList", 13}});
    const AbDataValue *groups =
        root && root->kind == AB_DATA_TABLE
            ? ab_data_get(root->table,
                          (AbDataValue){.kind = AB_DATA_STRING, .string = profile})
            : NULL;
    return groups && groups->kind == AB_DATA_TABLE ? groups->table : NULL;
}

bool ab_asset_load_lists_get(AbAssetLoadLists *lists, AbSpan profile,
                             const AbDataTable **output, AbError *error) {
    if (!lists || !output || profile.size > lists->limit ||
        (profile.size && !profile.data) ||
        (profile.size && memchr(profile.data, 0, profile.size))) {
        return ab_fail(error, "Invalid asset load-list profile/output");
    }
    for (size_t index = 0; index < lists->count; ++index) {
        const AssetListEntry *entry = lists->entries + index;
        if (entry->profile.size == profile.size &&
            (!profile.size ||
             memcmp(entry->profile.data, profile.data, profile.size) == 0)) {
            *output = entry->groups;
            return true;
        }
    }
    static const char prefix[] = "assets/data/images/";
    static const char suffix[] = "/loadlist.lua";
    const size_t overhead = sizeof(prefix) - 1 + sizeof(suffix);
    size_t remaining = lists->limit - lists->bytes;
    if (lists->count == lists->capacity || overhead > remaining ||
        profile.size > remaining - overhead) {
        return ab_fail(error, "Asset load-list profile/path capacity exceeded");
    }
    size_t path_bytes = overhead + profile.size;
    char *path = malloc(path_bytes);
    AbTargetData data = {0};
    if (!path) {
        return ab_fail(error, "Cannot allocate asset load-list path");
    }
    memcpy(path, prefix, sizeof(prefix) - 1);
    if (profile.size) {
        memcpy(path + sizeof(prefix) - 1, profile.data, profile.size);
    }
    memcpy(path + sizeof(prefix) - 1 + profile.size, suffix, sizeof(suffix));
    if (!ab_target_data_open(lists->archive, path,
                             (AbSpan){lists->key, sizeof(lists->key)},
                             remaining - path_bytes, NULL, &data, error)) {
        goto release_entry;
    }
    const AbDataTable *groups = profile_groups(&data, profile);
    if (!groups) {
        ab_fail(error, "Asset load list has no matching profile group table");
        goto release_entry;
    }
    size_t used = path_bytes + data.bytes.size + data.chunk.allocated_bytes +
                  data.data.allocated_bytes;
    if (used > remaining) {
        ab_fail(error, "Asset load-list retained data exceeds byte limit");
        goto release_entry;
    }
    lists->entries[lists->count++] = (AssetListEntry){
        .path = path,
        .profile = {(const uint8_t *)path + sizeof(prefix) - 1, profile.size},
        .data = data,
        .groups = groups};
    lists->bytes += used;
    *output = groups;
    return true;

release_entry:
    ab_target_data_close(&data);
    free(path);
    return false;
}
