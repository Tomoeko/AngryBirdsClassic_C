#include "ab/calendar_time.h"

#include <limits.h>
#include <math.h>
#include <time.h>

static bool component(float value, int offset, int *output, AbError *error) {
    if (!isfinite(value))
        return ab_fail(error, "Nonfinite calendar component");
    double truncated = (double)truncf(value);
    double adjusted = truncated + offset;
    if (truncated < INT_MIN || truncated > INT_MAX || adjusted < INT_MIN ||
        adjusted > INT_MAX)
        return ab_fail(error, "Calendar component exceeds native integer range");
    *output = (int)adjusted;
    return true;
}

static bool timestamp(const AbCalendarTime *date, time_t *output, AbError *error) {
    struct tm calendar = {0};
    if (!date || !component(date->year, -1900, &calendar.tm_year, error) ||
        !component(date->month, -1, &calendar.tm_mon, error) ||
        !component(date->day, 0, &calendar.tm_mday, error) ||
        !component(date->hour, 0, &calendar.tm_hour, error) ||
        !component(date->minutes, 0, &calendar.tm_min, error) ||
        !component(date->seconds, 0, &calendar.tm_sec, error))
        return false;
    time_t value = mktime(&calendar);
    double seconds = (double)value;
    *output = seconds < INT32_MIN || seconds > INT32_MAX ? (time_t)-1 : value;
    return true;
}

bool ab_calendar_time_now(AbCalendarTime *output, AbError *error) {
    if (!output)
        return ab_fail(error, "Missing calendar output");
    time_t value = time(NULL);
    double seconds = (double)value;
    if (seconds < INT32_MIN || seconds > INT32_MAX)
        return ab_fail(error, "Wall clock exceeds target signed32 time range");
    struct tm local_value;
#if defined(_WIN32)
    struct tm *local = localtime_s(&local_value, &value) == 0 ? &local_value : NULL;
#else
    struct tm *local = localtime(&value);
    if (local) {
        local_value = *local;
        local = &local_value;
    }
#endif
    if (!local)
        return ab_fail(error, "Cannot resolve local calendar time");
    *output =
        (AbCalendarTime){(float)(local->tm_year + 1900), (float)(local->tm_mon + 1),
                         (float)local->tm_mday,          (float)local->tm_hour,
                         (float)local->tm_min,           (float)local->tm_sec};
    return true;
}

bool ab_calendar_time_difference(const AbCalendarTime *first,
                                 const AbCalendarTime *second, float *output,
                                 AbError *error) {
    if (!first || !second || !output)
        return ab_fail(error, "Missing calendar difference input/output");
    time_t a, b;
    if (!timestamp(first, &a, error) || !timestamp(second, &b, error))
        return false;
    *output = (float)difftime(a, b);
    return true;
}

bool ab_calendar_time_difference_parts(const AbCalendarTime *first,
                                       const AbCalendarTime *second,
                                       AbCalendarDifference *output, AbError *error) {
    if (!first || !second || !output)
        return ab_fail(error, "Missing calendar-parts input/output");
    time_t a, b;
    if (!timestamp(first, &a, error) || !timestamp(second, &b, error))
        return false;
    double magnitude = fabs(difftime(a, b));
    if (!isfinite(magnitude) || magnitude > UINT32_MAX)
        return ab_fail(error, "Calendar difference exceeds native unsigned32 range");
    uint32_t seconds = (uint32_t)magnitude;
    uint32_t minutes = seconds / 60;
    uint32_t hours = minutes / 60;
    *output = (AbCalendarDifference){.days = (float)(hours / 24),
                                     .hours = (float)(hours % 24),
                                     .minutes = (float)(minutes % 60),
                                     .seconds = (float)(seconds % 60)};
    return true;
}
