#include "ab/burn_manager.h"
#include "burn_manager_cases.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static float number(uint32_t word) {
    float value;
    memcpy(&value, &word, sizeof(value));
    return value;
}

static uint32_t bits(float value) {
    uint32_t word;
    memcpy(&word, &value, sizeof(word));
    return word;
}

static bool check(FILE *native) {
    AbBurnManager state = {0};
    if (native) {
        char header[96];
        if (!fgets(header, sizeof(header), native) ||
            strcmp(header, "AB_ANDROID_BURN_MANAGER_V1 24\n"))
            return false;
    }
    for (size_t index = 0; index < AB_BURN_MANAGER_CASES; ++index) {
        const uint32_t *input = burn_manager_cases[index];
        if (native) {
            uint32_t words[3];
            if (fscanf(native, "%" SCNx32 " %" SCNx32 " %" SCNx32, words, words + 1,
                       words + 2) != 3 ||
                memcmp(words, input, sizeof(words)))
                return false;
        }
        bool okay;
        if (input[0] == 0)
            okay = ab_burn_manager_add(&state, number(input[1]), NULL);
        else if (input[0] == 1)
            okay = ab_burn_manager_update(&state, number(input[2]), number(input[1]),
                                          NULL);
        else if (input[0] == 2)
            okay = ab_burn_manager_reset(&state, NULL);
        else
            okay = true;
        bool burning = true;
        if (!okay || !ab_burn_manager_is_burning(NULL, &burning, NULL) || burning)
            return false;
        uint32_t output[] = {(uint32_t)state.pending |
                                 ((uint32_t)state.deadline_present << 1),
                             state.deadline_present ? bits(state.deadline) : 0,
                             bits(state.pending_seconds), (uint32_t)burning};
        if (native) {
            uint32_t expected[4];
            if (fscanf(native, "%" SCNx32 " %" SCNx32 " %" SCNx32 " %" SCNx32, expected,
                       expected + 1, expected + 2, expected + 3) != 4 ||
                memcmp(output, expected, sizeof(output))) {
                fprintf(stderr, "Burn manager diverged at operation %zu\n", index);
                return false;
            }
        }
    }
    if (state.pending || state.deadline_present || state.pending_seconds != 0)
        return false;
    AbError error = {0};
    if (ab_burn_manager_add(&state, NAN, &error) || state.pending ||
        !ab_burn_manager_add(&state, 1, NULL) ||
        ab_burn_manager_update(&state, 0, NAN, NULL) || !state.pending ||
        state.pending_seconds != 1)
        return false;
    return true;
}

int main(int argc, char **argv) {
    if (argc > 2)
        return 2;
    FILE *native = argc == 2 ? fopen(argv[1], "rb") : NULL;
    if (argc == 2 && !native)
        return 2;
    bool okay = check(native);
    if (native && fclose(native))
        okay = false;
    if (okay)
        puts("Burn manager preserves duration/deadline state and the original false "
             "query");
    return okay ? 0 : 1;
}
