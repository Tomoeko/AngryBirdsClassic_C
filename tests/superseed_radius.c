#include "ab/classic_superseed.h"
#include "superseed_cases.h"

#include <stdio.h>
#include <string.h>

static AbDataValue text(const char *name) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)name, strlen(name)}};
}

static AbDataValue number(float value) {
    AbDataValue result = {.kind = AB_DATA_NUMBER};
    memcpy(&result.bits, &value, sizeof(value));
    return result;
}

int main(int argc, char **argv) {
    FILE *reference = NULL;
    if (argc == 2) {
        reference = fopen(argv[1], "r");
        char header[96];
        if (!reference || !fgets(header, sizeof(header), reference) ||
            strcmp(header, "AB_ANDROID_SUPERSEED_RADIUS_V1 2048\n")) {
            fputs("Missing original Super Seeds radius corpus\n", stderr);
            if (reference)
                fclose(reference);
            return 1;
        }
    } else if (argc != 1) {
        return 2;
    }
    AbError error = {0};
    int status = 1;
    for (size_t index = 0; index < AB_SUPERSEED_CASES; ++index) {
        AbSuperseedCase input = ab_superseed_case(index);
        AbDataEntry entries[] = {
            {text("superseed_target_radius"), number(input.target)},
            {text("superseed_timer"),
             input.timer_present ? number(input.timer) : (AbDataValue){0}}};
        AbDataTable physical = {entries, 2, 2, NULL};
        AbDataEntry base = {text("radius"), number(input.base)};
        AbDataTable definition = {&base, 1, 1, NULL};
        float radius;
        if (!ab_classic_superseed_radius(&physical, &definition, &radius, &error)) {
            fprintf(stderr, "%zu: %s\n", index, error.message);
            goto close_reference;
        }
        uint32_t actual[3] = {0, entries[1].value.kind == AB_DATA_NUMBER ? 1u : 0u,
                              entries[1].value.bits};
        memcpy(actual, &radius, sizeof(radius));
        if ((!input.timer_present || input.timer >= 0.5f) &&
            (radius != input.target || actual[1] != 0)) {
            fprintf(stderr, "Super Seeds final-radius boundary failed at %zu\n", index);
            goto close_reference;
        }
        if (reference) {
            uint32_t expected[3];
            if (fscanf(reference, "%x %x %x", expected, expected + 1, expected + 2) !=
                    3 ||
                memcmp(expected, actual, sizeof(actual))) {
                fprintf(stderr,
                        "Original Super Seeds radius mismatch at %zu: %08x %08x %08x\n",
                        index, actual[0], actual[1], actual[2]);
                goto close_reference;
            }
        }
    }
    if (reference) {
        char marker[96];
        if (fgetc(reference) != '\n' || !fgets(marker, sizeof(marker), reference) ||
            strcmp(marker, "AB Super Seeds radius reference complete\n") ||
            fgetc(reference) != EOF || ferror(reference)) {
            fputs("Incomplete original Super Seeds radius corpus\n", stderr);
            goto close_reference;
        }
    }
    puts("Super Seeds: 2048 source growth/timer cases, including nil, half-second "
         "edges and bird radii.");
    status = 0;
close_reference:
    if (reference && fclose(reference))
        status = 1;
    return status;
}
