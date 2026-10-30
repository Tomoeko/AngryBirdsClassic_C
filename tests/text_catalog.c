#include "ab/text.h"
#include "ab/text_resources.h"
#include "ab/zip.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "TEXT catalog comparison failed at %d: %s\n", __LINE__,    \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    FILE *file;
    uint8_t *bytes;
    size_t locales, pairs, empty, string_bytes, lookups, insertions;
} Reference;

typedef struct {
    AbSpan id;
    size_t index;
} OrderedId;

static const char *const catalogs[] = {"TEXTS_APPRATER", "TEXTS_BASIC",
                                       "TEXTS_SUGOTOKU_CUSTOMISATION", "TEXTS_UPDATE"};

static bool words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, output + index) == 1 &&
              fgetc(file) == (index + 1 == count ? '\n' : ' '));
    }
    return true;
}

static int hex_digit(int character) {
    if (character >= '0' && character <= '9') {
        return character - '0';
    }
    return character >= 'a' && character <= 'f' ? character - 'a' + 10 : -1;
}

static bool string_equal(Reference *reference, AbSpan expected) {
    uint32_t size;
    CHECK(words(reference->file, &size, 1) && size <= 1024 * 1024);
    for (size_t index = 0; index < size; ++index) {
        int high = hex_digit(fgetc(reference->file));
        int low = hex_digit(fgetc(reference->file));
        CHECK(high >= 0 && low >= 0);
        reference->bytes[index] = (uint8_t)(high * 16 + low);
    }
    CHECK(fgetc(reference->file) == '\n');
    if (size != expected.size ||
        (size && memcmp(reference->bytes, expected.data, size) != 0)) {
        fprintf(stderr, "TEXT string differs: native %" PRIu32 " bytes, C %zu bytes\n",
                size, expected.size);
        return false;
    }
    reference->string_bytes += size;
    return true;
}

static int order_ids(const void *first, const void *second) {
    const OrderedId *a = first, *b = second;
    size_t length = a->id.size < b->id.size ? a->id.size : b->id.size;
    int result = length ? memcmp(a->id.data, b->id.data, length) : 0;
    if (!result) {
        result = (a->id.size > b->id.size) - (a->id.size < b->id.size);
    }
    return result;
}

static bool compare_lookup(Reference *reference, const AbTextResources *resources,
                           AbSpan group, AbSpan identifier, AbSpan expected,
                           uint32_t before, uint32_t after) {
    AbSpan actual, fallback = {0};
    const AbSpan missing = {(const uint8_t *)"__AB520_GROUP_MISSING__", 23};
    CHECK(ab_text_resources_get(resources, group, identifier, &actual, NULL) &&
          ab_text_resources_get(resources, missing, identifier, &fallback, NULL));
    CHECK(string_equal(reference, expected) && string_equal(reference, actual) &&
          string_equal(reference, fallback));
    uint32_t metadata[5];
    CHECK(words(reference->file, metadata, 5) && metadata[0] == before &&
          metadata[1] == after && metadata[2] == after && metadata[3] == 1 &&
          metadata[4] == 1);
    ++reference->lookups;
    reference->insertions += after - before;
    return true;
}

static bool compare_queries(Reference *reference, const AbTextResources *resources,
                            AbSpan group, const AbTextCatalog *catalog) {
    static const char *const values[] = {"",
                                         "__AB520_MISSING_TEXT__",
                                         "__AB520_MISSING_TEXT__",
                                         "TEXTS_BASIC",
                                         "AB520_MISSING_\xc3\xa9",
                                         "A\0B",
                                         " ",
                                         ""};
    const size_t lengths[] = {0, 22, 22, 11, 16, 3, 1, 0};
    uint32_t count, map_size = (uint32_t)catalog->id_count;
    CHECK(words(reference->file, &count, 1) && count == 8);
    for (size_t index = 0; index < count; ++index) {
        AbSpan id = {(const uint8_t *)values[index], lengths[index]}, expected;
        CHECK(string_equal(reference, id));
        bool found = ab_text_find(catalog, resources->locale, id, &expected);
        bool repeated = false;
        for (size_t previous = 0; previous < index; ++previous) {
            repeated |= lengths[previous] == id.size &&
                        (!id.size || memcmp(values[previous], id.data, id.size) == 0);
        }
        uint32_t after = map_size + (!found && !repeated);
        CHECK(compare_lookup(reference, resources, group, id, found ? expected : id,
                             map_size, after));
        map_size = after;
    }
    return true;
}

