#ifndef AB_SCRIPT_TIME_H
#define AB_SCRIPT_TIME_H

#include "ab/calendar_time.h"

/* Utils.timeDiff6319, distinct from native calendar difference: subtract
 * time-of-day seconds, adding 86400 to the first time when day fields differ.
 * Year/month are unused, even for dates many days apart. Preserve binary32
 * operation order. Finite numeric used fields/results; output unchanged on
 * failure. No calendar normalization, timezone access, clock or allocation. */
bool ab_script_time_difference(const AbCalendarTime *first,
                               const AbCalendarTime *second, float *output,
                               AbError *error);

#endif
