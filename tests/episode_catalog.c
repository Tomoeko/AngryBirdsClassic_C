#include "ab/episode_catalog.h"
#include "ab/hash.h"
#include "ab/lua_data_json.h"

#include <stdio.h>
#include <string.h>

static AbSpan span(const char *name) {
    return (AbSpan){(const uint8_t *)name, strlen(name)};
}

static AbDataValue text(const char *name) {
    return (AbDataValue){.kind = AB_DATA_STRING, .string = span(name)};
}

static AbDataValue number(size_t index) {
    float value = (float)index;
    AbDataValue output = {.kind = AB_DATA_NUMBER};
    memcpy(&output.bits, &value, sizeof(value));
    return output;
}

static const AbDataTable *child(const AbDataTable *parent, AbDataValue key) {
    const AbDataValue *value = ab_data_get(parent, key);
    return value && value->kind == AB_DATA_TABLE ? value->table : NULL;
}

static bool named(const AbDataValue *value, const char *name) {
    return value && value->kind == AB_DATA_STRING &&
           value->string.size == strlen(name) && value->string.data &&
           !memcmp(value->string.data, name, value->string.size);
}

static const AbDataTable *page(const AbDataTable *episodes, AbDataValue episode,
                               size_t index) {
    const AbDataTable *record = child(episodes, episode);
    return child(child(record, text("pages")), number(index));
}

static const AbDataTable *level(const AbDataTable *page, const char *name) {
    const AbDataTable *levels = child(page, text("levels"));
    for (size_t index = 0; levels && index < levels->count; ++index) {
        const AbDataValue *value = &levels->entries[index].value;
        if (value->kind == AB_DATA_TABLE &&
            named(ab_data_get(value->table, text("name")), name))
            return value->table;
    }
    return NULL;
}

static bool count_levels(const AbDataTable *episodes, size_t *output, AbError *error) {
    size_t count = 0;
    for (size_t episode = 0; episode < episodes->count; ++episode) {
        const AbDataValue *record = &episodes->entries[episode].value;
        if (record->kind != AB_DATA_TABLE)
            return ab_fail(error, "Catalog episode is not a table");
        const AbDataTable *pages = child(record->table, text("pages"));
        if (!pages)
            return ab_fail(error, "Catalog episode lacks pages");
        for (size_t index = 0; index < pages->count; ++index) {
            const AbDataTable *current = child(pages, number(index + 1));
            const AbDataTable *levels = child(current, text("levels"));
            if (!levels)
                return ab_fail(error, "Catalog page lacks dense levels");
            for (size_t item = 0; item < levels->count; ++item) {
                const AbDataTable *entry = child(levels, number(item + 1));
                const AbDataValue *name = ab_data_get(entry, text("name"));
                if (!name || name->kind != AB_DATA_STRING || !name->string.size)
                    return ab_fail(error, "Catalog level lacks its actual name");
                ++count;
            }
        }
    }
    *output = count;
    return true;
}

static bool numeric(const AbDataValue *value, size_t expected) {
    return value && value->kind == AB_DATA_NUMBER &&
           value->bits == number(expected).bits;
}

static bool annotations(AbEpisodeCatalog *catalog, AbError *error) {
    const AbDataTable *episodes = ab_episode_catalog_episodes(catalog);
    const AbDataTable *first_page = page(episodes, number(1), 1);
    const AbDataTable *first = child(child(first_page, text("levels")), number(1));
    if (ab_episode_catalog_levels_annotated(catalog) ||
        !ab_episode_catalog_annotate_levels(catalog, error))
        return false;
    const AbDataTable *same = child(child(first_page, text("levels")), number(1));
    const AbDataTable *second =
        child(child(page(episodes, number(1), 2), text("levels")), number(1));
    const AbDataTable *surf =
        child(child(page(episodes, text("SURF"), 2), text("levels")), number(1));
    const AbDataTable *egg =
        child(child(page(episodes, text("G"), 2), text("levels")), number(1));
    const AbDataTable *extra_page = page(episodes, number(5), 4);
    const AbDataTable *extra = child(child(extra_page, text("levels")), number(1));
    const AbDataTable *first_episode = child(episodes, number(1));
    const AbDataTable *names = child(first_episode, text("level_list"));
    const AbDataTable *bonus = child(first_episode, text("bonus_level_list"));
    const AbDataTable *empty =
        child(child(episodes, text("RMFDemo")), text("level_list"));
    if (!ab_episode_catalog_levels_annotated(catalog) || first != same ||
        !numeric(ab_data_get(first_page, text("episode")), 1) ||
        !numeric(ab_data_get(first_page, text("index")), 1) ||
        !numeric(ab_data_get(first, text("index_in_episode")), 1) ||
        !numeric(ab_data_get(second, text("index_in_episode")), 22) ||
        !numeric(ab_data_get(second, text("page")), 2) ||
        !numeric(ab_data_get(second, text("index")), 1) ||
        !named(ab_data_get(second, text("display_number")), "1") ||
        !named(ab_data_get(surf, text("display_number")), "16") ||
        !numeric(ab_data_get(surf, text("index_in_episode")), 16) ||
        !named(ab_data_get(egg, text("display_number")), "16") ||
        !numeric(ab_data_get(egg, text("index_in_episode")), 0) ||
        (extra && (!numeric(ab_data_get(extra, text("index_in_episode")), 0) ||
                   !named(ab_data_get(extra, text("display_number")), "1"))) ||
        !names || names->count != 63 || !bonus || bonus->count || !empty ||
        empty->count)
        return ab_fail(error, "Startup level annotation boundary/identity differs");
    size_t bytes = ab_episode_catalog_bytes(catalog);
    if (!ab_episode_catalog_annotate_levels(catalog, error) ||
        ab_episode_catalog_bytes(catalog) != bytes ||
        child(first_episode, text("level_list")) != names)
        return ab_fail(error, "Repeated annotation changed owned graph/storage");
    return true;
}

