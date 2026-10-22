#include "ab/powerup_catalog.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Powerup catalog check failed at %d: %s\n", __LINE__,      \
                    #condition);                                                       \
            goto release;                                                              \
        }                                                                              \
    } while (0)

static AbDataValue text(const char *name) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)name, strlen(name)}};
}

static bool named(AbSpan value, const char *name) {
    return value.size == strlen(name) && !memcmp(value.data, name, value.size);
}

static AbDataValue field(const AbDataTable *table, const char *name) {
    const AbDataValue *value = ab_data_get(table, text(name));
    return value ? *value : (AbDataValue){0};
}

static bool check(const char *apk) {
    AbBuffer bytes = {0};
    AbZip archive = {0};
    AbPowerupCatalog *catalog = NULL;
    AbError error = {0};
    uint8_t key[32] = {0};
    bool okay = false;
    CHECK(ab_file_read(apk, 64 * 1024 * 1024, &bytes, &error) &&
          ab_zip_open((AbSpan){bytes.data, bytes.size}, &archive, &error) &&
          ab_target_resource_key(&archive, 16 * 1024 * 1024, key, &error));
    const char *const names[] = {"powerpotion", "kingsling", "slingscope",
                                 "birdquake",   "shockwave", "redsmightyfeathers"};
    AbDataEntry overrides[] = {{text("powerpotion"), {.kind = AB_DATA_NUMBER}},
                               {text("birdquake"), text("retained")},
                               {text("shockwave"), {.kind = AB_DATA_BOOLEAN}}};
    AbDataTable table = {overrides, 3, 3, NULL};
    for (unsigned variant = 0; variant < 2; ++variant) {
        AbDataValue override =
            variant ? (AbDataValue){.kind = AB_DATA_TABLE, .table = &table}
                    : (AbDataValue){0};
        CHECK(ab_powerup_catalog_open(&archive, (AbSpan){key, sizeof(key)}, override,
                                      1024 * 1024, &catalog, &error) &&
              ab_powerup_catalog_count(catalog) == 6);
        for (size_t index = 0; index < 6; ++index) {
            const AbPowerupMetadata *row = ab_powerup_catalog_at(catalog, index);
            CHECK(row && named(row->name, names[index]) && row->definition &&
                  field(row->definition, "filePath").kind == AB_DATA_STRING &&
                  field(row->definition, "hudIcon").kind == AB_DATA_STRING &&
                  row->reward_probability == (index < 4 ? 25 : 0) &&
                  row->infinite.kind == AB_DATA_BOOLEAN &&
                  !!row->infinite.bits == (index == 5));
            AbDataValue visible = field(row->definition, "showInPowerupBar");
            CHECK(index < 4 ? visible.kind == AB_DATA_NIL
                            : visible.kind == AB_DATA_BOOLEAN && !visible.bits);
            if (variant && index == 0)
                CHECK(row->initial_amount.kind == AB_DATA_NUMBER &&
                      !row->initial_amount.bits);
            else if (variant && index == 3)
                CHECK(row->initial_amount.kind == AB_DATA_STRING &&
                      row->initial_amount.string.data ==
                          overrides[1].value.string.data);
            else
                CHECK(row->initial_amount.kind == AB_DATA_NUMBER &&
                      row->initial_amount.bits == UINT32_C(0x40a00000));
        }
        CHECK(!ab_powerup_catalog_at(catalog, 6));
        ab_powerup_catalog_close(catalog);
        catalog = NULL;
    }
    CHECK(!ab_powerup_catalog_open(&archive, (AbSpan){key, sizeof(key)},
                                   (AbDataValue){.kind = AB_DATA_NUMBER}, 1024 * 1024,
                                   &catalog, NULL) &&
          !catalog &&
          !ab_powerup_catalog_open(&archive, (AbSpan){key, sizeof(key)},
                                   (AbDataValue){0}, 128, &catalog, NULL) &&
          !catalog && !ab_powerup_catalog_count(NULL));
    puts("Actual six-item Android powerup catalog preserves resource metadata, "
         "activation order, reward weights and raw initial overrides.");
    okay = true;
release:
    if (!okay && error.message[0])
        fprintf(stderr, "%s\n", error.message);
    ab_powerup_catalog_close(catalog);
    ab_zip_close(&archive);
    ab_buffer_free(&bytes);
    memset(key, 0, sizeof(key));
    return okay;
}

int main(int argc, char **argv) {
    return argc == 2 ? (check(argv[1]) ? 0 : 1) : 2;
}
