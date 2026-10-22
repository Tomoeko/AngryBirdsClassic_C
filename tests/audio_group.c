#include "ab/audio_group.h"
#include "ab/audio_catalog.h"
#include "ab/audio_definition.h"
#include "ab/target_resources.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Audio group failed at %d: %s\n", __LINE__, #condition);   \
            return false;                                                              \
        }                                                                              \
    } while (0)

static AbDataValue number(float value) {
    AbDataValue result = {.kind = AB_DATA_NUMBER};
    memcpy(&result.bits, &value, sizeof(value));
    return result;
}

static AbDataValue string(const char *text) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)text, strlen(text)}};
}

static AbDataValue requested(uint32_t code) {
    return code == 0   ? (AbDataValue){0}
           : code == 1 ? (AbDataValue){.kind = AB_DATA_BOOLEAN}
           : code == 2 ? number(0)
                       : string(code == 3 ? "request" : "absent");
}

static bool same_value(AbDataValue first, AbDataValue second) {
    return first.kind == second.kind &&
           (first.kind == AB_DATA_STRING
                ? first.string.size == second.string.size &&
                      (!first.string.size ||
                       memcmp(first.string.data, second.string.data,
                              first.string.size) == 0)
                : first.bits == second.bits);
}

static bool words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, output + index) == 1);
        CHECK(fgetc(file) == (index + 1 == count ? '\n' : ' '));
    }
    return true;
}

static bool native_checks(FILE *file) {
    char header[80];
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB_ANDROID_AUDIO_GROUP_V1 4096\n") == 0);
    size_t draws = 0, errors = 0, nil_choices = 0;
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[6], expected[5];
        CHECK(words(file, input, 6) && words(file, expected, 5) && input[1] <= 65535 &&
              input[2] <= 4 && input[3] <= 1 && input[4] <= 16 && expected[2] <= 65535);
        AbRandom48 random = {input[0] | (uint64_t)input[1] << 32};
        AbDataEntry variants[16];
        char names[16][16];
        for (size_t index = 0; index < input[4]; ++index) {
            uint32_t kind = ((uint32_t)index + input[5]) % 5;
            CHECK(snprintf(names[index], sizeof(names[index]), "variant%zu", index) >
                  0);
            variants[index] =
                (AbDataEntry){number((float)(index + 1)),
                              kind == 0   ? (AbDataValue){.kind = AB_DATA_BOOLEAN}
                              : kind == 1 ? number(0)
                                          : string(names[index])};
        }
        AbDataTable group = {variants, input[4], 16, NULL};
        AbDataEntry entry = {string("request"),
                             {.kind = AB_DATA_TABLE, .table = &group}};
        AbDataTable groups = {&entry, input[3], 1, NULL};
        AbDataValue output = number(99);
        AbError error = {0};
        bool okay =
            ab_audio_group_name(&groups, requested(input[2]), &random, &output, &error);
        bool present = input[2] == 3 && input[3] != 0;
        CHECK(expected[0] == (present && !input[4] ? 2u : 0u));
        if (present) {
            ++draws;
        }
        if (!okay) {
            ++errors;
            CHECK(expected[0] == 2 && same_value(output, number(99)));
        } else {
            CHECK(expected[0] == 0);
            AbDataValue selected = {0};
            if (expected[3] == 1) {
                CHECK(!expected[4]);
                selected.kind = AB_DATA_BOOLEAN;
            } else if (expected[3] == 3) {
                selected = (AbDataValue){.kind = AB_DATA_NUMBER, .bits = expected[4]};
            } else if (expected[3] == 4) {
                CHECK((expected[4] >= 1 && expected[4] <= input[4]) ||
                      expected[4] == 256 || expected[4] == 257);
                selected = expected[4] == 256   ? string("request")
                           : expected[4] == 257 ? string("absent")
                                                : string(names[expected[4] - 1]);
            } else {
                CHECK(expected[3] == 0 && !expected[4]);
                nil_choices += present;
            }
            if (!same_value(output, selected)) {
                fprintf(stderr,
                        "Native audio-group case %zu selected a different value\n",
                        item);
                return false;
            }
        }
        CHECK(random.value == (expected[1] | (uint64_t)expected[2] << 32));
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB audio group reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("All 4096 native audio choices/RNG states match: %zu draws, %zu empty-group "
           "errors, %zu nil overshoots\n",
           draws, errors, nil_choices);
    return true;
}