static bool check_configuration(const AbZip *zip, AbSpan key,
                                AbEpisodeCatalogConfig config, AbError *error) {
    AbEpisodeCatalog *catalog = NULL;
    bool okay = false;
    if (!ab_episode_catalog_open(zip, key, config, 2 * 1024 * 1024, &catalog, error))
        return false;
    const AbDataTable *episodes = ab_episode_catalog_episodes(catalog);
    const AbDataTable *ids = ab_episode_catalog_ids(catalog);
    const AbDataTable *feathers = ab_episode_catalog_feather_ids(catalog);
    const AbDataTable *second = page(episodes, text("G"), 2);
    const AbDataTable *rio = level(second, "LevelGE_19");
    size_t count = 0;
    if (!count_levels(episodes, &count, error))
        goto release;
    bool apple = (config.device_model.size == 4 &&
                  !memcmp(config.device_model.data, "ipad", 4)) ||
                 (config.device_model.size == 6 &&
                  !memcmp(config.device_model.data, "iphone", 6));
    bool limited = (config.device_model.size == 4 &&
                    !memcmp(config.device_model.data, "roku", 4)) ||
                   (config.device_model.size == 10 &&
                    !memcmp(config.device_model.data, "blackberry", 10));
    const AbDataTable *school = page(episodes, text("PowerupSchool"), 1);
    const AbDataTable *school_levels = child(school, text("levels"));
    const AbDataValue *fourth_gate =
        ab_data_get(child(school_levels, number(4)), text("disable_next_level_button"));
    if (!episodes || !ids || !feathers || !rio ||
        episodes->count != (config.freemium_china ? 18 : 17) ||
        ids->count != (config.freemium_china ? 15 : 14) || feathers->count != 2 ||
        !named(ab_data_get(ids, number(ids->count)), "G") ||
        !named(ab_data_get(ids, number(ids->count - 1)), "SURF") ||
        !named(ab_data_get(ids, number(ids->count - 2)), "birdday5UGC") ||
        (config.freemium_china && !named(ab_data_get(ids, number(1)), "Talkweb")) ||
        (!config.freemium_china && child(episodes, text("Talkweb"))) ||
        !school_levels || school_levels->count != (config.freemium_china ? 5 : 4) ||
        !fourth_gate || fourth_gate->kind != AB_DATA_BOOLEAN ||
        (fourth_gate->bits != 0) == config.freemium_china ||
        (level(school, "Level1933") != NULL) != config.freemium_china ||
        (level(second, "LevelGE_RED") != NULL) != apple ||
        (page(episodes, number(10), 3) == NULL) != limited ||
        (page(episodes, number(5), 4) == NULL) !=
            (config.disable_external_links || config.apply_china_restrictions) ||
        (ab_data_get(rio, text("external_link")) == NULL) !=
            config.disable_external_links ||
        (!config.disable_external_links &&
         !named(ab_data_get(rio, text("external_link")), "RIO_CONTEST_URL"))) {
        ab_fail(error, "Catalog source branch/dense graph differs");
        goto release;
    }
    printf("Catalog %.*s links=%u restrictions=%u china=%u: %zu episodes, %zu IDs, "
           "%zu level entries, %zu retained bytes.\n",
           (int)config.device_model.size, (const char *)config.device_model.data,
           config.disable_external_links, config.apply_china_restrictions,
           config.freemium_china, episodes->count, ids->count, count,
           ab_episode_catalog_bytes(catalog));
    okay = annotations(catalog, error);

release:
    ab_episode_catalog_close(catalog);
    return okay;
}

