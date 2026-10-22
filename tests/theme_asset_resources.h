#ifndef AB_TEST_THEME_ASSET_RESOURCES_H
#define AB_TEST_THEME_ASSET_RESOURCES_H

#include "ab/android_assets.h"
#include "ab/asset_resources.h"
#include "ab/target_data.h"

typedef struct {
    AbAndroidAssets *planning;
    AbAssetResources *resources;
    size_t loads, releases;
} ThemeResourceHost;

static AbDataValue resource_name(const char *name) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)name, strlen(name)}};
}

static bool theme_resources_visit(void *context, AbSpan group, bool loading,
                                  AbError *error) {
    ThemeResourceHost *host = context;
    AbDataEntry entry = {number(1), {.kind = AB_DATA_STRING, .string = group}};
    AbDataTable requested = {&entry, 1, 1, NULL};
    host->loads += loading;
    host->releases += !loading;
    return ab_android_assets_manage(host->planning, &requested, loading,
                                    ab_asset_resources_host, host->resources, error);
}

static bool theme_resources_loaded(const AbAndroidAssets *planning,
                                   const AbDataTable *groups) {
    for (size_t index = 0; index < groups->count; ++index) {
        const AbDataValue *value = ab_data_get(groups, number(index + 1));
        CHECK(value && value->kind == AB_DATA_STRING);
        bool found = false;
        for (size_t group = 0; group < ab_android_assets_group_count(planning);
             ++group) {
            AbSpan current;
            CHECK(ab_android_assets_group(planning, group, &current));
            if (value->string.size == current.size &&
                (!current.size ||
                 memcmp(value->string.data, current.data, current.size) == 0)) {
                found = true;
                break;
            }
        }
        CHECK(found);
    }
    return true;
}

static bool theme_resources_transitions(ThemeResourceHost *host,
                                        const AbTargetData *themes,
                                        AbThemeAssets *state, AbError *error) {
    size_t count = 0, peak_bytes = 0, peak_sources = 0;
    for (size_t index = 0; index < themes->data.globals->count; ++index) {
        const AbDataEntry *entry = themes->data.globals->entries + index;
        if (entry->value.kind != AB_DATA_TABLE) {
            continue;
        }
        const AbDataValue *groups =
            ab_data_get(entry->value.table, resource_name("graphicSetName"));
        if (!groups) {
            continue;
        }
        CHECK(entry->key.kind == AB_DATA_STRING && groups->kind == AB_DATA_TABLE);
        if (!ab_theme_assets_transition(state, *groups, false, theme_resources_visit,
                                        host, error)) {
            fprintf(stderr, "Theme resource transition failed for %.*s: %s\n",
                    (int)entry->key.string.size, (const char *)entry->key.string.data,
                    error->message);
            return false;
        }
        CHECK(state->current == groups->table &&
              theme_resources_loaded(host->planning, groups->table));
        size_t calls = host->loads + host->releases;
        size_t bytes = ab_asset_resources_bytes(host->resources);
        size_t sources = ab_asset_resources_owners(host->resources)->count;
        CHECK(ab_theme_assets_transition(state, *groups, false, theme_resources_visit,
                                         host, error) &&
              host->loads + host->releases == calls &&
              ab_asset_resources_bytes(host->resources) == bytes &&
              ab_asset_resources_owners(host->resources)->count == sources);
        if (bytes > peak_bytes) {
            peak_bytes = bytes;
        }
        if (sources > peak_sources) {
            peak_sources = sources;
        }
        ++count;
    }
    CHECK(count == 31 && !state->failed &&
          !ab_asset_resources_failed(host->resources) &&
          !ab_android_assets_failed(host->planning));
    printf("Loaded all %zu authored theme graphics lists with real resource "
           "transitions; %zu load/%zu release callbacks, peak %zu sources/%zu "
           "retained bytes. Repeated selections preserve identity.\n",
           count, host->loads, host->releases, peak_sources, peak_bytes);
    return true;
}

static bool theme_resource_checks(const char *path) {
    AbBuffer bytes = {0};
    AbZip archive = {0};
    AbTargetData themes = {0};
    ThemeResourceHost host = {0};
    AbThemeAssets state;
    AbDataTable empty = {0};
    AbSpan scratch[32];
    uint8_t key[32] = {0};
    AbError error = {0};
    bool okay = false;
    AbAndroidAssetLimits planning = {480, 320, false, false, false, 64, 8, 1024 * 1024};
    AbAssetResourceLimits limits = {64, 8192, 128 * 1024 * 1024, 16 * 1024 * 1024};
    if (!ab_file_read(path, 64 * 1024 * 1024, &bytes, &error) ||
        !ab_zip_open((AbSpan){bytes.data, bytes.size}, &archive, &error) ||
        !ab_target_resource_key(&archive, 16 * 1024 * 1024, key, &error) ||
        !ab_target_data_open(&archive, "assets/data/scripts/themes.lua",
                             (AbSpan){key, sizeof(key)}, 4 * 1024 * 1024, NULL, &themes,
                             &error) ||
        !ab_android_assets_open(&archive, (AbSpan){key, sizeof(key)}, planning,
                                &host.planning, &error) ||
        !ab_asset_resources_open(&archive, limits, &host.resources, &error) ||
        !ab_theme_assets_init(&empty, scratch, 16, &state, &error)) {
        goto release;
    }
    size_t initial_bytes = ab_asset_resources_bytes(host.resources);
    if (!theme_resources_visit(&host, resource_name("INGAME").string, true, &error) ||
        !theme_resources_visit(&host, resource_name("INGAME_PIGS_NEW").string, true,
                               &error) ||
        !theme_resources_transitions(&host, &themes, &state, &error) ||
        !ab_theme_assets_transition(
            &state, (AbDataValue){.kind = AB_DATA_TABLE, .table = &empty}, false,
            theme_resources_visit, &host, &error) ||
        !theme_resources_visit(&host, resource_name("INGAME_PIGS_NEW").string, false,
                               &error) ||
        !theme_resources_visit(&host, resource_name("INGAME").string, false, &error)) {
        goto release;
    }
    okay = !ab_asset_resources_owners(host.resources)->count &&
           !ab_asset_resources_registry(host.resources)->count &&
           !ab_android_assets_group_count(host.planning) &&
           ab_asset_resources_bytes(host.resources) == initial_bytes;

release:
    if (!okay && error.message[0]) {
        fprintf(stderr, "%s\n", error.message);
    }
    memset(key, 0, sizeof(key));
    ab_asset_resources_close(host.resources);
    ab_android_assets_close(host.planning);
    ab_target_data_close(&themes);
    ab_zip_close(&archive);
    ab_buffer_free(&bytes);
    return okay;
}

#endif
