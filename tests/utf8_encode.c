#include "ab/utf8.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "UTF8 encoding failed at %d: %s\n", __LINE__, #condition); \
            return false;                                                              \
        }                                                                              \
    } while (0)

static bool edge_checks(void) {
    uint8_t bytes[8] = {0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5};
    size_t count = 7;
    CHECK(ab_utf8_encode(0x1fffff, bytes, sizeof(bytes), &count) && count == 4 &&
          memcmp(bytes, "\xf7\xbf\xbf\xbf", 4) == 0 && bytes[4] == 0xa5);
    CHECK(ab_utf8_encode(0xd800, bytes, sizeof(bytes), &count) && count == 3 &&
          memcmp(bytes, "\xed\xa0\x80", 3) == 0);
    CHECK(ab_utf8_encode(UINT32_MAX, bytes, sizeof(bytes), &count) && count == 1 &&
          bytes[0] == 0xff);
    CHECK(ab_utf8_encode(UINT32_C(0x80000000), bytes, sizeof(bytes), &count) &&
          count == 1 && bytes[0] == 0);
    uint8_t saved[8];
    memcpy(saved, bytes, sizeof(saved));
    CHECK(!ab_utf8_encode(0x200000, bytes, sizeof(bytes), &count) && !count &&
          memcmp(saved, bytes, sizeof(saved)) == 0);
    CHECK(!ab_utf8_encode(0x80, bytes, 1, &count) && !count &&
          memcmp(saved, bytes, sizeof(saved)) == 0);
    count = 7;
    CHECK(!ab_utf8_encode(1, NULL, 1, &count) && count == 7);
    CHECK(!ab_utf8_encode(1, NULL, 0, &count) && !count);
    CHECK(!ab_utf8_encode(1, bytes, sizeof(bytes), NULL));
    const uint32_t codes[] = {0x200000, 'A', UINT32_MAX, 0, 0x800};
    CHECK(ab_utf8_from_codes(codes, 5, bytes, sizeof(bytes), &count) && count == 6 &&
          memcmp(bytes, "A\xff\0\xe0\xa0\x80", 6) == 0);
    count = 7;
    CHECK(!ab_utf8_from_codes(codes, 5, bytes, 2, &count) && count == 7);
    CHECK(ab_utf8_from_codes(codes, 1, NULL, 0, &count) && !count);
    CHECK(ab_utf8_from_codes(NULL, 0, NULL, 0, &count) && !count);
    CHECK(!ab_utf8_from_codes(NULL, 1, bytes, sizeof(bytes), &count));
    puts("UTF8 encoding legacy signed codes, surrogates, range/capacity, invalid "
         "storage, skipped values and partial-output checks pass.");
    return true;
}

static bool words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, output + index) == 1 &&
              fgetc(file) == (index + 1 == count ? '\n' : ' '));
    }
    return true;
}

static bool string_case(FILE *file, size_t *total_bytes) {
    uint32_t count, codes[8] = {0};
    CHECK(fscanf(file, "%" SCNx32, &count) == 1 && count <= 8 &&
          fgetc(file) == (count ? ' ' : '\n'));
    if (count) {
        CHECK(words(file, codes, count));
    }
    uint32_t length, packed[8] = {0};
    CHECK(fscanf(file, "%" SCNx32, &length) == 1 && length <= 32);
    size_t packed_count = (length + 3) / 4;
    CHECK(fgetc(file) == (packed_count ? ' ' : '\n'));
    if (packed_count) {
        CHECK(words(file, packed, packed_count));
    }
    uint8_t actual[32] = {0};
    size_t written;
    CHECK(ab_utf8_from_codes(codes, count, actual, sizeof(actual), &written) &&
          written == length);
    for (size_t index = 0; index < packed_count; ++index) {
        CHECK(ab_u32le(actual + index * 4) == packed[index]);
    }
    *total_bytes += length;
    return true;
}

static bool compare_native(FILE *file) {
    char header[80];
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB_ANDROID_UTF8_ENCODE_V1 65536\n") == 0);
    size_t accepted = 0, string_bytes = 0;
    for (size_t test = 0; test < 65536; ++test) {
        uint32_t input[2], expected[4];
        CHECK(words(file, input, 2) && input[0] <= 5 && words(file, expected, 4) &&
              expected[0] <= 1 && expected[1] <= 4);
        uint8_t bytes[] = {0xa5, 0xa5, 0xa5, 0xa5, 0x5a, 0x5a, 0x5a, 0x5a};
        size_t written = UINT32_C(0x12345678);
        bool okay = ab_utf8_encode(input[1], bytes, input[0], &written);
        if ((uint32_t)okay != expected[0] || written != expected[1] ||
            ab_u32le(bytes) != expected[2] || ab_u32le(bytes + 4) != expected[3]) {
            fprintf(stderr, "UTF8 encoder differs at case %zu\n", test);
            return false;
        }
        accepted += okay;
        if (test % 16 == 0) {
            CHECK(string_case(file, &string_bytes));
        }
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB UTF8 encode reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("All 65536 native UTF8 encodes match: %zu accepted; 4096 string conversions "
           "match all %zu bytes, including rejected codes and signed inputs.\n",
           accepted, string_bytes);
    return true;
}

int main(int argc, char **argv) {
    if (!edge_checks()) {
        return 1;
    }
    if (argc == 1) {
        return 0;
    }
    if (argc != 2) {
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
