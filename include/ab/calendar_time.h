#ifndef AB_CALENDAR_TIME_H
#define AB_CALENDAR_TIME_H

#include "ab/base.h"

typedef struct {
    float year, month, day, hour, minutes, seconds;
} AbCalendarTime;

/* GameLua.getCurrentTime uses time/localtime and publishes six numeric fields.
 * Isolate that native C-library boundary here. Callers serialize local calendar
 * operations and configure their intended process timezone before use. Values
 * have second precision and retain the original field spellings above. */
bool ab_calendar_time_now(AbCalendarTime *output, AbError *error);
/* Original getTimeAsStruct truncates each field toward zero, offsets
 * year/month and zeroes tm_isdst. mktime normalizes overflowing date fields;
 * an unrepresentable signed32 target timestamp contributes -1, including on
 * hosts with wider time_t. Difference is double, then rounded to binary32.
 * All six inputs are finite numeric fields; string coercions/missing fields
 * and process timezone changes during a call are outside this projection.
 * UTC native comparison is recorded; other platform timezone/DST behavior
 * remains unverified. No allocation by this module; the C library owns its
 * timezone initialization/cache. Preserve output on rejected inputs. */
bool ab_calendar_time_difference(const AbCalendarTime *first,
                                 const AbCalendarTime *second, float *output,
                                 AbError *error);

typedef struct {
    float days, hours, minutes, seconds;
} AbCalendarDifference;

/* Separate native GameLua.getTimeDifference operation: absolute double timeDiff,
 * truncate to unsigned32 seconds, then split by integer division/remainder.
 * Do not round the total through the float seconds API above; long intervals
 * would lose seconds before decomposition. Same calendar/timezone domain and
 * failure policy as difference; this operation is symmetric in its arguments. */
bool ab_calendar_time_difference_parts(const AbCalendarTime *first,
                                       const AbCalendarTime *second,
                                       AbCalendarDifference *output, AbError *error);

#endif
