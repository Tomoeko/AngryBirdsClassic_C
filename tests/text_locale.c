#include "ab/text_resources.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "TEXT locale comparison failed at %d: %s\n", __LINE__,     \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

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

static bool read_text(FILE *file, uint8_t bytes[128], AbSpan *output) {
    uint32_t length;
    CHECK(words(file, &length, 1) && length <= 128);
    for (size_t index = 0; index < length; ++index) {
        int high = hex_digit(fgetc(file)), low = hex_digit(fgetc(file));
        CHECK(high >= 0 && low >= 0);
        bytes[index] = (uint8_t)(high * 16 + low);
    }
    CHECK(fgetc(file) == '\n');
    *output = (AbSpan){bytes, length};
    return true;
}

static bool equals(AbSpan first, AbSpan second) {
    return first.size == second.size &&
           (!first.size || memcmp(first.data, second.data, first.size) == 0);
}

static bool compare_metadata(FILE *file) {
    uint32_t metadata[3], count;
    CHECK(words(file, metadata, 3) && metadata[0] == 1 &&
          metadata[1] == 17 + 4096 * 21 && metadata[2] == 0x10006 &&
          words(file, &count, 1) && count == 15);
    const uint32_t slots[] = {6,   21,  22,  23,  33,  35,  113, 115,
                              164, 167, 168, 221, 228, 230, 231};
    const uint32_t counts[] = {4097, 12291, 12291, 12291, 4096,  4096, 4097, 4097,
                               4096, 1,     4096,  4096,  16386, 1,    1};
    for (size_t index = 0; index < 15; ++index) {
        uint32_t call[2];
        CHECK(words(file, call, 2) && call[0] == slots[index] &&
              call[1] == counts[index]);
    }
    char completion[80];
    CHECK(fgets(completion, sizeof(completion), file) &&
          strcmp(completion, "AB text locale reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    return true;
}

static bool compare_native(FILE *file) {
    char header[80];
    uint32_t available_count = 0;
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB_ANDROID_TEXT_LOCALE_V1 4096\n") == 0 &&
          words(file, &available_count, 1) && available_count == 9);
    uint8_t source_bytes[32][128];
    AbSpan available[32];
    for (uint32_t index = 0; index < available_count; ++index) {
        CHECK(read_text(file, source_bytes[index], available + index));
    }
    size_t japanese = 0, empty_vectors = 0, absent_fallback = 0;
    for (size_t test = 0; test < 4096; ++test) {
        uint32_t mask;
        uint8_t preferred_bytes[128], expected_bytes[128];
        AbSpan preferred = {0}, expected = {0}, actual, selected[32];
        size_t count = 0;
        CHECK(words(file, &mask, 1) && read_text(file, preferred_bytes, &preferred) &&
              read_text(file, expected_bytes, &expected));
        for (uint32_t index = 0; index < available_count; ++index) {
            if (mask & (UINT32_C(1) << index)) {
                selected[count++] = available[index];
            }
        }
        CHECK(ab_text_locale_select(selected, count, preferred, &actual, NULL));
        if (!equals(actual, expected)) {
            fprintf(stderr, "TEXT locale result differs at case %zu\n", test);
            return false;
        }
        japanese +=
            preferred.size >= 2 && preferred.data[0] == 'j' && preferred.data[1] == 'a';
        empty_vectors += count == 0;
        bool present = false;
        for (size_t index = 0; index < count; ++index) {
            present |= equals(actual, selected[index]);
        }
        absent_fallback += !present;
    }
    CHECK(compare_metadata(file));
    printf("All 4096 Android GameLua locale choices match: %zu Japanese-prefix "
           "inputs, %zu empty available lists, %zu absent fallback selections.\n",
           japanese, empty_vectors, absent_fallback);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fputs("Usage: ab_text_locale_tests REFERENCE\n", stderr);
        return 2;
    }
    FILE *file = fopen(argv[1], "rb");
    if (!file) {
        return 1;
    }
    bool okay = compare_native(file);
    fclose(file);
    return okay ? 0 : 1;
}
