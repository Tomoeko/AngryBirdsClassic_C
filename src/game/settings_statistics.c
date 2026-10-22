#include "ab/settings_statistics.h"
#include "ab/string_map.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    AbDataTable data;
    AbStringMap *map;
    bool array;
} StatisticsTable;

struct AbSettingsStatistics {
    StatisticsTable *tables;
    AbDataEntry *entries;
    uint8_t *ids;
    AbSpan device, installation;
    size_t capacity, table_count, bytes;
};

static AbDataValue key_value(AbSpan key) {
    return (AbDataValue){.kind = AB_DATA_STRING, .string = key};
}

static AbDataValue number_value(float value) {
    AbDataValue result = {.kind = AB_DATA_NUMBER};
    memcpy(&result.bits, &value, sizeof(value));
    return result;
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL && (value.kind != AB_DATA_BOOLEAN || value.bits);
}

static bool finite_number(AbDataValue value, float *output) {
    if (value.kind != AB_DATA_NUMBER)
        return false;
    memcpy(output, &value.bits, sizeof(*output));
    return isfinite(*output);
}

static bool valid_span(AbSpan value) {
    return value.size <= UINT32_MAX && (!value.size || value.data);
}

static AbDataValue get(const AbDataTable *table, AbSpan key) {
    const AbDataValue *value = ab_data_get(table, key_value(key));
    return value ? *value : (AbDataValue){0};
}

static StatisticsTable *owned_table(const AbSettingsStatistics *statistics,
                                    const AbDataTable *data) {
    for (size_t index = 0; index < statistics->table_count; ++index)
        if (&statistics->tables[index].data == data)
            return statistics->tables + index;
    return NULL;
}

static bool put(StatisticsTable *table, AbSpan key, AbDataValue value, AbError *error) {
    if (table->array)
        return ab_fail(error, "String write into a dense settings array");
    uintptr_t handle = 0;
    bool present = false;
    if (!ab_string_map_get(table->map, key, &handle, &present, error))
        return false;
    /* Retain nil-valued entry slots independently of live hash values. */
    size_t index = present ? (size_t)handle - 1 : table->data.count;
    if (!present) {
        for (size_t candidate = 0; candidate < table->data.count; ++candidate) {
            AbSpan old = table->data.entries[candidate].key.string;
            if (old.size == key.size &&
                (!key.size || !memcmp(old.data, key.data, key.size))) {
                index = candidate;
                break;
            }
        }
    }
    if (index == table->data.capacity)
        return ab_fail(error, "Statistics table entry capacity exhausted");
    if (!ab_string_map_set(table->map, key, (uintptr_t)(index + 1),
                           value.kind != AB_DATA_NIL, error))
        return false;
    table->data.entries[index] = (AbDataEntry){key_value(key), value};
    if (index == table->data.count)
        ++table->data.count;
    return true;
}

static bool reserve(size_t count, size_t element, size_t limit, size_t *bytes) {
    if (*bytes > limit || count > (limit - *bytes) / element)
        return false;
    *bytes += count * element;
    return true;
}

bool ab_settings_statistics_open(AbSettingsStatisticsConfig config,
                                 AbSettingsStatistics **output, AbError *error) {
    return ab_settings_statistics_open_capacities(config, NULL, 0, output, error);
}