static bool read_label(FILE *file, char bytes[256], size_t *size) {
    uint32_t length;
    CHECK(words(file, &length, 1) && length > 0 && length <= 255 &&
          fread(bytes, 1, length, file) == length && fgetc(file) == '\n');
    *size = length;
    return true;
}

static bool catalog_matches(FILE *file, const AbAudioCatalog *catalog) {
    char header[80];
    CHECK(catalog->groups && catalog->groups->count == 46 &&
          catalog->data.globals->count == 1 && fgets(header, sizeof(header), file) &&
          strcmp(header, "AB_ANDROID_AUDIO_CATALOG_V1 46 196\n") == 0);
    bool seen[46] = {0};
    size_t variants = 0;
    for (size_t group = 0; group < 46; ++group) {
        char bytes[256];
        size_t size;
        CHECK(read_label(file, bytes, &size));
        AbDataValue key = {.kind = AB_DATA_STRING,
                           .string = {(const uint8_t *)bytes, size}};
        const AbDataValue *value = ab_data_get(catalog->groups, key);
        CHECK(value && value->kind == AB_DATA_TABLE && value->table);
        size_t index = 0;
        while (index < 46 && &catalog->groups->entries[index].value != value) {
            ++index;
        }
        CHECK(index < 46 && !seen[index]);
        seen[index] = true;
        uint32_t count;
        CHECK(words(file, &count, 1) && count > 0 && count <= 12 &&
              value->table->count == count);
        for (uint32_t item = 0; item < count; ++item) {
            CHECK(read_label(file, bytes, &size));
            const AbDataValue *variant =
                ab_data_get(value->table, number((float)(item + 1)));
            CHECK(variant && variant->kind == AB_DATA_STRING &&
                  variant->string.size == size &&
                  memcmp(variant->string.data, bytes, size) == 0);
        }
        variants += count;
    }
    CHECK(variants == 196 && fgets(header, sizeof(header), file) &&
          strcmp(header, "AB audio catalog reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    puts("All 46 original audio-group names and 196 ordered variants match without Lua "
         "execution");
    return true;
}

static bool registrations_match(FILE *file, const AbAudioCatalog *catalog) {
    char header[80];
    CHECK(catalog->registrations && catalog->registrations->count == 340 &&
          fgets(header, sizeof(header), file) &&
          strcmp(header, "AB_ANDROID_AUDIO_ASSETS_V1 340\n") == 0);
    size_t properties = 0;
    for (size_t item = 0; item < 340; ++item) {
        const AbDataValue *entry =
            ab_data_get(catalog->registrations, number((float)(item + 1)));
        uint32_t count;
        CHECK(entry && entry->kind == AB_DATA_TABLE && entry->table &&
              words(file, &count, 1) && count >= 2 && count <= 6 &&
              entry->table->count == count);
        bool seen[6] = {0};
        for (size_t field = 0; field < count; ++field) {
            char bytes[256];
            size_t size;
            uint32_t value_words[2];
            CHECK(read_label(file, bytes, &size));
            AbDataValue key = {.kind = AB_DATA_STRING,
                               .string = {(const uint8_t *)bytes, size}};
            const AbDataValue *value = ab_data_get(entry->table, key);
            size_t index = 0;
            while (index < count && &entry->table->entries[index].value != value) {
                ++index;
            }
            CHECK(index < count && !seen[index] && words(file, value_words, 2));
            seen[index] = true;
            AbDataValue expected = {.bits = value_words[1]};
            if (value_words[0] == 4) {
                CHECK(!value_words[1] && read_label(file, bytes, &size));
                expected.kind = AB_DATA_STRING;
                expected.string = (AbSpan){(const uint8_t *)bytes, size};
            } else if (value_words[0] == 3) {
                expected.kind = AB_DATA_NUMBER;
            } else {
                CHECK(value_words[0] == 1 && value_words[1] <= 1);
                expected.kind = AB_DATA_BOOLEAN;
            }
            CHECK(same_value(*value, expected));
        }
        properties += count;
    }
    CHECK(properties == 825 && fgets(header, sizeof(header), file) &&
          strcmp(header, "AB audio assets reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    puts("All 340 original audio registrations and 825 typed properties match in "
         "call order without Lua execution");
    return true;
}

static bool resources_match(FILE *file, const AbAudioCatalog *catalog) {
    char header[80];
    CHECK(catalog->registrations && catalog->registrations->count == 340 &&
          fgets(header, sizeof(header), file) &&
          strcmp(header, "AB_ANDROID_AUDIO_RESOURCES_V1 340\n") == 0);
    for (size_t item = 0; item < 340; ++item) {
        const AbDataValue *entry =
            ab_data_get(catalog->registrations, number((float)(item + 1)));
        CHECK(entry && entry->kind == AB_DATA_TABLE && entry->table);
        AbAudioDefinition definition;
        uint8_t storage[256];
        bool created = false;
        CHECK(ab_audio_definition_read(entry->table, (AbDataValue){0},
                                       string("audio").string, true, storage,
                                       sizeof(storage), &definition, &created, NULL) &&
              created);
        char bytes[256];
        size_t size;
        CHECK(read_label(file, bytes, &size) &&
              same_value(definition.name,
                         (AbDataValue){.kind = AB_DATA_STRING,
                                       .string = {(const uint8_t *)bytes, size}}));
        CHECK(read_label(file, bytes, &size) && definition.path.size == size &&
              memcmp(definition.path.data, bytes, size) == 0);
        uint32_t expected[9];
        CHECK(words(file, expected, 9) && expected[0] <= 1 &&
              definition.stream.kind == AB_DATA_BOOLEAN &&
              definition.stream.bits == expected[0]);
        const AbDataValue metadata[] = {definition.channel, definition.volume,
                                        definition.can_be_overwritten, definition.loop};
        for (size_t field = 0; field < 4; ++field) {
            uint32_t tag = expected[field * 2 + 1];
            CHECK(tag == 0 || tag == 1 || tag == 3);
            AbDataValue value = {.kind = tag == 0   ? AB_DATA_NIL
                                         : tag == 1 ? AB_DATA_BOOLEAN
                                                    : AB_DATA_NUMBER,
                                 .bits = expected[field * 2 + 2]};
            CHECK(same_value(metadata[field], value));
        }
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB audio resources reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    puts("All 340 original resolved paths, stream requests and stored audio metadata "
         "match");
    return true;
}

static bool resources_exist(const AbAudioCatalog *catalog, const AbZip *archive) {
    size_t mp3 = 0, wave = 0;
    for (size_t item = 0; item < 340; ++item) {
        const AbDataValue *entry =
            ab_data_get(catalog->registrations, number((float)(item + 1)));
        CHECK(entry && entry->kind == AB_DATA_TABLE && entry->table);
        uint8_t storage[256];
        AbAudioDefinition definition;
        bool created;
        CHECK(ab_audio_definition_read(entry->table, (AbDataValue){0},
                                       string("audio").string, true, storage,
                                       sizeof(storage), &definition, &created, NULL) &&
              created && definition.path.size > 4);
        char path[sizeof(storage) + 13] = "assets/data/";
        memcpy(path + 12, definition.path.data, definition.path.size);
        path[12 + definition.path.size] = 0;
        const AbZipEntry *asset = ab_zip_find(archive, path);
        CHECK(asset && asset->size);
        const uint8_t *extension = definition.path.data + definition.path.size - 4;
        mp3 += memcmp(extension, ".mp3", 4) == 0;
        wave += memcmp(extension, ".wav", 4) == 0;
    }
    CHECK(mp3 == 339 && wave == 1);
    puts("Every registered sound exists in the pinned APK: 339 MP3 and one WAV");
    return true;
}

typedef bool (*CatalogComparison)(FILE *, const AbAudioCatalog *);

static bool catalog_checks(const char *decoded_path, const char *reference_path,
                           CatalogComparison compare) {
    AbBuffer bytes = {0};
    AbAudioCatalog catalog = {0};
    AbError error = {0};
    FILE *file = NULL;
    bool okay = false;
    if (!ab_file_read(decoded_path, 65536, &bytes, &error)) {
        goto release_catalog;
    }
    AbSpan input = {bytes.data, bytes.size};
    if (ab_audio_catalog_read(input, 1, &catalog, &error) || catalog.groups) {
        goto release_catalog;
    }
    error = (AbError){0};
    file = fopen(reference_path, "rb");
    if (!file || !ab_audio_catalog_read(input, 1024 * 1024, &catalog, &error) ||
        !compare(file, &catalog)) {
        goto release_catalog;
    }
    size_t needed = catalog.chunk.allocated_bytes + catalog.data.allocated_bytes;
    ab_audio_catalog_close(&catalog);
    if (!needed || !ab_audio_catalog_read(input, needed, &catalog, &error)) {
        goto release_catalog;
    }
    ab_audio_catalog_close(&catalog);
    if (ab_audio_catalog_read(input, needed - 1, &catalog, &error) || catalog.groups ||
        catalog.chunk.input.data || catalog.data.owned) {
        goto release_catalog;
    }
    bytes.data[0] ^= 1;
    bool accepted_corruption =
        ab_audio_catalog_read(input, 1024 * 1024, &catalog, &error);
    bytes.data[0] ^= 1;
    if (accepted_corruption || catalog.groups) {
        goto release_catalog;
    }
    okay = true;

release_catalog:
    if (!okay && error.message[0]) {
        fprintf(stderr, "Audio catalog check: %s\n", error.message);
    }
    if (file && fclose(file) != 0) {
        okay = false;
    }
    ab_audio_catalog_close(&catalog);
    ab_buffer_free(&bytes);
    return okay;
}

static bool apk_catalog_checks(const char *apk_path, const char *reference_path,
                               CatalogComparison compare) {
    AbBuffer bytes = {0};
    AbZip archive = {0};
    AbAudioCatalog catalog = {0};
    AbError error = {0};
    uint8_t key[32] = {0};
    FILE *file = NULL;
    bool okay = false;
    if (!ab_file_read(apk_path, 64 * 1024 * 1024, &bytes, &error) ||
        !ab_zip_open((AbSpan){bytes.data, bytes.size}, &archive, &error) ||
        !ab_target_resource_key(&archive, 32 * 1024 * 1024, key, &error)) {
        goto release_catalog;
    }
    if (ab_audio_catalog_open(&archive, (AbSpan){key, sizeof(key)}, 1, &catalog,
                              &error) ||
        catalog.groups || catalog.owned_bytes.data) {
        goto release_catalog;
    }
    error = (AbError){0};
    file = fopen(reference_path, "rb");
    if (!file ||
        !ab_audio_catalog_open(&archive, (AbSpan){key, sizeof(key)}, 1024 * 1024,
                               &catalog, &error) ||
        !catalog.owned_bytes.data ||
        catalog.chunk.input.data != catalog.owned_bytes.data ||
        !compare(file, &catalog) ||
        (compare == resources_match && !resources_exist(&catalog, &archive))) {
        goto release_catalog;
    }
    puts("Loaded audio catalog data directly from the pinned APK");
    okay = true;

release_catalog:
    memset(key, 0, sizeof(key));
    if (!okay && error.message[0]) {
        fprintf(stderr, "APK audio catalog check: %s\n", error.message);
    }
    if (file && fclose(file) != 0) {
        okay = false;
    }
    ab_audio_catalog_close(&catalog);
    ab_zip_close(&archive);
    ab_buffer_free(&bytes);
    return okay;
}

static bool edge_checks(void) {
    AbAudioCatalog catalog = {0};
    CHECK(!ab_audio_catalog_read((AbSpan){0}, 0, NULL, NULL));
    CHECK(!ab_audio_catalog_read((AbSpan){0}, 0, &catalog, NULL));
    CHECK(!catalog.groups && !catalog.chunk.input.data && !catalog.data.owned);
    ab_audio_catalog_close(&catalog);
    ab_audio_catalog_close(NULL);
    CHECK(!ab_audio_catalog_open(NULL, (AbSpan){0}, 0, NULL, NULL));
    CHECK(!ab_audio_catalog_open(NULL, (AbSpan){0}, 0, &catalog, NULL));
    CHECK(!catalog.groups && !catalog.owned_bytes.data);
    AbRandom48 random, before;
    CHECK(ab_random48_seed(&random, 5));
    before = random;
    AbDataValue output = number(99);
    AbDataTable empty = {0};
    CHECK(!ab_audio_group_name(NULL, string("missing"), &random, &output, NULL));
    CHECK(!ab_audio_group_name(&empty, string("missing"), &random, NULL, NULL));
    CHECK(ab_audio_group_name(&empty, string("missing"), NULL, &output, NULL) &&
          same_value(output, string("missing")) && random.value == before.value);
    AbDataEntry variant = {number(2), string("sound")};
    AbDataTable group = {&variant, 1, 1, NULL};
    AbDataEntry entry = {string("request"), {.kind = AB_DATA_TABLE, .table = &group}};
    AbDataTable groups = {&entry, 1, 1, NULL};
    output = number(99);
    CHECK(!ab_audio_group_name(&groups, string("request"), &random, &output, NULL) &&
          same_value(output, number(99)) && random.value == before.value);
    group.count = 0;
    CHECK(!ab_audio_group_name(&groups, string("request"), &random, &output, NULL));
    uint32_t ignored;
    CHECK(ab_random48_next(&before, &ignored) && random.value == before.value &&
          same_value(output, number(99)));
    return true;
}

int main(int argc, char **argv) {
    if (argc == 4 && strcmp(argv[1], "--apk-resources") == 0) {
        return apk_catalog_checks(argv[2], argv[3], resources_match) ? 0 : 1;
    }
    if (argc == 4 && strcmp(argv[1], "--resources") == 0) {
        return catalog_checks(argv[2], argv[3], resources_match) ? 0 : 1;
    }
    if (argc == 4 && strcmp(argv[1], "--apk-assets") == 0) {
        return apk_catalog_checks(argv[2], argv[3], registrations_match) ? 0 : 1;
    }
    if (argc == 4 && strcmp(argv[1], "--assets") == 0) {
        return catalog_checks(argv[2], argv[3], registrations_match) ? 0 : 1;
    }
    if (argc == 4 && strcmp(argv[1], "--apk-catalog") == 0) {
        return apk_catalog_checks(argv[2], argv[3], catalog_matches) ? 0 : 1;
    }
    if (argc == 4 && strcmp(argv[1], "--catalog") == 0) {
        return catalog_checks(argv[2], argv[3], catalog_matches) ? 0 : 1;
    }
    if (argc == 1) {
        return edge_checks() ? 0 : 1;
    }
    if (argc != 2) {
        fprintf(stderr,
                "Usage: ab_audio_group_tests [NATIVE_CORPUS]\n"
                "       ab_audio_group_tests --catalog DECODED_AUDIO NATIVE_CATALOG\n"
                "       ab_audio_group_tests --apk-catalog APK NATIVE_CATALOG\n"
                "       ab_audio_group_tests --assets DECODED_AUDIO NATIVE_ASSETS\n"
                "       ab_audio_group_tests --apk-assets APK NATIVE_ASSETS\n"
                "       ab_audio_group_tests --resources DECODED_AUDIO NATIVE_PATHS\n"
                "       ab_audio_group_tests --apk-resources APK NATIVE_PATHS\n");
        return 2;
    }
    FILE *file = fopen(argv[1], "rb");
    if (!file) {
        return 1;
    }
    bool okay = native_checks(file);
    if (fclose(file) != 0) {
        okay = false;
    }
    return okay ? 0 : 1;
}
