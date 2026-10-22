#include "ab/classic_level_request.h"
#include "ab/episode_catalog.h"

#include <stdio.h>
#include <string.h>

static AbSpan span(const char *text) {
    return (AbSpan){(const uint8_t *)text, strlen(text)};
}

static AbDataValue text(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING, .string = span(value)};
}

static AbDataValue number(size_t value) {
    float converted = (float)value;
    AbDataValue result = {.kind = AB_DATA_NUMBER};
    memcpy(&result.bits, &converted, sizeof(converted));
    return result;
}

static AbDataValue table(AbDataTable *value) {
    return (AbDataValue){.kind = AB_DATA_TABLE, .table = value};
}

static AbDataValue get(const AbDataTable *owner, AbDataValue key) {
    const AbDataValue *value = ab_data_get(owner, key);
    return value ? *value : (AbDataValue){0};
}

static AbDataValue field(const AbDataTable *owner, const char *name) {
    return get(owner, text(name));
}

static bool edges(void) {
    AbDataEntry names[5] = {{text("name"), text("Level1")},
                            {text("name"), text("Level2")},
                            {text("name"), text("Level3")},
                            {text("name"), text("Level4")},
                            {text("name"), text("Level5")}};
    AbDataTable levels[5];
    AbDataEntry list_fields[5];
    for (size_t index = 0; index < 5; ++index) {
        levels[index] = (AbDataTable){names + index, 1, 1, NULL};
        list_fields[index] = (AbDataEntry){number(index < 3 ? index + 1 : index - 2),
                                           table(levels + index)};
    }
    AbDataTable lists[] = {{list_fields, 3, 3, NULL}, {list_fields + 3, 2, 2, NULL}};
    AbDataEntry page_fields[2][3];
    AbDataTable pages[2];
    AbDataEntry page_list_fields[2];
    for (size_t index = 0; index < 2; ++index) {
        page_fields[index][0] = (AbDataEntry){text("levels"), table(lists + index)};
        page_fields[index][1] =
            (AbDataEntry){text("folder_name"), text("/levels/pack1")};
        page_fields[index][2] = (AbDataEntry){text("world_number"), number(index + 1)};
        pages[index] = (AbDataTable){page_fields[index], 3, 3, NULL};
        page_list_fields[index] =
            (AbDataEntry){number(index + 1), table(pages + index)};
    }
    AbDataTable page_list = {page_list_fields, 2, 2, NULL};
    AbDataEntry episode_field = {text("pages"), table(&page_list)};
    AbDataTable episode = {&episode_field, 1, 1, NULL};
    AbDataEntry episode_list_field = {number(1), table(&episode)};
    AbDataTable episodes = {&episode_list_field, 1, 1, NULL};
    AbDataEntry meta_fields[] = {{text("episode"), number(1)},
                                 {text("page"), number(2)},
                                 {text("level"), number(1)},
                                 {text("levelName"), text("Level4")}};
    AbDataTable metadata = {meta_fields, 4, 4, NULL};
    AbClassicLevelRequest result = {0};
    if (!ab_classic_level_request_resolve(&episodes, &metadata, &result, NULL) ||
        result.episode != &episode || result.page != pages + 1 ||
        result.level != levels + 3 || result.current_level_number != 3 ||
        result.current_level_number_in_theme != 1 ||
        strcmp(result.resource_path, "assets/data/levels/pack1/Level4.lua"))
        return false;
    AbClassicLevelRequest before = result;
    meta_fields[3].value = text("Level1");
    if (ab_classic_level_request_resolve(&episodes, &metadata, &result, NULL) ||
        memcmp(&result, &before, sizeof(result)))
        return false;
    meta_fields[3].value = text("Level4");
    page_fields[1][1].value = text("../outside");
    if (ab_classic_level_request_resolve(&episodes, &metadata, &result, NULL) ||
        memcmp(&result, &before, sizeof(result)))
        return false;
    page_fields[1][1].value = text("/levels/pack1");
    names[0].value = text("Level4");
    names[1].value = text("Level4");
    if (ab_classic_level_request_resolve(&episodes, &metadata, &result, NULL) ||
        memcmp(&result, &before, sizeof(result)))
        return false;
    puts("Level request: canonical identities, selected-page offset semantics, "
         "path bounds and ambiguous-name rejection passed.");
    return true;
}

