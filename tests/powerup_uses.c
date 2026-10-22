#include "ab/powerup_uses.h"
#include "powerup_uses_cases.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Powerup use check failed at %d: %s\n", __LINE__,          \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static bool storage_checks(void) {
    AbPowerupUses *uses = NULL;
    CHECK(ab_powerup_uses_open(1, 4, 256, &uses, NULL));
    bool found = true;
    CHECK(!ab_powerup_uses_contains(uses, "abcde", &found, NULL) && found &&
          !ab_powerup_uses_mark(uses, "\xc3\xa9", NULL) &&
          !ab_powerup_uses_count(uses) && ab_powerup_uses_mark(uses, "ABcd", NULL) &&
          ab_powerup_uses_contains(uses, "abcd", &found, NULL) && found &&
          !ab_powerup_uses_mark(uses, "", NULL) && ab_powerup_uses_count(uses) == 1);
    size_t bytes = ab_powerup_uses_bytes(uses);
    CHECK(ab_powerup_uses_clear(uses) && !ab_powerup_uses_count(uses));
    ab_powerup_uses_close(uses);
    CHECK(!ab_powerup_uses_open(1, 4, bytes - 1, &uses, NULL) && !uses &&
          ab_powerup_uses_open(1, 4, bytes, &uses, NULL));
    ab_powerup_uses_close(uses);
    return true;
}

static bool compare(FILE *file) {
    char marker[80];
    CHECK(fgets(marker, sizeof(marker), file) &&
          !strcmp(marker, "AB_ANDROID_POWERUP_USES_V1 8 16\n"));
    for (uint32_t sample = 0; sample < POWERUP_USE_CASES; ++sample) {
        uint32_t expected_sample;
        CHECK(fscanf(file, "%8" SCNx32, &expected_sample) == 1 &&
              sample == expected_sample);
        AbPowerupUses *uses = NULL;
        CHECK(ab_powerup_uses_open(32, 64, 4096, &uses, NULL));
        for (uint32_t initial = 0; initial < 3; ++initial)
            if (sample & (1u << initial))
                CHECK(ab_powerup_uses_mark(uses, powerup_use_initial_names[initial],
                                           NULL));
        for (uint32_t step = 0; step < POWERUP_USE_ACTIONS; ++step) {
            PowerupUseAction action = powerup_use_actions[step];
            bool result = false;
            if (action.operation == POWERUP_USE_CLEAR)
                CHECK(ab_powerup_uses_clear(uses));
            else if (action.operation == POWERUP_USE_MARK)
                CHECK(ab_powerup_uses_mark(uses, action.name, NULL));
            else
                CHECK(ab_powerup_uses_contains(uses, action.name, &result, NULL));
            uint32_t actual[5] = {result, (uint32_t)ab_powerup_uses_count(uses), 0, 0,
                                  0};
            for (size_t query = 0; query < 3; ++query) {
                CHECK(ab_powerup_uses_contains(uses, powerup_use_queries[query],
                                               &result, NULL));
                actual[query + 2] = result;
            }
            for (size_t word = 0; word < 5; ++word) {
                uint32_t expected;
                CHECK(fscanf(file, "%8" SCNx32, &expected) == 1);
                CHECK(actual[word] == expected);
            }
        }
        ab_powerup_uses_close(uses);
    }
    CHECK(fgetc(file) == '\n' && fgets(marker, sizeof(marker), file) &&
          !strcmp(marker, "AB powerup uses reference complete\n") &&
          fgetc(file) == EOF && !ferror(file));
    puts("Powerup used-list storage matches 128 original actions and 384 subsequent "
         "queries; duplicate/case/empty/reset and bounded C storage checks pass.");
    return true;
}

int main(int argc, char **argv) {
    if (!storage_checks())
        return 1;
    if (argc == 1)
        return 0;
    if (argc != 2)
        return 2;
    FILE *file = fopen(argv[1], "rb");
    if (!file)
        return 1;
    bool okay = compare(file);
    if (fclose(file))
        okay = false;
    return okay ? 0 : 1;
}
