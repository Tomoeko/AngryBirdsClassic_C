#include "ab/asset_groups.h"
#include "ab/asset_load_lists.h"
#include "ab/target_resources.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Asset load-list check failed at %d: %s\n", __LINE__,      \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static AbSpan text(const char *value) {
    return (AbSpan){(const uint8_t *)value, strlen(value)};
}

static bool edge_checks(void) {
    const uint8_t zip_bytes[22] = {'P', 'K', 5, 6};
    uint8_t key[32] = {0};
    AbZip archive = {0};
    AbAssetLoadLists *lists = NULL;
    CHECK(ab_zip_open((AbSpan){zip_bytes, sizeof(zip_bytes)}, &archive, NULL));
    CHECK(!ab_asset_load_lists_open(&archive, (AbSpan){key, 31}, 1, 65536, &lists,
                                    NULL) &&
          !lists && !ab_asset_load_lists_bytes(NULL) &&
          !ab_asset_load_lists_count(NULL));
    CHECK(!ab_asset_load_lists_open(&archive, (AbSpan){key, 32}, SIZE_MAX, 65536,
                                    &lists, NULL) &&
          !lists);
    CHECK(
        ab_asset_load_lists_open(&archive, (AbSpan){key, 32}, 1, 65536, &lists, NULL));
    size_t bytes = ab_asset_load_lists_bytes(lists);
    AbDataTable sentinel = {0};
    const AbDataTable *view = &sentinel;
    CHECK(!ab_asset_load_lists_get(lists, text("base"), &view, NULL) &&
          view == &sentinel && !ab_asset_load_lists_count(lists) &&
          ab_asset_load_lists_bytes(lists) == bytes);
    CHECK(!ab_asset_load_lists_get(lists, (AbSpan){(const uint8_t *)"x", SIZE_MAX},
                                   &view, NULL) &&
          !ab_asset_load_lists_get(lists, (AbSpan){(const uint8_t *)"a\0b", 3}, &view,
                                   NULL) &&
          view == &sentinel);
    ab_asset_load_lists_close(lists);
    lists = NULL;
    CHECK(!ab_asset_load_lists_open(&archive, (AbSpan){key, 32}, 1, bytes - 1, &lists,
                                    NULL) &&
          !lists);
    CHECK(
        ab_asset_load_lists_open(&archive, (AbSpan){key, 32}, 0, 65536, &lists, NULL) &&
        !ab_asset_load_lists_get(lists, text("base"), &view, NULL) &&
        view == &sentinel);
    ab_asset_load_lists_close(lists);
    ab_zip_close(&archive);
    return true;
}

typedef struct {
    AbAssetLoadLists *lists;
    const AbZip *archive;
    AbSpan profile;
    size_t sheets, composites, releases, packaged, absent;
} ListHost;

static bool list_host(void *context, AbAssetCall *call, AbError *error) {
    ListHost *host = context;
    if (call->operation == AB_ASSET_SELECT_PROFILE) {
        call->profile = host->profile;
        return true;
    }
    if (call->operation == AB_ASSET_LOAD_LIST) {
        return ab_asset_load_lists_get(host->lists, call->profile, &call->groups,
                                       error);
    }
    char path[512];
    size_t length;
    CHECK(ab_asset_resource_path(call, path, sizeof(path), &length, error));
    if (call->operation == AB_ASSET_CREATE_SHEET) {
        ++host->sheets;
    } else if (call->operation == AB_ASSET_CREATE_COMPOSITES) {
        ++host->composites;
    } else {
        CHECK(call->operation == AB_ASSET_RELEASE_SHEET ||
              call->operation == AB_ASSET_RELEASE_COMPOSITES);
        ++host->releases;
        return true;
    }
    /* Audit authored paths independently of the resource-planning observer.
     * It does not create resources or establish original lookup/fallback rules. */
    if (ab_zip_find(host->archive, path)) {
        ++host->packaged;
    } else {
        ++host->absent;
    }
    return true;
}

static bool profile_from_path(AbSpan path, AbSpan *profile) {
    const char prefix[] = "assets/data/images/", suffix[] = "/loadlist.lua";
    size_t first = sizeof(prefix) - 1, last = sizeof(suffix) - 1;
    if (path.size <= first + last || memcmp(path.data, prefix, first) ||
        memcmp(path.data + path.size - last, suffix, last)) {
        return false;
    }
    *profile = (AbSpan){path.data + first, path.size - first - last};
    return true;
}

