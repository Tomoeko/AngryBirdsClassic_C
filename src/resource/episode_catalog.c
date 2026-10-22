#include "ab/episode_catalog.h"

#include "ab/hash.h"
#include "data_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct AbEpisodeCatalog {
    AbBuffer bytes;
    AbLuaChunk chunk;
    AbLuaData parts[10];
    AbDataTable *episodes, *ids, *feather_ids;
    size_t limit;
    char *display_numbers;
    size_t display_bytes;
    bool annotated, annotation_failed;
};

static AbDataValue text(const char *name) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)name, strlen(name)}};
}

static AbDataValue number(size_t index) {
    float value = (float)index;
    AbDataValue output = {.kind = AB_DATA_NUMBER};
    memcpy(&output.bits, &value, sizeof(value));
    return output;
}

static AbDataValue table(AbDataTable *value) {
    return (AbDataValue){.kind = AB_DATA_TABLE, .table = value};
}

static bool named(AbSpan value, const char *name) {
    return value.size == strlen(name) && value.data &&
           !memcmp(value.data, name, value.size);
}

static size_t remaining(const AbEpisodeCatalog *catalog) {
    size_t used = sizeof(*catalog) + catalog->bytes.size +
                  catalog->chunk.allocated_bytes + catalog->display_bytes;
    for (size_t index = 0; index < 10; ++index)
        used += catalog->parts[index].allocated_bytes;
    return used <= catalog->limit ? catalog->limit - used : 0;
}

static bool decode(AbEpisodeCatalog *catalog, size_t part, size_t first, size_t count,
                   const unsigned *registers, size_t selected, AbDataValue *output,
                   AbError *error) {
    return ab_data_literal_tables(&catalog->chunk, first, count, registers, selected,
                                  remaining(catalog), output, catalog->parts + part,
                                  error);
}

static AbLuaData *owner(AbEpisodeCatalog *catalog, const AbDataTable *table) {
    for (size_t index = 0; index < 10; ++index) {
        AbLuaData *part = catalog->parts + index;
        for (const AbDataTable *owned = part->owned; owned; owned = owned->next_owned)
            if (owned == table) {
                part->memory_limit = part->allocated_bytes + remaining(catalog);
                return part;
            }
    }
    return NULL;
}

static bool put(AbEpisodeCatalog *catalog, AbDataTable *destination, AbDataValue key,
                AbDataValue value, AbError *error) {
    AbLuaData *data = owner(catalog, destination);
    return data ? ab_data_table_put(data, destination, key, value, error)
                : ab_fail(error, "Foreign episode catalog table mutation");
}

static bool child(AbDataTable *parent, AbDataValue key, AbDataTable **output,
                  AbError *error) {
    const AbDataValue *value = ab_data_get(parent, key);
    if (!value || value->kind != AB_DATA_TABLE || !value->table) {
        ab_fail(error, "Missing episode catalog child table");
        return false;
    }
    *output = value->table;
    return true;
}

static bool page(AbEpisodeCatalog *catalog, AbDataValue episode, size_t index,
                 AbDataTable **output, AbError *error) {
    AbDataTable *record, *pages;
    return child(catalog->episodes, episode, &record, error) &&
           child(record, text("pages"), &pages, error) &&
           child(pages, number(index), output, error);
}

static bool remove_page(AbEpisodeCatalog *catalog, size_t episode, size_t index,
                        AbError *error) {
    AbDataTable *record, *pages;
    return child(catalog->episodes, number(episode), &record, error) &&
           child(record, text("pages"), &pages, error) &&
           put(catalog, pages, number(index), (AbDataValue){0}, error);
}

static size_t length(const AbDataTable *array) {
    size_t count = 0;
    while (count < 16384 && ab_data_get(array, number(count + 1)))
        ++count;
    return count;
}