static bool apk(const char *path) {
    AbBuffer bytes = {0};
    AbZip archive = {0};
    AbEpisodeCatalog *catalog = NULL;
    AbError error = {0};
    uint8_t key[32] = {0};
    bool okay = false;
    size_t count = 0, file_count = 0, special_count = 0;
    if (!ab_file_read(path, 64 * 1024 * 1024, &bytes, &error) ||
        !ab_zip_open((AbSpan){bytes.data, bytes.size}, &archive, &error) ||
        !ab_target_resource_key(&archive, 16 * 1024 * 1024, key, &error) ||
        !ab_episode_catalog_open(
            &archive, (AbSpan){key, sizeof(key)},
            (AbEpisodeCatalogConfig){.device_model = span("android"),
                                     .disable_external_links = true},
            2 * 1024 * 1024, &catalog, &error) ||
        !ab_episode_catalog_annotate_levels(catalog, &error))
        goto release;
    const AbDataTable *episodes = ab_episode_catalog_episodes(catalog);
    for (size_t episode = 0; episode < episodes->count; ++episode) {
        AbDataValue value = episodes->entries[episode].value;
        if (value.kind != AB_DATA_TABLE)
            goto release;
        const AbDataTable *pages = field(value.table, "pages").table;
        for (size_t page = 1; page <= pages->count; ++page) {
            AbDataValue current = get(pages, number(page));
            if (current.kind == AB_DATA_NIL)
                break;
            const AbDataTable *levels = field(current.table, "levels").table;
            for (size_t level = 1; level <= levels->count; ++level) {
                AbDataValue record = get(levels, number(level));
                if (record.kind == AB_DATA_NIL)
                    break;
                AbDataEntry fields[] = {
                    {text("episode"), episodes->entries[episode].key},
                    {text("page"), number(page)},
                    {text("level"), number(level)},
                    {text("levelName"), field(record.table, "name")}};
                AbDataTable metadata = {fields, 4, 4, NULL};
                AbClassicLevelRequest result = {0};
                if (!ab_classic_level_request_resolve(episodes, &metadata, &result,
                                                      &error)) {
                    AbDataValue name = field(record.table, "name");
                    AbDataValue folder = field(current.table, "folder_name");
                    fprintf(stderr,
                            "Catalog request page %zu level %zu, name %.*s, "
                            "folder kind %u bytes %zu: %.*s\n",
                            page, level,
                            (int)(name.string.size < 120 ? name.string.size : 120),
                            name.string.data ? (const char *)name.string.data : "",
                            (unsigned)folder.kind, folder.string.size,
                            (int)(folder.string.size < 120 ? folder.string.size : 120),
                            folder.string.data ? (const char *)folder.string.data : "");
                    goto release;
                }
                if (result.page != current.table || result.level != record.table) {
                    ab_fail(&error, "Level request resource differs: %s",
                            result.resource_path);
                    goto release;
                }
                if (ab_zip_find(&archive, result.resource_path)) {
                    ++file_count;
                } else {
                    const char *const special[] = {"SOUNDBOARD1", "RADIO", "KEYBOARD",
                                                   "SEQUENCER", "ACCORDION"};
                    bool known = false;
                    for (size_t index = 0; index < 5; ++index)
                        if (result.name.string.size == strlen(special[index]) &&
                            !memcmp(result.name.string.data, special[index],
                                    result.name.string.size))
                            known = true;
                    if (!known) {
                        ab_fail(&error, "Unknown non-file level request: %s",
                                result.resource_path);
                        goto release;
                    }
                    ++special_count;
                }
                ++count;
            }
        }
    }
    if (count != 594 || file_count != 589 || special_count != 5) {
        ab_fail(&error, "Level request coverage differs: %zu records", count);
        goto release;
    }
    printf("All %zu actual APK requests resolve canonical records: %zu level files "
           "and %zu special non-file entries. No special-mode factory, gameplay "
           "or bootstrap parity is claimed.\n",
           count, file_count, special_count);
    okay = true;
release:
    if (!okay)
        fprintf(stderr, "%s\n", error.message);
    memset(key, 0, sizeof(key));
    ab_episode_catalog_close(catalog);
    ab_zip_close(&archive);
    ab_buffer_free(&bytes);
    return okay;
}

int main(int argc, char **argv) {
    if (!edges())
        return 1;
    return argc == 1 ? 0 : argc == 2 ? (apk(argv[1]) ? 0 : 1) : 2;
}
