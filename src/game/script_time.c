#include "ab/script_time.h"

#include <math.h>

static bool valid(const AbCalendarTime *value) {
    return value && isfinite(value->day) && isfinite(value->hour) &&
           isfinite(value->minutes) && isfinite(value->seconds);
}

static float seconds_of_day(const AbCalendarTime *value) {
    float total = value->hour * 60;
    total = total * 60;
    total = total + value->minutes * 60;
    return total + value->seconds;
}

bool ab_script_time_difference(const AbCalendarTime *first,
                               const AbCalendarTime *second, float *output,
                               AbError *error) {
    if (!output || !valid(first) || !valid(second))
        return ab_fail(error, "Script time difference requires finite used fields");
    float prior = seconds_of_day(second);
    float current = seconds_of_day(first);
    if (first->day != second->day)
        current = current + 86400;
    float result = current - prior;
    if (!isfinite(result))
        return ab_fail(error, "Script time difference overflow");
    *output = result;
    return true;
}