bool ab_settings_statistics_open_capacities(AbSettingsStatisticsConfig config,
                                            const size_t *capacities, size_t count,
                                            AbSettingsStatistics **output,
                                            AbError *error) {
    if (!output)
        return ab_fail(error, "Missing statistics storage output");
    *output = NULL;
    if (!valid_span(config.device_id) || !valid_span(config.installation_id) ||
        !config.tables || !config.entries_per_table ||
        (config.entries_per_table & (config.entries_per_table - 1)) ||
        count > config.tables || (count && !capacities))
        return ab_fail(error, "Invalid statistics storage configuration");
    size_t bytes = sizeof(AbSettingsStatistics);
    if (!reserve(config.tables, sizeof(StatisticsTable), config.storage_bytes, &bytes))
        return ab_fail(error, "Statistics table headers exceed byte limit");
    size_t entry_count = 0;
    for (size_t index = 0; index < config.tables; ++index) {
        size_t capacity = index < count ? capacities[index] : config.entries_per_table;
        if (!capacity || (capacity & (capacity - 1)) ||
            capacity > SIZE_MAX - entry_count)
            return ab_fail(error, "Invalid statistics table capacity");
        entry_count += capacity;
    }
    if (!reserve(entry_count, sizeof(AbDataEntry), config.storage_bytes, &bytes) ||
        !reserve(config.device_id.size, 1, config.storage_bytes, &bytes) ||
        !reserve(config.installation_id.size, 1, config.storage_bytes, &bytes) ||
        !reserve(22, 1, config.storage_bytes, &bytes))
        return ab_fail(error, "Statistics storage exceeds byte limit");
    AbSettingsStatistics *statistics = calloc(1, sizeof(*statistics));
    if (!statistics)
        return ab_fail(error, "Cannot allocate statistics storage");
    statistics->capacity = config.tables;
    statistics->bytes = bytes;
    statistics->tables = calloc(config.tables, sizeof(*statistics->tables));
    statistics->entries = calloc(entry_count, sizeof(*statistics->entries));
    statistics->ids = malloc(config.device_id.size + config.installation_id.size + 22);
    if (!statistics->tables || !statistics->entries || !statistics->ids) {
        ab_fail(error, "Cannot allocate statistics tables");
        goto release_statistics;
    }
    statistics->device = (AbSpan){statistics->ids, 7 + config.device_id.size};
    statistics->installation = (AbSpan){statistics->ids + statistics->device.size,
                                        15 + config.installation_id.size};
    memcpy(statistics->ids, "device_", 7);
    if (config.device_id.size)
        memcpy(statistics->ids + 7, config.device_id.data, config.device_id.size);
    memcpy((uint8_t *)statistics->installation.data, "installationId_", 15);
    if (config.installation_id.size)
        memcpy((uint8_t *)statistics->installation.data + 15,
               config.installation_id.data, config.installation_id.size);
    size_t entry_offset = 0;
    for (size_t index = 0; index < config.tables; ++index) {
        StatisticsTable *table = statistics->tables + index;
        table->data.entries = statistics->entries + entry_offset;
        table->data.capacity =
            index < count ? capacities[index] : config.entries_per_table;
        entry_offset += table->data.capacity;
        if (!ab_string_map_open(0, table->data.capacity,
                                config.storage_bytes - statistics->bytes, &table->map,
                                error))
            goto release_statistics;
        statistics->bytes += ab_string_map_bytes(table->map);
    }
    statistics->table_count = 1;
    *output = statistics;
    return true;

release_statistics:
    ab_settings_statistics_close(statistics);
    return false;
}

void ab_settings_statistics_close(AbSettingsStatistics *statistics) {
    if (!statistics)
        return;
    if (statistics->tables)
        for (size_t index = 0; index < statistics->capacity; ++index)
            ab_string_map_close(statistics->tables[index].map);
    free(statistics->ids);
    free(statistics->entries);
    free(statistics->tables);
    free(statistics);
}

size_t ab_settings_statistics_bytes(const AbSettingsStatistics *statistics) {
    return statistics ? statistics->bytes : 0;
}

const AbDataTable *ab_settings_statistics_root(const AbSettingsStatistics *statistics) {
    return statistics ? &statistics->tables[0].data : NULL;
}

static StatisticsTable *parent_table(AbSettingsStatistics *statistics,
                                     const AbSpan *path, size_t count, AbError *error) {
    if (!statistics || !path || !count || count > 3) {
        ab_fail(error, "Invalid statistics storage path");
        return NULL;
    }
    for (size_t index = 0; index < count; ++index)
        if (!valid_span(path[index])) {
            ab_fail(error, "Invalid statistics path key");
            return NULL;
        }
    StatisticsTable *table = statistics->tables;
    for (size_t index = 0; index + 1 < count; ++index) {
        AbDataValue child = get(&table->data, path[index]);
        if (child.kind == AB_DATA_NIL) {
            if (statistics->table_count == statistics->capacity) {
                ab_fail(error, "Statistics table capacity exhausted");
                return NULL;
            }
            child = (AbDataValue){
                .kind = AB_DATA_TABLE,
                .table = &statistics->tables[statistics->table_count].data};
            if (!put(table, path[index], child, error))
                return NULL;
            ++statistics->table_count;
        }
        if (child.kind != AB_DATA_TABLE) {
            ab_fail(error, "Statistics path indexes a non-table value");
            return NULL;
        }
        table = owned_table(statistics, child.table);
        if (!table) {
            ab_fail(error, "Statistics path escaped owned storage");
            return NULL;
        }
    }
    return table;
}

