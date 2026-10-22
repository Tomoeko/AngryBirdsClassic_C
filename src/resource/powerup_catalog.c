#include "ab/powerup_catalog.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

struct AbPowerupCatalog {
    AbBuffer config_bytes;
    AbLuaChunk config_chunk;
    AbLuaData activated;
    AbTargetData definitions;
    AbPowerupMetadata *rows;
    size_t count, limit;
};

static AbDataValue text(const char *name) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)name, strlen(name)}};
}

static AbDataValue number(float value) {
    AbDataValue result = {.kind = AB_DATA_NUMBER};
    memcpy(&result.bits, &value, sizeof(value));
    return result;
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL && (value.kind != AB_DATA_BOOLEAN || value.bits);
}

static AbDataValue get(const AbDataTable *table, AbDataValue key) {
    const AbDataValue *value = ab_data_get(table, key);
    return value ? *value : (AbDataValue){0};
}

static bool numeric(AbDataValue value, float *output, AbError *error) {
    if (value.kind != AB_DATA_NUMBER)
        return ab_fail(error, "Nonnumeric powerup reward weight");
    memcpy(output, &value.bits, sizeof(*output));
    return isfinite(*output) || ab_fail(error, "Nonfinite powerup reward weight");
}

static size_t remaining(const AbPowerupCatalog *catalog) {
    size_t used = sizeof(*catalog) + catalog->config_bytes.size +
                  catalog->config_chunk.allocated_bytes +
                  catalog->activated.allocated_bytes + catalog->definitions.bytes.size +
                  catalog->definitions.chunk.allocated_bytes +
                  catalog->definitions.data.allocated_bytes +
                  catalog->count * sizeof(*catalog->rows);
    return used <= catalog->limit ? catalog->limit - used : 0;
}

static bool activation_array(AbPowerupCatalog *catalog, const AbZip *archive,
                             AbSpan key, AbError *error) {
    if (!ab_target_packed_read(archive, "assets/data/scripts/game_conf.lua", key,
                               remaining(catalog), &catalog->config_bytes, error) ||
        !ab_lua_read((AbSpan){catalog->config_bytes.data, catalog->config_bytes.size},
                     remaining(catalog), &catalog->config_chunk, error))
        return false;
    const AbLuaPrototype *root = &catalog->config_chunk.root;
    /* The initial six-item assignment precedes the guarded China replacement.
     * Check its actual registration rather than accepting an arbitrary window. */
    if (root->instructions.size < 118 * 4)
        return ab_fail(error, "Truncated powerup configuration registration");
    uint32_t word = ab_u32le(root->instructions.data + 117 * 4);
    size_t constant = word >> 14;
    if ((word & 63) != 7 || ((word >> 6) & 255) != 0 ||
        constant >= root->constant_count ||
        root->constants[constant].kind != AB_LUA_STRING)
        return ab_fail(error, "Unsupported powerup enabled-array registration");
    AbSpan name = root->constants[constant].string;
    const char expected[] = "g_enabled_powerups";
    if (name.size != sizeof(expected) - 1 || memcmp(name.data, expected, name.size))
        return ab_fail(error, "Unexpected powerup enabled-array global");
    return ab_lua_data_literal_table_range(&catalog->config_chunk, 109, 8, 0,
                                           remaining(catalog), &catalog->activated,
                                           error);
}

