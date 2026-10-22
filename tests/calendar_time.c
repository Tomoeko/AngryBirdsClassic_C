#include "ab/calendar_time.h"
#include "../tools/oracle/reference_calendar.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Calendar time failed at %d: %s\n", __LINE__, #condition); \
            return false;                                                              \
        }                                                                              \
    } while (0)

static uint32_t bits(float value) {
    uint32_t output;
    memcpy(&output, &value, sizeof(output));
    return output;
}

static bool corpus(FILE *reference) {
    if (reference) {
        char header[64];
        CHECK(fgets(header, sizeof(header), reference) &&
              !strcmp(header, "AB_ANDROID_CALENDAR_TIME_V1 512 UTC\n"));
    }
    for (unsigned index = 0; index < 512; ++index) {
        float values[12];
        reference_calendar(index, values);
        AbCalendarTime first = {values[0], values[1], values[2],
                                values[3], values[4], values[5]};
        AbCalendarTime second = {values[6], values[7],  values[8],
                                 values[9], values[10], values[11]};
        float result;
        CHECK(ab_calendar_time_difference(&first, &second, &result, NULL));
        if (!reference)
            continue;
        for (unsigned field = 0; field < 13; ++field) {
            uint32_t expected;
            CHECK(fscanf(reference, "%" SCNx32, &expected) == 1);
            uint32_t actual = bits(field == 12 ? result : values[field]);
            if (expected != actual) {
                fprintf(stderr,
                        "Calendar case %u field %u: %08" PRIx32 " != %08" PRIx32 "\n",
                        index, field, expected, actual);
                return false;
            }
        }
    }
    if (reference) {
        char marker[64];
        CHECK(fscanf(reference, " %63[^\n]", marker) == 1 &&
              !strcmp(marker, "AB calendar time reference complete"));
        int byte;
        do {
            byte = fgetc(reference);
        } while (byte == '\n' || byte == '\r');
        CHECK(byte == EOF && !ferror(reference));
    }
    return true;
}

static bool edges(void) {
    AbCalendarTime current;
    CHECK(ab_calendar_time_now(&current, NULL) && current.month >= 1 &&
          current.month <= 12 && current.day >= 1 && current.day <= 31 &&
          current.hour >= 0 && current.hour <= 23 && current.minutes >= 0 &&
          current.minutes <= 59 && current.seconds >= 0 && current.seconds <= 60);
    float result = 42;
    AbCalendarTime invalid = current;
    invalid.seconds = NAN;
    CHECK(!ab_calendar_time_difference(&invalid, &current, &result, NULL) &&
          result == 42);
    invalid = current;
    invalid.year = 0x1p31f;
    CHECK(!ab_calendar_time_difference(&invalid, &current, &result, NULL) &&
          result == 42 && !ab_calendar_time_difference(NULL, &current, &result, NULL) &&
          !ab_calendar_time_difference(&current, &current, NULL, NULL));
    return true;
}

int main(int argc, char **argv) {
    const char *zone = getenv("TZ");
    if (!zone || strcmp(zone, "UTC0")) {
        fputs("Calendar comparison requires explicit TZ=UTC0\n", stderr);
        return 2;
    }
    FILE *reference = argc == 2 ? fopen(argv[1], "rb") : NULL;
    if (argc > 2 || (argc == 2 && !reference))
        return 2;
    bool okay = edges() && corpus(reference);
    if (reference)
        okay = fclose(reference) == 0 && okay;
    if (!okay)
        return 1;
    puts(reference ? "Calendar conversion matches 512 original UTC cases"
                   : "Calendar range, local clock and rejected-input checks passed");
    return 0;
}