bool ab_settings_statistics_write(AbSettingsStatistics *statistics, const AbSpan *path,
                                  size_t count, AbDataValue value, AbError *error) {
    float number;
    if (value.kind < AB_DATA_NIL || value.kind > AB_DATA_STRING ||
        (value.kind == AB_DATA_NUMBER && !finite_number(value, &number)) ||
        (value.kind == AB_DATA_STRING && !valid_span(value.string)))
        return ab_fail(error, "Unsupported statistics storage write");
    StatisticsTable *parent = parent_table(statistics, path, count, error);
    return parent && put(parent, path[count - 1], value, error);
}

bool ab_settings_statistics_create_table(AbSettingsStatistics *statistics,
                                         const AbSpan *path, size_t count,
                                         size_t hash_hint, AbError *error) {
    StatisticsTable *parent = parent_table(statistics, path, count, error);
    if (!parent)
        return false;
    if (statistics->table_count == statistics->capacity)
        return ab_fail(error, "Statistics table capacity exhausted");
    StatisticsTable *child = statistics->tables + statistics->table_count;
    if (!ab_string_map_reset(child->map, hash_hint, error) ||
        !put(parent, path[count - 1],
             (AbDataValue){.kind = AB_DATA_TABLE, .table = &child->data}, error))
        return false;
    ++statistics->table_count;
    return true;
}

bool ab_settings_statistics_append(AbSettingsStatistics *statistics, const AbSpan *path,
                                   size_t count, AbDataValue value, AbError *error) {
    float number;
    if (value.kind < AB_DATA_BOOLEAN || value.kind > AB_DATA_STRING ||
        (value.kind == AB_DATA_NUMBER && !finite_number(value, &number)) ||
        (value.kind == AB_DATA_STRING && !valid_span(value.string)))
        return ab_fail(error, "Unsupported settings array value");
    StatisticsTable *parent = parent_table(statistics, path, count, error);
    if (!parent)
        return false;
    AbDataValue leaf = get(&parent->data, path[count - 1]);
    if (leaf.kind == AB_DATA_NIL) {
        if (!ab_settings_statistics_create_table(statistics, path, count, 0, error))
            return false;
        leaf = get(&parent->data, path[count - 1]);
    }
    StatisticsTable *array =
        leaf.kind == AB_DATA_TABLE ? owned_table(statistics, leaf.table) : NULL;
    if (!array || (!array->array && array->data.count))
        return ab_fail(error, "Settings array is not a dense owned leaf");
    if (array->data.count == array->data.capacity ||
        array->data.count >= UINT32_C(16777216))
        return ab_fail(error, "Settings array capacity exhausted");
    array->array = true;
    array->data.entries[array->data.count] =
        (AbDataEntry){number_value((float)(array->data.count + 1)), value};
    ++array->data.count;
    return true;
}

bool ab_settings_statistics_pairs(const AbSettingsStatistics *statistics,
                                  const AbSpan *path, size_t count,
                                  AbDataEntry *entries, size_t capacity,
                                  size_t *output_count, AbError *error) {
    if (!statistics || !output_count || count > 3 || (count && !path) ||
        (capacity && !entries))
        return ab_fail(error, "Invalid settings dictionary snapshot");
    AbDataTable *data = &statistics->tables[0].data;
    for (size_t index = 0; index < count; ++index) {
        if (!valid_span(path[index]))
            return ab_fail(error, "Invalid settings snapshot key");
        AbDataValue value = get(data, path[index]);
        if (value.kind != AB_DATA_TABLE || !value.table)
            return ab_fail(error, "Settings snapshot indexes a non-table");
        data = value.table;
    }
    const StatisticsTable *table = owned_table(statistics, data);
    if (!table || table->array || table->data.count > capacity)
        return ab_fail(error, "Settings snapshot requires a bounded owned dictionary");
    size_t cursor = 0, written = 0;
    for (;;) {
        AbStringMapEntry entry;
        if (!ab_string_map_next(table->map, &cursor, &entry, error))
            return false;
        if (!entry.present)
            break;
        if (!entry.value || entry.value > table->data.count || written == capacity)
            return ab_fail(error, "Invalid settings snapshot entry handle");
        entries[written++] = table->data.entries[entry.value - 1];
    }
    *output_count = written;
    return true;
}

