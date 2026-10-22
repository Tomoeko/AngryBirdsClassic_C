#include "ab/time_format.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static bool integer_text(float value, char *output, size_t capacity, AbError *error) {
    double truncated = trunc((double)value);
    if (!isfinite(value) || truncated < INT_MIN || truncated > INT_MAX)
        return ab_fail(error, "Time format exceeds numeric integer profile");
    int size = snprintf(output, capacity, "%d", (int)truncated);
    return (size >= 0 && (size_t)size < capacity) ||
           ab_fail(error, "Time format integer exceeds buffer");
}

bool ab_time_format(float value, char *output, size_t capacity, AbError *error) {
    if (!output || !capacity)
        return ab_fail(error, "Missing time format output buffer");
    char hours[32], minutes[32], seconds[32];
    float hours_value = floorf(value / 3600);
    float minutes_value = floorf(fmodf(value, 3600) / 60);
    float seconds_value = fmodf(value, 60);
    if (!integer_text(hours_value, hours, sizeof(hours), error) ||
        !integer_text(minutes_value, minutes + 2, sizeof(minutes) - 2, error) ||
        !integer_text(seconds_value, seconds + 2, sizeof(seconds) - 2, error))
        return false;
    minutes[0] = minutes[1] = seconds[0] = seconds[1] = '0';
    size_t minute_length = strlen(minutes), second_length = strlen(seconds);
    int size = value >= 3600
                   ? snprintf(output, capacity, "%s:%s:%s", hours,
                              minutes + minute_length - 2, seconds + second_length - 2)
                   : snprintf(output, capacity, "%s:%s", minutes + 2,
                              seconds + second_length - 2);
    return (size >= 0 && (size_t)size < capacity) ||
           ab_fail(error, "Time format exceeds output buffer");
}