static bool golden_eggs(AbEpisodeCatalog *catalog, bool disable_links, AbError *error) {
    /* The source leaves these tables pending SETLIST/SETTABLE around getRioUrl.
     * Read both pure fragments independently; no state crosses into a reader. */
    const unsigned before_registers[] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14};
    const unsigned after_registers[] = {15, 16, 17, 18, 19, 20, 21, 22, 23, 24};
    AbDataValue before[12], after[10];
    if (!decode(catalog, 1, 3442, 420, before_registers, 12, before, error) ||
        !decode(catalog, 2, 3865, 147, after_registers, 10, after, error))
        return false;
    /* getRioUrl37285 is a Boolean gate returning this resource identifier. */
    AbDataValue link = disable_links ? (AbDataValue){0} : text("RIO_CONTEST_URL");
    if (!put(catalog, before[11].table, text("external_link"), link, error))
        return false;
    AbDataTable *levels = before[4].table;
    for (size_t index = 0; index < 7; ++index)
        if (!put(catalog, levels, number(index + 1), before[index + 5], error))
            return false;
    for (size_t index = 0; index < 10; ++index)
        if (!put(catalog, levels, number(index + 8), after[index], error))
            return false;
    return put(catalog, before[3].table, text("levels"), table(levels), error) &&
           put(catalog, before[1].table, number(1), before[2], error) &&
           put(catalog, before[1].table, number(2), before[3], error) &&
           put(catalog, before[0].table, text("pages"), before[1], error) &&
           put(catalog, catalog->episodes, text("G"), before[0], error);
}

static bool hidden_episodes(AbEpisodeCatalog *catalog, AbError *error) {
    static const struct {
        const char *name;
        size_t first, count;
    } fragments[] = {{"EagleDemo", 4018, 67},
                     {"PowerupSchool", 4087, 74},
                     {"Talkweb", 4163, 118},
                     {"RMFDemo", 4283, 3}};
    const unsigned selected = 3;
    for (size_t index = 0; index < 4; ++index) {
        AbDataValue record;
        if (!decode(catalog, index + 3, fragments[index].first, fragments[index].count,
                    &selected, 1, &record, error) ||
            !put(catalog, catalog->episodes, text(fragments[index].name), record,
                 error))
            return false;
    }
    return true;
}

static bool apple_egg(AbEpisodeCatalog *catalog, AbError *error) {
    const unsigned selected = 1;
    AbDataValue level;
    AbDataTable *second, *levels;
    return decode(catalog, 7, 4307, 15, &selected, 1, &level, error) &&
           page(catalog, text("G"), 2, &second, error) &&
           child(second, text("levels"), &levels, error) &&
           put(catalog, levels, number(length(levels) + 1), level, error);
}

static bool china_school(AbEpisodeCatalog *catalog, AbError *error) {
    AbDataTable *first, *levels, *fourth;
    const unsigned selected = 2;
    AbDataValue level;
    if (!page(catalog, text("PowerupSchool"), 1, &first, error) ||
        !child(first, text("levels"), &levels, error) ||
        !child(levels, number(4), &fourth, error) ||
        !put(catalog, fourth, text("disable_next_level_button"),
             (AbDataValue){.kind = AB_DATA_BOOLEAN}, error) ||
        !decode(catalog, 8, 4374, 6, &selected, 1, &level, error) ||
        !put(catalog, levels, number(length(levels) + 1), level, error))
        return false;
    /* Read the source's non-ASCII display number from its actual LOADK. */
    uint32_t word = ab_u32le(catalog->chunk.root.instructions.data + 4448 * 4);
    size_t constant = word >> 14;
    if ((word & 63) != 1 || constant >= catalog->chunk.root.constant_count ||
        catalog->chunk.root.constants[constant].kind != AB_LUA_STRING)
        return ab_fail(error, "Invalid China school display-number constant");
    return put(catalog, first, text("display_number"),
               (AbDataValue){.kind = AB_DATA_STRING,
                             .string = catalog->chunk.root.constants[constant].string},
               error);
}