static bool compare_catalog(Reference *reference, const AbTextCatalog *catalog,
                            OrderedId *ordered, const char *group_name) {
    uint32_t locale_count;
    CHECK(words(reference->file, &locale_count, 1) &&
          locale_count == catalog->locale_count && catalog->id_count <= 8192);
    for (size_t index = 0; index < catalog->id_count; ++index) {
        ordered[index] = (OrderedId){catalog->ids[index], index};
    }
    qsort(ordered, catalog->id_count, sizeof(*ordered), order_ids);
    for (size_t index = 1; index < catalog->id_count; ++index) {
        CHECK(order_ids(ordered + index - 1, ordered + index) < 0);
    }
    for (size_t locale = 0; locale < locale_count; ++locale) {
        AbSpan group = {(const uint8_t *)group_name, strlen(group_name)};
        AbTextResourceGroup entry = {group, catalog};
        AbTextResources resources = {&entry, 1, catalog->locales[locale]};
        uint32_t pair_count = 0;
        CHECK(string_equal(reference, catalog->locales[locale]) &&
              words(reference->file, &pair_count, 1) &&
              pair_count == catalog->id_count);
        for (size_t index = 0; index < pair_count; ++index) {
            AbSpan translation =
                catalog
                    ->translations[locale * catalog->id_count + ordered[index].index];
            if (!string_equal(reference, ordered[index].id) ||
                !string_equal(reference, translation) ||
                !compare_lookup(reference, &resources, group, ordered[index].id,
                                translation, pair_count, pair_count)) {
                fprintf(stderr, "TEXT differs at locale %zu, sorted identifier %zu\n",
                        locale, index);
                return false;
            }
            ++reference->pairs;
            reference->empty += translation.size == 0;
        }
        CHECK(compare_queries(reference, &resources, group, catalog));
        ++reference->locales;
    }
    return true;
}

static bool compare_file(Reference *reference, const AbZip *zip, size_t index) {
    char path[128], filename[80];
    int length = snprintf(path, sizeof(path), "assets/data/localization/%s.dat",
                          catalogs[index]);
    CHECK(length > 0 && (size_t)length < sizeof(path));
    length = snprintf(filename, sizeof(filename), "%s.dat", catalogs[index]);
    CHECK(length > 0 && (size_t)length < sizeof(filename) &&
          string_equal(reference, (AbSpan){(const uint8_t *)filename, (size_t)length}));
    const AbZipEntry *entry = ab_zip_find(zip, path);
    CHECK(entry);
    AbBuffer data = {0};
    AbTextCatalog catalog = {0};
    AbError error = {0};
    OrderedId *ordered = NULL;
    bool okay = false;
    if (!ab_zip_read(zip, (size_t)(entry - zip->entries), 16 * 1024 * 1024, &data,
                     &error) ||
        !ab_text_read((AbSpan){data.data, data.size}, 16 * 1024 * 1024, &catalog,
                      &error)) {
        fprintf(stderr, "Cannot decode TEXT catalog: %s\n", error.message);
        goto release_catalog;
    }
    if (!catalog.id_count || catalog.id_count > 8192 ||
        !(ordered = calloc(catalog.id_count, sizeof(*ordered)))) {
        goto release_catalog;
    }
    okay = compare_catalog(reference, &catalog, ordered, catalogs[index]);

release_catalog:
    free(ordered);
    ab_text_free(&catalog);
    ab_buffer_free(&data);
    return okay;
}

static bool compare_metadata(Reference *reference) {
    uint32_t metadata[3], count;
    CHECK(words(reference->file, metadata, 3) &&
          metadata[0] == 4 + reference->locales && metadata[1] == 17 * metadata[0] &&
          metadata[2] == 0x10006 && words(reference->file, &count, 1) && count == 10);
    const uint32_t slots[] = {6, 21, 22, 23, 113, 115, 167, 228, 230, 231};
    const uint32_t multiples[] = {1, 3, 3, 3, 1, 1, 1, 2, 1, 1};
    for (size_t index = 0; index < 10; ++index) {
        uint32_t call[2];
        CHECK(words(reference->file, call, 2) && call[0] == slots[index] &&
              call[1] == multiples[index] * metadata[0]);
    }
    char completion[80];
    CHECK(fgets(completion, sizeof(completion), reference->file) &&
          strcmp(completion, "AB text catalog reference complete\n") == 0 &&
          fgetc(reference->file) == EOF && !ferror(reference->file));
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fputs("Usage: ab_text_catalog_tests REFERENCE APK\n", stderr);
        return 2;
    }
    AbBuffer archive = {0};
    AbZip zip = {0};
    AbError error = {0};
    Reference reference = {.file = fopen(argv[1], "rb"), .bytes = malloc(1024 * 1024)};
    bool okay = false;
    if (!reference.file || !reference.bytes ||
        !ab_file_read(argv[2], 64 * 1024 * 1024, &archive, &error) ||
        !ab_zip_open((AbSpan){archive.data, archive.size}, &zip, &error)) {
        fprintf(stderr, "Cannot prepare TEXT comparison: %s\n", error.message);
        goto release_inputs;
    }
    char header[80];
    if (!fgets(header, sizeof(header), reference.file) ||
        strcmp(header, "AB_ANDROID_TEXT_CATALOG_V2 4\n") != 0) {
        goto release_inputs;
    }
    for (size_t index = 0; index < 4; ++index) {
        if (!compare_file(&reference, &zip, index)) {
            goto release_inputs;
        }
    }
    okay = compare_metadata(&reference);
    if (okay) {
        printf("All four native TEXT catalogs match: %zu locales, %zu identifier/"
               "translation pairs, %zu empty translations and %zu string bytes; "
               "%zu lookup triplets, %zu observed native missing-ID insertions.\n",
               reference.locales, reference.pairs, reference.empty,
               reference.string_bytes, reference.lookups, reference.insertions);
    }

release_inputs:
    if (reference.file) {
        fclose(reference.file);
    }
    free(reference.bytes);
    ab_zip_close(&zip);
    ab_buffer_free(&archive);
    return okay ? 0 : 1;
}