static bool apk_cache_checks(const AbZip *archive, AbSpan key,
                             AbAssetLoadLists *lists) {
    size_t profiles = 0, groups_count = 0;
    ListHost host = {.lists = lists, .archive = archive};
    for (size_t index = 0; index < archive->count; ++index) {
        AbSpan profile;
        if (!profile_from_path(archive->entries[index].name, &profile)) {
            continue;
        }
        host.profile = profile;
        const AbDataTable *groups = NULL, *again = NULL;
        CHECK(ab_asset_load_lists_get(lists, profile, &groups, NULL) && groups);
        size_t bytes = ab_asset_load_lists_bytes(lists);
        CHECK(ab_asset_load_lists_get(lists, profile, &again, NULL) &&
              again == groups && ab_asset_load_lists_bytes(lists) == bytes);
        size_t prior_packaged = host.packaged, prior_absent = host.absent;
        for (size_t group = 0; group < groups->count; ++group) {
            const AbDataEntry *entry = groups->entries + group;
            CHECK(entry->key.kind == AB_DATA_STRING &&
                  entry->value.kind == AB_DATA_TABLE);
            AbSpan loaded[1], scratch[1];
            AbAssetGroups state = {loaded, 0, 1, scratch, 1, false};
            CHECK(ab_asset_group_manage(&state, entry->key.string, true,
                                        text("assets/data/images"), list_host, &host,
                                        NULL) &&
                  state.count == 1);
            CHECK(ab_asset_group_manage(&state, entry->key.string, false,
                                        text("assets/data/images"), list_host, &host,
                                        NULL) &&
                  !state.count);
            ++groups_count;
        }
        printf("Profile %.*s: %zu planned paths packaged at their declared path, "
               "%zu absent.\n",
               (int)profile.size, (const char *)profile.data,
               host.packaged - prior_packaged, host.absent - prior_absent);
        ++profiles;
    }
    CHECK(profiles == 8 && ab_asset_load_lists_count(lists) == profiles &&
          host.releases == host.sheets + host.composites &&
          host.packaged + host.absent == host.sheets + host.composites);
    size_t bytes = ab_asset_load_lists_bytes(lists);
    const AbDataTable sentinel = {0}, *view = &sentinel;
    CHECK(!ab_asset_load_lists_get(lists, text("absent_profile"), &view, NULL) &&
          view == &sentinel && ab_asset_load_lists_bytes(lists) == bytes);
    AbAssetLoadLists *short_cache = NULL;
    CHECK(ab_asset_load_lists_open(archive, key, 1, 1024 * 1024, &short_cache, NULL));
    size_t base = ab_asset_load_lists_bytes(short_cache);
    ab_asset_load_lists_close(short_cache);
    short_cache = NULL;
    CHECK(ab_asset_load_lists_open(archive, key, 1, base, &short_cache, NULL));
    bool rejected = !ab_asset_load_lists_get(short_cache, text("base"), &view, NULL) &&
                    !ab_asset_load_lists_count(short_cache) &&
                    ab_asset_load_lists_bytes(short_cache) == base && view == &sentinel;
    ab_asset_load_lists_close(short_cache);
    CHECK(rejected);
    printf("Read %zu original profile load lists and %zu groups; dispatched %zu sheet "
           "and %zu composite paths with matching releases, retaining %zu bytes. "
           "Cache views are stable; this is resource planning, without GPU creation.\n",
           profiles, groups_count, host.sheets, host.composites, bytes);
    return true;
}

static bool apk_checks(const char *path) {
    AbBuffer bytes = {0};
    AbZip archive = {0};
    AbAssetLoadLists *lists = NULL;
    uint8_t key[32] = {0};
    AbError error = {0};
    bool okay = ab_file_read(path, 128 * 1024 * 1024, &bytes, &error) &&
                ab_zip_open((AbSpan){bytes.data, bytes.size}, &archive, &error) &&
                ab_target_resource_key(&archive, 16 * 1024 * 1024, key, &error) &&
                ab_asset_load_lists_open(&archive, (AbSpan){key, sizeof(key)}, 8,
                                         1024 * 1024, &lists, &error) &&
                apk_cache_checks(&archive, (AbSpan){key, sizeof(key)}, lists);
    if (!okay && error.message[0]) {
        fprintf(stderr, "%s\n", error.message);
    }
    memset(key, 0, sizeof(key));
    ab_asset_load_lists_close(lists);
    ab_zip_close(&archive);
    ab_buffer_free(&bytes);
    return okay;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        return edge_checks() ? 0 : 1;
    }
    if (argc != 3 || strcmp(argv[1], "--apk")) {
        return 2;
    }
    return apk_checks(argv[2]) ? 0 : 1;
}