static bool episode_ids(AbEpisodeCatalog *catalog, bool china, AbError *error) {
    AbLuaData *data = catalog->parts;
    data->memory_limit = data->allocated_bytes + remaining(catalog);
    catalog->ids = ab_data_table_new(data, error);
    if (!catalog->ids || !ab_data_record_layout(data, catalog->ids, 0, 0, error))
        return false;
    size_t position = 1;
    if (china &&
        !put(catalog, catalog->ids, number(position++), text("Talkweb"), error))
        return false;
    size_t count = length(catalog->episodes);
    for (size_t index = 1; index <= count; ++index)
        if (!put(catalog, catalog->ids, number(position++), number(index), error))
            return false;
    const char *const extra[] = {"birdday5UGC", "SURF", "G"};
    for (size_t index = 0; index < 3; ++index)
        if (!put(catalog, catalog->ids, number(position++), text(extra[index]), error))
            return false;
    return true;
}

static bool compose(AbEpisodeCatalog *catalog, AbEpisodeCatalogConfig config,
                    AbError *error) {
    const unsigned selected = 1;
    AbDataValue episodes;
    if (!decode(catalog, 0, 5, 3436, &selected, 1, &episodes, error))
        return false;
    catalog->episodes = episodes.table;
    if (!golden_eggs(catalog, config.disable_external_links, error) ||
        !hidden_episodes(catalog, error))
        return false;
    if ((named(config.device_model, "ipad") || named(config.device_model, "iphone")) &&
        !apple_egg(catalog, error))
        return false;
    if (config.freemium_china && !china_school(catalog, error))
        return false;
    if (!episode_ids(catalog, config.freemium_china, error))
        return false;
    const unsigned feather_register = 2;
    AbDataValue feathers;
    if (!decode(catalog, 9, 4413, 4, &feather_register, 1, &feathers, error))
        return false;
    catalog->feather_ids = feathers.table;
    if ((config.disable_external_links || config.apply_china_restrictions) &&
        !remove_page(catalog, 5, 4, error))
        return false;
    if (!config.freemium_china &&
        !put(catalog, catalog->episodes, text("Talkweb"), (AbDataValue){0}, error))
        return false;
    return !(named(config.device_model, "roku") ||
             named(config.device_model, "blackberry")) ||
           remove_page(catalog, 10, 3, error);
}

static bool truthy(const AbDataValue *value) {
    return value && value->kind != AB_DATA_NIL &&
           (value->kind != AB_DATA_BOOLEAN || value->bits != 0);
}

static AbDataTable *new_list(AbEpisodeCatalog *catalog, AbError *error) {
    AbLuaData *part = catalog->parts;
    part->memory_limit = part->allocated_bytes + remaining(catalog);
    return ab_data_table_new(part, error);
}

static bool prepare_display_numbers(AbEpisodeCatalog *catalog, AbError *error) {
    size_t count = 0;
    for (size_t index = 0; index < catalog->episodes->count; ++index) {
        AbDataValue record = catalog->episodes->entries[index].value;
        AbDataTable *pages;
        if (record.kind != AB_DATA_TABLE ||
            !child(record.table, text("pages"), &pages, error))
            return false;
        size_t pages_count = length(pages);
        for (size_t page_index = 1; page_index <= pages_count; ++page_index) {
            AbDataTable *current, *levels;
            if (!child(pages, number(page_index), &current, error) ||
                !child(current, text("levels"), &levels, error))
                return false;
            size_t added = length(levels);
            if (added > 16384 - count)
                return ab_fail(error, "Episode level count exceeds annotation domain");
            count += added;
        }
        const AbDataValue *bonus = ab_data_get(record.table, text("bonus_levels"));
        if (truthy(bonus)) {
            if (bonus->kind != AB_DATA_TABLE)
                return ab_fail(error, "Unsupported episode bonus-level collection");
            size_t added = length(bonus->table);
            if (added > 16384 - count)
                return ab_fail(error, "Episode bonus count exceeds annotation domain");
            count += added;
        }
    }
    /* All target ordinals are small positive integers; six bytes admit the
     * existing bounded dense-array domain, with no approximate number format. */
    if (!count || count > 16384 || count > remaining(catalog) / 6)
        return ab_fail(error, "Episode display-number storage exceeds catalog budget");
    size_t bytes = count * 6;
    char *numbers = malloc(bytes);
    if (!numbers)
        return ab_fail(error, "Cannot allocate owned episode display numbers");
    catalog->display_numbers = numbers;
    catalog->display_bytes = bytes;
    for (size_t index = 1; index <= count; ++index) {
        int written = snprintf(numbers + (index - 1) * 6, 6, "%zu", index);
        if (written < 1 || written >= 6)
            return ab_fail(error, "Unsupported episode display-number formatting");
    }
    return true;
}