static bool export_catalog(const char *path, AbEpisodeCatalog *catalog,
                           AbError *error) {
    FILE *file = fopen(path, "wbx");
    if (!file)
        return ab_fail(error, "Cannot create a new catalog research export");
    /* Read-only borrowed graph view; this does not adopt the catalog's tables. */
    AbLuaData view = {
        .globals = (AbDataTable *)ab_episode_catalog_episodes(catalog),
        .returned = {.kind = AB_DATA_TABLE,
                     .table = (AbDataTable *)ab_episode_catalog_ids(catalog)}};
    bool okay = ab_lua_data_json(&view, file, error);
    if (fclose(file))
        okay = ab_fail(error, "Cannot finish catalog research export");
    return okay;
}

int main(int argc, char **argv) {
    if (argc < 2 || argc > 4)
        return 2;
    AbError error = {0};
    AbBuffer apk = {0};
    AbZip zip = {0};
    AbEpisodeCatalog *catalog = NULL;
    uint8_t key[32], digest[32];
    char hash[65];
    bool okay = false;
    if (!ab_file_read(argv[1], 64 * 1024 * 1024, &apk, &error) ||
        !ab_sha256((AbSpan){apk.data, apk.size}, digest, &error))
        goto release;
    ab_digest_hex(digest, hash);
    if (strcmp(hash,
               "b524b6b4b0ad999f46828546dee1bb047d6e00f724285a2e8c9f4b34a8bee7ad") ||
        !ab_zip_open((AbSpan){apk.data, apk.size}, &zip, &error) ||
        !ab_target_resource_key(&zip, 16 * 1024 * 1024, key, &error))
        goto release;
    for (unsigned flags = 0; flags < 8; ++flags) {
        AbEpisodeCatalogConfig config = {.device_model = span("android"),
                                         .disable_external_links = (flags & 1) != 0,
                                         .apply_china_restrictions = (flags & 2) != 0,
                                         .freemium_china = (flags & 4) != 0};
        if (!check_configuration(&zip, (AbSpan){key, sizeof(key)}, config, &error))
            goto release;
    }
    const char *const devices[] = {"ipad", "iphone", "roku", "blackberry"};
    for (size_t index = 0; index < 4; ++index)
        if (!check_configuration(
                &zip, (AbSpan){key, sizeof(key)},
                (AbEpisodeCatalogConfig){.device_model = span(devices[index])}, &error))
            goto release;
    AbEpisodeCatalogConfig config = {.device_model = span("android")};
    if (!ab_episode_catalog_open(&zip, (AbSpan){key, sizeof(key)}, config,
                                 2 * 1024 * 1024, &catalog, &error))
        goto release;
    if (argc >= 3 && strcmp(argv[2], "-") && !export_catalog(argv[2], catalog, &error))
        goto release;
    size_t bytes = ab_episode_catalog_bytes(catalog);
    if (!annotations(catalog, &error) ||
        (argc == 4 && !export_catalog(argv[3], catalog, &error)))
        goto release;
    size_t annotated_bytes = ab_episode_catalog_bytes(catalog);
    ab_episode_catalog_close(catalog);
    catalog = NULL;
    if (!ab_episode_catalog_open(&zip, (AbSpan){key, sizeof(key)}, config,
                                 annotated_bytes, &catalog, &error) ||
        !annotations(catalog, &error) ||
        ab_episode_catalog_bytes(catalog) != annotated_bytes)
        goto release;
    ab_episode_catalog_close(catalog);
    catalog = NULL;
    if (!ab_episode_catalog_open(&zip, (AbSpan){key, sizeof(key)}, config,
                                 annotated_bytes - 1, &catalog, &error))
        goto release;
    if (ab_episode_catalog_annotate_levels(catalog, &error) ||
        ab_episode_catalog_levels_annotated(catalog) ||
        ab_episode_catalog_annotate_levels(catalog, &error)) {
        ab_fail(&error, "Level annotation accepted an insufficient catalog budget");
        goto release;
    }
    ab_episode_catalog_close(catalog);
    catalog = NULL;
    if (!ab_episode_catalog_open(&zip, (AbSpan){key, sizeof(key)}, config, bytes,
                                 &catalog, &error))
        goto release;
    ab_episode_catalog_close(catalog);
    catalog = NULL;
    if (ab_episode_catalog_open(&zip, (AbSpan){key, sizeof(key)}, config, bytes - 1,
                                &catalog, &error) ||
        catalog) {
        ab_fail(&error, "Catalog accepted insufficient retained memory");
        goto release;
    }
    puts("Complete source catalog, feature branches and exact retained-memory boundary "
         "pass. Owned startup indexes and numbering preserve source identities.");
    okay = true;

release:
    if (!okay)
        fprintf(stderr, "%s\n", error.message);
    memset(key, 0, sizeof(key));
    ab_episode_catalog_close(catalog);
    ab_zip_close(&zip);
    ab_buffer_free(&apk);
    return okay ? 0 : 1;
}
