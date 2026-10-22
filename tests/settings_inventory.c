#include "ab/settings_inventory.h"

#include <float.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Inventory check failed at %d: %s\n", __LINE__,            \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static AbSpan span(const char *text) {
    return (AbSpan){(const uint8_t *)text, strlen(text)};
}

static AbDataValue number(float value) {
    AbDataValue output = {.kind = AB_DATA_NUMBER};
    memcpy(&output.bits, &value, sizeof(value));
    return output;
}

static bool balances(AbSettingsStatistics *settings, AbError *error) {
    AbSpan name = span("powerpotion");
    AbDataValue item;
    float amount = 123;
    CHECK(ab_settings_inventory_amount(settings, name, &amount, error) && amount == 0 &&
          ab_settings_inventory_item(settings, name, &item, error) &&
          item.kind == AB_DATA_TABLE && item.table->count == 2);
    AbDataTable *identity = item.table;
    CHECK(ab_settings_inventory_set(settings, name, number(5), number(2), error) &&
          ab_settings_inventory_amount(settings, name, &amount, error) && amount == 3 &&
          ab_settings_inventory_consume(settings, name, error) &&
          ab_settings_inventory_amount(settings, name, &amount, error) && amount == 2 &&
          ab_settings_inventory_add(settings, name, number(1.25f), error) &&
          ab_settings_inventory_amount(settings, name, &amount, error) &&
          amount == 3.25f && ab_settings_inventory_item(settings, name, &item, error) &&
          item.table == identity);
    CHECK(ab_settings_inventory_set(settings, name, number(0), number(0), error) &&
          ab_settings_inventory_consume(settings, name, error) &&
          ab_settings_inventory_amount(settings, name, &amount, error) && amount == -1);
    CHECK(
        ab_settings_inventory_set(settings, name, number(FLT_MAX), number(1), error) &&
        !ab_settings_inventory_add(settings, name, number(FLT_MAX), NULL) &&
        ab_settings_inventory_amount(settings, name, &amount, error) &&
        amount == FLT_MAX);
    AbDataValue unsupported = {.kind = AB_DATA_TABLE, .table = identity};
    CHECK(!ab_settings_inventory_set(settings, name, number(10), unsupported, NULL) &&
          ab_settings_inventory_amount(settings, name, &amount, error) && amount == 9);
    AbSpan path[] = {span("installationSpecificInventory"), span("slingscope")};
    CHECK(ab_settings_statistics_write(settings, path, 2,
                                       (AbDataValue){.kind = AB_DATA_BOOLEAN}, error) &&
          ab_settings_inventory_item(settings, path[1], &item, error) &&
          item.kind == AB_DATA_BOOLEAN && !item.bits);
    amount = 123;
    CHECK(!ab_settings_inventory_amount(settings, path[1], &amount, NULL) &&
          amount == 123);
    path[1] = span("birdquake");
    CHECK(!ab_settings_inventory_add(settings, path[1], unsupported, NULL) &&
          ab_settings_inventory_amount(settings, path[1], &amount, error) &&
          amount == 0);
    path[0] = span("installationSpecificInventory");
    CHECK(ab_settings_statistics_write(settings, path, 1,
                                       (AbDataValue){.kind = AB_DATA_BOOLEAN}, error) &&
          !ab_settings_inventory_item(settings, name, &item, NULL));
    return true;
}

int main(void) {
    AbSettingsStatistics *settings = NULL;
    AbError error = {0};
    bool okay = ab_settings_statistics_open(
        (AbSettingsStatisticsConfig){span("inventory-test"), span("local"), 16, 8,
                                     65536},
        &settings, &error);
    if (okay)
        okay = balances(settings, &error);
    ab_settings_statistics_close(settings);
    if (!okay) {
        fprintf(stderr, "%s\n", error.message);
        return 1;
    }
    puts("Inventory preserves lazy creation, raw records and unclamped balances.");
    return 0;
}