static bool annotate_level(AbEpisodeCatalog *catalog, AbDataTable *level,
                           AbDataValue episode, size_t page_index, size_t index,
                           size_t progression, size_t display_number,
                           AbDataTable *names, size_t ordinal, AbError *error) {
    const AbDataValue *name = ab_data_get(level, text("name"));
    if (!name || name->kind != AB_DATA_STRING || !name->string.size ||
        !display_number || display_number > catalog->display_bytes / 6)
        return ab_fail(error, "Missing actual level name or display-number storage");
    /* Field insertion may reallocate entries; retain the borrowed string value
     * before assigning the source startup metadata to this same level table. */
    AbDataValue saved_name = *name;
    const char *digits = catalog->display_numbers + (display_number - 1) * 6;
    return put(catalog, level, text("episode"), episode, error) &&
           put(catalog, level, text("page"), number(page_index), error) &&
           put(catalog, level, text("index"), number(index), error) &&
           put(catalog, level, text("index_in_episode"), number(progression), error) &&
           put(catalog, level, text("display_number"), text(digits), error) &&
           put(catalog, names, number(ordinal), saved_name, error);
}

static bool annotate_episode(AbEpisodeCatalog *catalog, AbDataValue id,
                             AbDataTable *record, AbError *error) {
    AbDataTable *pages;
    if (!child(record, text("pages"), &pages, error))
        return false;
    AbDataTable *names = new_list(catalog, error);
    if (!names || !put(catalog, record, text("level_list"), table(names), error))
        return false;
    size_t progression = 1, display_number = 1, ordinal = 1;
    size_t pages_count = length(pages);
    for (size_t page_index = 1; page_index <= pages_count; ++page_index) {
        AbDataTable *current, *levels;
        if (!child(pages, number(page_index), &current, error) ||
            !child(current, text("levels"), &levels, error))
            return false;
        if (truthy(ab_data_get(record, text("per_page_level_numbering"))))
            display_number = 1;
        bool no_progression =
            truthy(ab_data_get(record, text("no_level_progression"))) ||
            truthy(ab_data_get(current, text("no_level_progression")));
        if (!put(catalog, current, text("episode"), id, error) ||
            !put(catalog, current, text("index"), number(page_index), error))
            return false;
        size_t count = length(levels);
        for (size_t index = 1; index <= count; ++index) {
            AbDataTable *level;
            if (!child(levels, number(index), &level, error) ||
                !annotate_level(catalog, level, id, page_index, index,
                                no_progression ? 0 : progression, display_number, names,
                                ordinal, error))
                return false;
            if (!no_progression)
                ++progression;
            ++display_number;
            ++ordinal;
        }
    }
    AbDataTable *bonus_names = new_list(catalog, error);
    if (!bonus_names ||
        !put(catalog, record, text("bonus_level_list"), table(bonus_names), error))
        return false;
    const AbDataValue *bonus = ab_data_get(record, text("bonus_levels"));
    if (!truthy(bonus))
        return true;
    if (bonus->kind != AB_DATA_TABLE)
        return ab_fail(error, "Unsupported episode bonus-level collection");
    AbDataTable *levels = bonus->table;
    size_t count = length(levels);
    for (size_t index = 1; index <= count; ++index) {
        AbDataTable *level;
        if (!child(levels, number(index), &level, error) ||
            !annotate_level(catalog, level, id, 1, index, 0, index, bonus_names, index,
                            error))
            return false;
    }
    return true;
}

