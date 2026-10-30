#include "ab/rotation.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Failed at line %d: %s\n", __LINE__, #condition);          \
            return false;                                                              \
        }                                                                              \
    } while (0)

static float from_bits(uint32_t bits) {
    float result;
    memcpy(&result, &bits, sizeof(result));
    return result;
}

static uint32_t to_bits(float value) {
    uint32_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static bool edge_checks(void) {
    const uint32_t tiny[] = {0, 0x80000000, 1, 0x80000001};
    AbRotation rotation = {0};
    for (size_t index = 0; index < sizeof(tiny) / sizeof(tiny[0]); ++index) {
        CHECK(ab_rotation_from_angle(from_bits(tiny[index]), &rotation));
        CHECK(to_bits(rotation.sine) == tiny[index] && rotation.cosine == 1);
    }
    CHECK(ab_rotation_from_angle(from_bits(0x3fc90fdb), &rotation));
    CHECK(to_bits(rotation.sine) == 0x3f800000 &&
          to_bits(rotation.cosine) == 0xb33bbd2e);
    CHECK(ab_rotation_from_angle(from_bits(0x3aed50e6), &rotation));
    CHECK(to_bits(rotation.cosine) == 0x3f7fffe5);
    const uint32_t invalid[] = {0x4dc90fdb, 0xcdc90fdb, 0x7f800000, 0xff800000,
                                0x7fc00001};
    for (size_t index = 0; index < sizeof(invalid) / sizeof(invalid[0]); ++index) {
        AbRotation original = rotation;
        CHECK(!ab_rotation_from_angle(from_bits(invalid[index]), &rotation));
        CHECK(memcmp(&rotation, &original, sizeof(rotation)) == 0);
    }
    CHECK(!ab_rotation_from_angle(0, NULL));
    return true;
}

static bool read_word(FILE *file, uint32_t *word) {
    uint8_t bytes[4];
    if (fread(bytes, sizeof(bytes), 1, file) != 1) {
        return false;
    }
    *word = (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8 | (uint32_t)bytes[2] << 16 |
            (uint32_t)bytes[3] << 24;
    return true;
}

static bool native_checks(FILE *file) {
    uint32_t magic, count = 0;
    CHECK(read_word(file, &magic) && read_word(file, &count));
    CHECK(magic == UINT32_C(0x314d4241) && (count == 4096 || count == 65536));
    for (uint32_t index = 0; index < count; ++index) {
        uint32_t words[3];
        for (size_t field = 0; field < 3; ++field) {
            CHECK(read_word(file, words + field));
        }
        AbRotation rotation;
        CHECK(ab_rotation_from_angle(from_bits(words[0]), &rotation));
        const uint32_t actual[] = {to_bits(rotation.sine), to_bits(rotation.cosine)};
        for (size_t field = 0; field < 2; ++field) {
            if (actual[field] != words[1 + field]) {
                fprintf(stderr,
                        "Android rotation case %" PRIu32 " angle %08" PRIx32
                        " field %zu: %08" PRIx32 " != %08" PRIx32 "\n",
                        index, words[0], field, actual[field], words[1 + field]);
                return false;
            }
        }
    }
    CHECK(fgetc(file) == EOF && !ferror(file));
    printf("All %" PRIu32 " rotations match Android sine/cosine bits.\n", count);
    return true;
}

int main(int argc, char **argv) {
    if (argc > 2) {
        fprintf(stderr, "Usage: ab_rotation_tests [ANDROID_MATH_CORPUS]\n");
        return 2;
    }
    if (!edge_checks()) {
        return 1;
    }
    if (argc == 1) {
        return 0;
    }
    FILE *file = fopen(argv[1], "rb");
    if (!file) {
        fprintf(stderr, "Cannot open Android reference math corpus\n");
        return 1;
    }
    bool okay = native_checks(file);
    if (fclose(file) != 0) {
        okay = false;
    }
    return okay ? 0 : 1;
}
