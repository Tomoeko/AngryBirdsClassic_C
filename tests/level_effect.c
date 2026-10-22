#include "ab/level_effect.h"
#include "level_effect_cases.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(value)                                                                   \
    do {                                                                               \
        if (!(value)) {                                                                \
            fprintf(stderr, "Base level-effect check failed at %d: %s\n", __LINE__,    \
                    #value);                                                           \
            return false;                                                              \
        }                                                                              \
    } while (0)

static AbDataValue text(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)value, strlen(value)}};
}

static AbDataValue scalar(uint32_t kind, AbDataTable *table) {
    if (!kind) {
        return (AbDataValue){0};
    }
    if (kind <= 2) {
        return (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = kind == 2};
    }
    if (kind <= 5) {
        return (AbDataValue){.kind = AB_DATA_NUMBER,
                             .bits = kind == 3   ? 0u
                                     : kind == 4 ? UINT32_C(0x80000000)
                                                 : UINT32_C(0x7fc12345)};
    }
    return kind == 6 ? text("sound")
                     : (AbDataValue){.kind = AB_DATA_TABLE, .table = table};
}

static bool words(FILE *file, const uint32_t *actual, size_t count, uint32_t sample) {
    for (size_t index = 0; index < count; ++index) {
        uint32_t expected;
        CHECK(fscanf(file, "%" SCNx32, &expected) == 1);
        if (actual[index] != expected) {
            fprintf(stderr,
                    "Base level-effect case %u word %zu: %08" PRIx32 " != %08" PRIx32
                    "\n",
                    sample, index, actual[index], expected);
            return false;
        }
    }
    return true;
}

static bool reference(FILE *file) {
    char marker[80];
    CHECK(fgets(marker, sizeof(marker), file) &&
          !strcmp(marker, "AB_ANDROID_LEVEL_EFFECT_BASE_V1 64\n"));
    size_t failures = 0;
    for (uint32_t sample = 0; sample < 64; ++sample) {
        uint32_t input[5];
        level_effect_case(sample, input);
        CHECK(words(file, input, 5, sample));
        AbDataTable empty = {0};
        const char *const fields[] = {"collisionSound", "damageSound",
                                      "destroyedSound"};
        AbDataEntry entries[3];
        for (size_t index = 0; index < 3; ++index) {
            entries[index] = (AbDataEntry){
                text(fields[index]), scalar((input[2] + (uint32_t)index) % 8u, &empty)};
        }
        AbDataTable populated = {entries, 3, 3, NULL};
        AbDataValue definition =
            input[2] < 8 ? (AbDataValue){.kind = AB_DATA_TABLE, .table = &populated}
                         : scalar(input[2] - 8u, &empty);
        AbDataValue result = {0};
        bool okay = true;
        if (!input[1]) {
            float volume = ab_level_effect_base_bird_volume();
            result.kind = AB_DATA_NUMBER;
            memcpy(&result.bits, &volume, sizeof(volume));
        } else {
            okay = ab_level_effect_base_sound((AbLevelEffectSound)(input[1] - 1u),
                                              definition, &result, NULL);
        }
        uint32_t output[4] = {!okay, okay, 0, 0};
        if (okay) {
            output[2] = result.kind == AB_DATA_TABLE    ? 5u
                        : result.kind == AB_DATA_STRING ? 4u
                        : result.kind == AB_DATA_NUMBER ? 3u
                                                        : (uint32_t)result.kind;
            output[3] = result.kind == AB_DATA_TABLE    ? 2u
                        : result.kind == AB_DATA_STRING ? 1u
                                                        : result.bits;
        }
        CHECK(words(file, output, 4, sample));
        failures += !okay;
    }
    CHECK(fscanf(file, " %79[^\n]", marker) == 1 &&
          !strcmp(marker, "AB level effect base reference complete"));
    CHECK(fgetc(file) == '\n' && fgetc(file) == EOF && !ferror(file));
    printf("All 64 original base level-effect calls match: 576 words, %zu original "
           "indexing failures.\n",
           failures);
    return true;
}

static bool controls(void) {
    AbDataValue untouched = text("sentinel"), output = untouched;
    CHECK(!ab_level_effect_base_sound(AB_LEVEL_EFFECT_DESTROYED_SOUND, (AbDataValue){0},
                                      &output, NULL));
    CHECK(output.kind == untouched.kind && output.string.data == untouched.string.data);
    AbDataTable empty = {0};
    CHECK(!ab_level_effect_base_sound(
        (AbLevelEffectSound)3, (AbDataValue){.kind = AB_DATA_TABLE, .table = &empty},
        &output, NULL));
    CHECK(output.kind == untouched.kind && output.string.data == untouched.string.data);
    return true;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        return controls() ? 0 : 1;
    }
    if (argc != 2) {
        return 2;
    }
    FILE *file = fopen(argv[1], "rb");
    if (!file) {
        return 1;
    }
    bool okay = reference(file);
    if (fclose(file)) {
        okay = false;
    }
    return okay ? 0 : 1;
}
