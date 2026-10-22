#include "ab/target_data.h"
#include "ab/theme_colors.h"
#include "native_data_fixture.h"

#include <stdio.h>
#include <string.h>

static const AbDataValue *property(const AbDataTable *table, const char *name) {
    return ab_data_get(table,
                       (AbDataValue){.kind = AB_DATA_STRING,
                                     .string = {(const uint8_t *)name, strlen(name)}});
}

int main(int argc, char **argv) {
    if (argc < 2 || argc > 3) {
        return 2;
    }
    AbBuffer bytes = {0};
    AbZip archive = {0};
    AbTargetData themes = {0};
    AbError error = {0};
    uint8_t key[32] = {0};
    bool okay = ab_file_read(argv[1], 64 * 1024 * 1024, &bytes, &error) &&
                ab_zip_open((AbSpan){bytes.data, bytes.size}, &archive, &error) &&
                ab_target_resource_key(&archive, 16 * 1024 * 1024, key, &error) &&
                ab_target_data_open(&archive, "assets/data/scripts/themes.lua",
                                    (AbSpan){key, sizeof(key)}, 4 * 1024 * 1024, NULL,
                                    &themes, &error);
    if (okay) {
        const AbDataValue *settings = property(themes.data.globals, "settings");
        const AbDataValue *amount = settings && settings->kind == AB_DATA_TABLE
                                        ? property(settings->table, "themeAmount")
                                        : NULL;
        okay = themes.data.globals->count == 32 && amount &&
               amount->kind == AB_DATA_NUMBER && amount->bits == UINT32_C(0x41f80000);
    }
    size_t layers = 0;
    for (size_t index = 0; okay && index < themes.data.globals->count; ++index) {
        const AbDataEntry *entry = themes.data.globals->entries + index;
        if (entry->key.kind == AB_DATA_STRING && entry->key.string.size == 8 &&
            memcmp(entry->key.string.data, "settings", 8) == 0) {
            continue;
        }
        const AbDataValue *background = entry->value.kind == AB_DATA_TABLE
                                            ? property(entry->value.table, "bgLayers")
                                            : NULL;
        const AbDataValue *foreground = entry->value.kind == AB_DATA_TABLE
                                            ? property(entry->value.table, "fgLayers")
                                            : NULL;
        AbThemeColors colors;
        okay = background && background->kind == AB_DATA_TABLE && foreground &&
               foreground->kind == AB_DATA_TABLE &&
               ab_theme_colors(entry->value.table, &colors, &error);
        if (okay) {
            layers += background->table->count + foreground->table->count;
        }
    }
    if (okay && argc == 3) {
        FILE *file = fopen(argv[2], "rb");
        AbTestNativeData reader = {.file = file};
        okay = file && native_data_compare(&reader, themes.data.globals);
        if (file && fclose(file) != 0) {
            okay = false;
        }
        if (okay) {
            printf("Theme catalog matches %zu native typed values in %zu tables.\n",
                   reader.values, reader.tables);
        }
    }
    if (okay) {
        printf("Static APK catalog contains 31 themes and %zu background/foreground "
               "layers; no Lua runtime executed.\n",
               layers);
    } else {
        fprintf(stderr, "Theme catalog comparison failed: %s\n", error.message);
    }
    memset(key, 0, sizeof(key));
    ab_target_data_close(&themes);
    ab_zip_close(&archive);
    ab_buffer_free(&bytes);
    return okay ? 0 : 1;
}
