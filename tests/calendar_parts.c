#include "ab/calendar_time.h"
#include "../tools/oracle/reference_calendar.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t bits(float value) {
    uint32_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static bool edges(void) {
    AbCalendarTime first = {2000, 1, 1, 0, 0, 0};
    AbCalendarTime second = {2026, 1, 1, 0, 0, 1};
    AbCalendarDifference result = {0}, reverse = {0};
    if (!ab_calendar_time_difference_parts(&first, &second, &result, NULL) ||
        !ab_calendar_time_difference_parts(&second, &first, &reverse, NULL) ||
        result.seconds != 1 || result.minutes != 0 || result.hours != 0 ||
        reverse.days != result.days || reverse.seconds != result.seconds)
        return false;
    AbCalendarDifference before = result;
    second.seconds = NAN;
    if (ab_calendar_time_difference_parts(&first, &second, &result, NULL) ||
        memcmp(&before, &result, sizeof(result)) ||
        ab_calendar_time_difference_parts(NULL, &first, &result, NULL) ||
        ab_calendar_time_difference_parts(&first, &first, NULL, NULL))
        return false;
    return true;
}

static bool compare(FILE *file) {
    char marker[80];
    if (file && (!fgets(marker, sizeof(marker), file) ||
                 strcmp(marker, "AB_ANDROID_CALENDAR_PARTS_V1 512 UTC\n")))
        return false;
    for (unsigned index = 0; index < 512; ++index) {
        float values[12];
        reference_calendar(index, values);
        AbCalendarTime first = {values[0], values[1], values[2],
                                values[3], values[4], values[5]};
        AbCalendarTime second = {values[6], values[7],  values[8],
                                 values[9], values[10], values[11]};
        AbCalendarDifference result = {0};
        if (!ab_calendar_time_difference_parts(&first, &second, &result, NULL) ||
            result.days < 0 || result.hours < 0 || result.hours >= 24 ||
            result.minutes < 0 || result.minutes >= 60 || result.seconds < 0 ||
            result.seconds >= 60)
            return false;
        float words[] = {result.days, result.hours, result.minutes, result.seconds};
        if (!file)
            continue;
        for (size_t field = 0; field < 16; ++field) {
            uint32_t expected = 0;
            if (fscanf(file, "%" SCNx32, &expected) != 1)
                return false;
            uint32_t actual = bits(field < 12 ? values[field] : words[field - 12]);
            if (actual != expected) {
                fprintf(stderr,
                        "Calendar parts case %u field %zu: %08" PRIx32 " != %08" PRIx32
                        "\n",
                        index, field, expected, actual);
                return false;
            }
        }
    }
    if (file) {
        if (fscanf(file, " %79[^\n]", marker) != 1 ||
            strcmp(marker, "AB calendar parts reference complete"))
            return false;
        int byte;
        do {
            byte = fgetc(file);
        } while (byte == '\r' || byte == '\n');
        if (byte != EOF || ferror(file))
            return false;
    }
    return true;
}

int main(int argc, char **argv) {
    const char *zone = getenv("TZ");
    if (!zone || strcmp(zone, "UTC0") || argc > 2)
        return 2;
    FILE *file = argc == 2 ? fopen(argv[1], "rb") : NULL;
    if (argc == 2 && !file)
        return 2;
    bool okay = edges() && compare(file);
    if (file)
        okay = fclose(file) == 0 && okay;
    if (!okay)
        return 1;
    puts(
        file
            ? "Calendar parts match all 512 original UTC cases."
            : "Calendar parts retain long-interval seconds and reject invalid inputs.");
    return 0;
}