bool ab_settings_statistics_table_next(const AbSettingsStatistics *statistics,
                                       const AbDataTable *data, size_t *cursor,
                                       AbDataEntry *output, bool *present,
                                       AbError *error) {
    if (!statistics || !data || !cursor || !output || !present)
        return ab_fail(error, "Invalid settings table iteration input");
    const StatisticsTable *table = owned_table(statistics, data);
    if (!table)
        return ab_fail(error, "Settings table iteration requires an owned table");
    if (table->array) {
        *present = *cursor < data->count;
        if (*present)
            *output = data->entries[(*cursor)++];
        return true;
    }
    AbStringMapEntry entry;
    if (!ab_string_map_next(table->map, cursor, &entry, error))
        return false;
    *present = entry.present;
    if (!entry.present)
        return true;
    if (!entry.value || entry.value > data->count)
        return ab_fail(error, "Invalid settings table iteration entry handle");
    *output = data->entries[entry.value - 1];
    return true;
}

static StatisticsTable *started_table(AbSettingsStatistics *statistics,
                                      AbError *error) {
    AbSpan key = {(const uint8_t *)"levelStarted", 12};
    AbDataValue value = get(ab_settings_statistics_root(statistics), key);
    if (value.kind == AB_DATA_NIL) {
        if (!ab_settings_statistics_create_table(statistics, &key, 1, 0, error))
            return NULL;
        value = get(ab_settings_statistics_root(statistics), key);
    }
    StatisticsTable *table =
        value.kind == AB_DATA_TABLE ? owned_table(statistics, value.table) : NULL;
    if (!table || table->array) {
        ab_fail(error, "Level-start records require an owned string dictionary");
        return NULL;
    }
    return table;
}

bool ab_settings_statistics_start_level(AbSettingsStatistics *statistics, AbSpan name,
                                        const AbDataTable **local_table,
                                        AbDataValue *previous, AbError *error) {
    if (!statistics || !valid_span(name) || !local_table || !previous)
        return ab_fail(error, "Invalid level-start statistics input");
    StatisticsTable *root = started_table(statistics, error);
    if (!root)
        return false;
    AbSpan path[] = {
        {(const uint8_t *)"levelStarted", 12}, statistics->installation, name};
    AbDataValue local = get(&root->data, path[1]);
    if (local.kind == AB_DATA_NIL) {
        if (!ab_settings_statistics_create_table(statistics, path, 2, 0, error))
            return false;
        local = get(&root->data, path[1]);
    }
    StatisticsTable *owned =
        local.kind == AB_DATA_TABLE ? owned_table(statistics, local.table) : NULL;
    if (!owned || owned->array)
        return ab_fail(error, "Local level-start records are not a dictionary");
    *local_table = &owned->data;
    *previous = get(*local_table, name);
    float count = 1;
    if (truthy(*previous)) {
        if (!finite_number(*previous, &count) || !isfinite(count + 1))
            return ab_fail(error, "Level-start count cannot be incremented");
        count += 1;
    }
    return ab_settings_statistics_write(statistics, path, 3, number_value(count),
                                        error);
}

bool ab_settings_statistics_level_started_count(AbSettingsStatistics *statistics,
                                                AbSpan name, float *output,
                                                AbError *error) {
    if (!statistics || !valid_span(name) || !output)
        return ab_fail(error, "Invalid level-start count input/output");
    StatisticsTable *root = started_table(statistics, error);
    if (!root)
        return false;
    size_t cursor = 0;
    float sum = 0;
    for (;;) {
        AbDataEntry row = {0};
        bool present = false;
        if (!ab_settings_statistics_table_next(statistics, &root->data, &cursor, &row,
                                               &present, error))
            return false;
        if (!present)
            break;
        StatisticsTable *record = row.value.kind == AB_DATA_TABLE
                                      ? owned_table(statistics, row.value.table)
                                      : NULL;
        if (!record || record->array)
            return ab_fail(error, "Level-start installation is not a dictionary");
        AbDataValue count = get(&record->data, name);
        if (count.kind == AB_DATA_NIL)
            continue;
        float amount = 0;
        if (!finite_number(count, &amount) || !isfinite(sum + amount))
            return ab_fail(error, "Level-start count cannot be summed");
        sum += amount;
    }
    *output = sum;
    return true;
}