bool ab_episode_catalog_annotate_levels(AbEpisodeCatalog *catalog, AbError *error) {
    if (!catalog || !catalog->episodes || catalog->annotation_failed)
        return ab_fail(error, "Invalid or failed episode level-annotation owner");
    if (catalog->annotated)
        return true;
    if (!prepare_display_numbers(catalog, error)) {
        catalog->annotation_failed = true;
        return false;
    }
    for (size_t index = 0; index < catalog->episodes->count; ++index) {
        AbDataEntry entry = catalog->episodes->entries[index];
        if (entry.value.kind != AB_DATA_TABLE ||
            !annotate_episode(catalog, entry.key, entry.value.table, error)) {
            catalog->annotation_failed = true;
            return false;
        }
    }
    catalog->annotated = true;
    return true;
}

bool ab_episode_catalog_levels_annotated(const AbEpisodeCatalog *catalog) {
    return catalog && catalog->annotated && !catalog->annotation_failed;
}

bool ab_episode_catalog_open(const AbZip *archive, AbSpan key,
                             AbEpisodeCatalogConfig config, size_t allocation_limit,
                             AbEpisodeCatalog **output, AbError *error) {
    if (!output)
        return ab_fail(error, "Missing episode catalog output");
    *output = NULL;
    if (!archive || (config.device_model.size && !config.device_model.data) ||
        allocation_limit < sizeof(AbEpisodeCatalog))
        return ab_fail(error, "Invalid episode catalog configuration/limit");
    AbEpisodeCatalog *catalog = calloc(1, sizeof(*catalog));
    if (!catalog)
        return ab_fail(error, "Cannot allocate episode catalog owner");
    catalog->limit = allocation_limit;
    uint8_t digest[32];
    char hash[65];
    if (!ab_target_packed_read(archive, "assets/data/scripts/episodes.lua", key,
                               remaining(catalog), &catalog->bytes, error) ||
        !ab_sha256((AbSpan){catalog->bytes.data, catalog->bytes.size}, digest, error))
        goto release;
    ab_digest_hex(digest, hash);
    if (strcmp(hash,
               "fccb2f129906205a6fe53783e84d262e03dc4f877a8bac6d44a637d2e600580f")) {
        ab_fail(error, "Unsupported episodes.lua constructor resource");
        goto release;
    }
    if (!ab_lua_read((AbSpan){catalog->bytes.data, catalog->bytes.size},
                     remaining(catalog), &catalog->chunk, error) ||
        !compose(catalog, config, error))
        goto release;
    *output = catalog;
    return true;

release:
    ab_episode_catalog_close(catalog);
    return false;
}

void ab_episode_catalog_close(AbEpisodeCatalog *catalog) {
    if (!catalog)
        return;
    for (size_t index = 0; index < 10; ++index)
        ab_lua_data_close(catalog->parts + index);
    free(catalog->display_numbers);
    ab_lua_close(&catalog->chunk);
    ab_buffer_free(&catalog->bytes);
    free(catalog);
}

const AbDataTable *ab_episode_catalog_episodes(const AbEpisodeCatalog *catalog) {
    return catalog ? catalog->episodes : NULL;
}

const AbDataTable *ab_episode_catalog_ids(const AbEpisodeCatalog *catalog) {
    return catalog ? catalog->ids : NULL;
}

const AbDataTable *ab_episode_catalog_feather_ids(const AbEpisodeCatalog *catalog) {
    return catalog ? catalog->feather_ids : NULL;
}

size_t ab_episode_catalog_bytes(const AbEpisodeCatalog *catalog) {
    return catalog ? catalog->limit - remaining(catalog) : 0;
}