static bool metadata(AbPowerupCatalog *catalog, AbDataValue overrides, AbError *error) {
    AbDataValue enabled = catalog->activated.returned;
    AbDataValue definitions =
        get(catalog->definitions.data.globals, text("g_powerupDefinitions"));
    if (enabled.kind != AB_DATA_TABLE || !enabled.table || enabled.table->count != 6 ||
        definitions.kind != AB_DATA_TABLE || !definitions.table ||
        (truthy(overrides) && (overrides.kind != AB_DATA_TABLE || !overrides.table)))
        return ab_fail(error, "Unsupported powerup metadata registration domain");
    if (enabled.table->count > remaining(catalog) / sizeof(*catalog->rows))
        return ab_fail(error, "Powerup metadata rows exceed storage bound");
    catalog->count = enabled.table->count;
    catalog->rows = calloc(catalog->count, sizeof(*catalog->rows));
    if (!catalog->rows)
        return ab_fail(error, "Cannot allocate powerup metadata rows");
    float total = 0;
    for (size_t index = 0; index < catalog->count; ++index) {
        AbDataValue name = get(enabled.table, number((float)index + 1));
        AbDataValue record = get(definitions.table, name);
        if (name.kind != AB_DATA_STRING || record.kind != AB_DATA_TABLE ||
            !record.table)
            return ab_fail(error, "Missing enabled powerup definition");
        AbPowerupMetadata *row = catalog->rows + index;
        row->name = name.string;
        row->definition = record.table;
        float weight;
        if (!numeric(get(record.table, text("rewardProbabilityWeight")), &weight,
                     error))
            return false;
        total += weight;
    }
    if (!isfinite(total) || total == 0)
        return ab_fail(error, "Powerup reward total exceeds finite nonzero domain");
    for (size_t index = 0; index < catalog->count; ++index) {
        AbPowerupMetadata *row = catalog->rows + index;
        AbDataValue infinite = get(row->definition, text("infinite"));
        row->infinite =
            truthy(infinite) ? infinite : (AbDataValue){.kind = AB_DATA_BOOLEAN};
        AbDataValue initial =
            truthy(overrides)
                ? get(overrides.table,
                      (AbDataValue){.kind = AB_DATA_STRING, .string = row->name})
                : (AbDataValue){0};
        row->initial_amount = truthy(initial) ? initial : number(5);
        float weight;
        if (!numeric(get(row->definition, text("rewardProbabilityWeight")), &weight,
                     error))
            return false;
        float probability = weight / total;
        probability *= 100;
        row->reward_probability = floorf(probability);
        if (!isfinite(row->reward_probability))
            return ab_fail(error, "Powerup reward probability exceeds finite domain");
    }
    return true;
}

bool ab_powerup_catalog_open(const AbZip *archive, AbSpan key,
                             AbDataValue initial_overrides, size_t storage_bytes,
                             AbPowerupCatalog **output, AbError *error) {
    if (!output)
        return ab_fail(error, "Missing powerup catalog output");
    *output = NULL;
    if (!archive || storage_bytes <= sizeof(AbPowerupCatalog))
        return ab_fail(error, "Invalid powerup catalog input/storage");
    AbPowerupCatalog *catalog = calloc(1, sizeof(*catalog));
    if (!catalog)
        return ab_fail(error, "Cannot allocate powerup catalog");
    catalog->limit = storage_bytes;
    if (!activation_array(catalog, archive, key, error) ||
        !ab_target_data_open(
            archive, "assets/data/scripts_common/powerups/PowerupDefines.lua", key,
            remaining(catalog), NULL, &catalog->definitions, error) ||
        !metadata(catalog, initial_overrides, error)) {
        ab_powerup_catalog_close(catalog);
        return false;
    }
    *output = catalog;
    return true;
}

void ab_powerup_catalog_close(AbPowerupCatalog *catalog) {
    if (!catalog)
        return;
    free(catalog->rows);
    ab_target_data_close(&catalog->definitions);
    ab_lua_data_close(&catalog->activated);
    ab_lua_close(&catalog->config_chunk);
    ab_buffer_free(&catalog->config_bytes);
    free(catalog);
}

size_t ab_powerup_catalog_count(const AbPowerupCatalog *catalog) {
    return catalog ? catalog->count : 0;
}

const AbPowerupMetadata *ab_powerup_catalog_at(const AbPowerupCatalog *catalog,
                                               size_t index) {
    return catalog && index < catalog->count ? catalog->rows + index : NULL;
}