bool ab_settings_statistics_extra_tutorial(AbSettingsStatistics *statistics,
                                           AbDataValue item, AbError *error) {
    if (!statistics)
        return ab_fail(error, "Missing tutorial settings storage");
    AbSpan path[] = {
        {(const uint8_t *)"tutorials", 9}, {0}, {(const uint8_t *)"showHelp", 8}};
    AbDataValue tutorials = get(&statistics->tables[0].data, path[0]);
    if (tutorials.kind == AB_DATA_NIL) {
        if (!ab_settings_statistics_create_table(statistics, path, 1, 0, error))
            return false;
        tutorials = get(&statistics->tables[0].data, path[0]);
    }
    if (tutorials.kind != AB_DATA_TABLE || !owned_table(statistics, tutorials.table))
        return ab_fail(error, "Tutorial settings indexes a non-owned table");
    if (item.kind != AB_DATA_STRING || !valid_span(item.string))
        return ab_fail(error, "Unsupported tutorial item key");
    path[1] = item.string;
    AbDataValue record = get(tutorials.table, path[1]);
    if (!truthy(record))
        return true;
    if (record.kind != AB_DATA_TABLE || !owned_table(statistics, record.table))
        return ab_fail(error, "Tutorial item indexes a non-owned table");
    if (!truthy(get(record.table, path[2])))
        return true;
    return ab_settings_statistics_write(statistics, path, 3,
                                        (AbDataValue){.kind = AB_DATA_BOOLEAN}, error);
}

bool ab_settings_statistics_get_local(const AbSettingsStatistics *statistics,
                                      AbSpan counter, AbDataValue *output,
                                      AbError *error) {
    if (!statistics || !output || !valid_span(counter))
        return ab_fail(error, "Invalid local statistics query");
    AbDataValue root = get(&statistics->tables[0].data, counter);
    AbDataValue device = {0}, result = {0};
    if (root.kind != AB_DATA_NIL) {
        if (root.kind != AB_DATA_TABLE)
            return ab_fail(error, "Local statistics root is not a table");
        device = get(root.table, statistics->device);
        if (device.kind != AB_DATA_NIL) {
            if (device.kind != AB_DATA_TABLE)
                return ab_fail(error, "Local statistics device is not a table");
            result = get(device.table, statistics->installation);
        }
    }
    *output = truthy(result) ? result : number_value(0);
    return true;
}

bool ab_settings_statistics_set_local(AbSettingsStatistics *statistics, AbSpan counter,
                                      AbDataValue value, AbError *error) {
    if (!statistics)
        return ab_fail(error, "Missing local statistics storage");
    AbSpan path[] = {counter, statistics->device, statistics->installation};
    return ab_settings_statistics_write(statistics, path, 3, value, error);
}

bool ab_settings_statistics_increment(AbSettingsStatistics *statistics, AbSpan counter,
                                      AbError *error) {
    AbDataValue value = {0};
    float number;
    if (!ab_settings_statistics_get_local(statistics, counter, &value, error))
        return false;
    if (!finite_number(value, &number) || !isfinite(number + 1.0f))
        return ab_fail(error, "Unsupported statistics increment operand");
    return ab_settings_statistics_set_local(statistics, counter,
                                            number_value(number + 1.0f), error);
}

bool ab_settings_statistics_device_sum(const AbSettingsStatistics *statistics,
                                       AbSpan counter, float *output, AbError *error) {
    if (!statistics || !output || !valid_span(counter))
        return ab_fail(error, "Invalid device statistics query");
    AbDataValue root = get(&statistics->tables[0].data, counter);
    float result = 0;
    if (truthy(root)) {
        if (root.kind != AB_DATA_TABLE)
            return ab_fail(error, "Device statistics root is not a table");
        AbDataValue device = get(root.table, statistics->device);
        if (device.kind == AB_DATA_TABLE) {
            StatisticsTable *table = owned_table(statistics, device.table);
            if (!table || table->array)
                return ab_fail(error, "Device statistics require an owned dictionary");
            size_t cursor = 0;
            for (;;) {
                AbStringMapEntry entry;
                if (!ab_string_map_next(table->map, &cursor, &entry, error))
                    return false;
                if (!entry.present)
                    break;
                float number;
                if (!finite_number(table->data.entries[entry.value - 1].value,
                                   &number) ||
                    !isfinite(result + number))
                    return ab_fail(error, "Unsupported device statistics sum operand");
                result = result + number;
            }
        }
    }
    *output = result;
    return true;
}
