#include "ab/settings_inventory.h"

#include <math.h>
#include <string.h>

static AbSpan span(const char *name) {
    return (AbSpan){(const uint8_t *)name, strlen(name)};
}

static AbDataValue get(const AbDataTable *table, AbSpan name) {
    const AbDataValue *value =
        ab_data_get(table, (AbDataValue){.kind = AB_DATA_STRING, .string = name});
    return value ? *value : (AbDataValue){0};
}

static AbDataValue number(float value) {
    AbDataValue output = {.kind = AB_DATA_NUMBER};
    memcpy(&output.bits, &value, sizeof(value));
    return output;
}

static bool numeric(AbDataValue value, float *output, AbError *error) {
    if (value.kind == AB_DATA_NUMBER) {
        memcpy(output, &value.bits, sizeof(*output));
        if (isfinite(*output))
            return true;
    }
    return ab_fail(error, "Inventory arithmetic requires finite numeric counts");
}

static bool item_field(AbDataValue item, const char *name, AbDataValue *output,
                       AbError *error) {
    if (item.kind != AB_DATA_TABLE || !item.table)
        return ab_fail(error, "Inventory operation indexes a non-table item");
    *output = get(item.table, span(name));
    return true;
}

bool ab_settings_inventory_item(AbSettingsStatistics *settings, AbSpan name,
                                AbDataValue *output, AbError *error) {
    const AbDataTable *root = ab_settings_statistics_root(settings);
    if (!root || !output || (name.size && !name.data))
        return ab_fail(error, "Invalid actual inventory binding/name/output");
    AbSpan path[] = {span("installationSpecificInventory"), name, span("totalCount")};
    AbDataValue inventory = get(root, path[0]);
    if (inventory.kind == AB_DATA_NIL) {
        if (!ab_settings_statistics_create_table(settings, path, 1, 0, error))
            return false;
        inventory = get(root, path[0]);
    }
    if (inventory.kind != AB_DATA_TABLE || !inventory.table)
        return ab_fail(error, "Inventory lookup indexes a non-table source inventory");
    AbDataValue item = get(inventory.table, name);
    if (item.kind == AB_DATA_NIL) {
        if (!ab_settings_statistics_create_table(settings, path, 2, 2, error) ||
            !ab_settings_statistics_write(settings, path, 3, number(0), error))
            return false;
        path[2] = span("usedCount");
        if (!ab_settings_statistics_write(settings, path, 3, number(0), error))
            return false;
    }
    *output = get(inventory.table, name);
    return true;
}

bool ab_settings_inventory_amount(AbSettingsStatistics *settings, AbSpan name,
                                  float *output, AbError *error) {
    if (!output)
        return ab_fail(error, "Missing inventory amount output");
    AbDataValue item = {0}, total = {0}, used = {0};
    if (!ab_settings_inventory_item(settings, name, &item, error) ||
        !item_field(item, "totalCount", &total, error) ||
        !ab_settings_inventory_item(settings, name, &item, error) ||
        !item_field(item, "usedCount", &used, error))
        return false;
    float total_count = 0, used_count = 0;
    if (!numeric(total, &total_count, error) || !numeric(used, &used_count, error))
        return false;
    float amount = total_count - used_count;
    if (!isfinite(amount))
        return ab_fail(error, "Inventory amount overflow");
    *output = amount;
    return true;
}

static bool assign(AbSettingsStatistics *settings, AbSpan name, const char *field,
                   AbDataValue value, AbError *error) {
    AbSpan path[] = {span("installationSpecificInventory"), name, span(field)};
    return ab_settings_statistics_write(settings, path, 3, value, error);
}

bool ab_settings_inventory_set(AbSettingsStatistics *settings, AbSpan name,
                               AbDataValue total, AbDataValue used, AbError *error) {
    AbDataValue item = {0};
    return ab_settings_inventory_item(settings, name, &item, error) &&
           assign(settings, name, "totalCount", total, error) &&
           ab_settings_inventory_item(settings, name, &item, error) &&
           assign(settings, name, "usedCount", used, error);
}

bool ab_settings_inventory_consume(AbSettingsStatistics *settings, AbSpan name,
                                   AbError *error) {
    AbDataValue item = {0}, used = {0};
    /* Retain both getItem calls from the source before the arithmetic/write. */
    if (!ab_settings_inventory_item(settings, name, &item, error) ||
        !ab_settings_inventory_item(settings, name, &item, error) ||
        !item_field(item, "usedCount", &used, error))
        return false;
    float count = 0;
    if (!numeric(used, &count, error))
        return false;
    return assign(settings, name, "usedCount", number(count + 1), error);
}

bool ab_settings_inventory_add(AbSettingsStatistics *settings, AbSpan name,
                               AbDataValue amount, AbError *error) {
    AbDataValue item = {0}, total = {0};
    if (!ab_settings_inventory_item(settings, name, &item, error) ||
        !ab_settings_inventory_item(settings, name, &item, error) ||
        !item_field(item, "totalCount", &total, error))
        return false;
    float current = 0, increment = 0;
    if (!numeric(total, &current, error) || !numeric(amount, &increment, error))
        return false;
    return assign(settings, name, "totalCount", number(current + increment), error);
}
