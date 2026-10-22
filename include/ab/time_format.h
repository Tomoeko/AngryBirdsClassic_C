#ifndef AB_TIME_FORMAT_H
#define AB_TIME_FORMAT_H

#include "ab/base.h"

/* Shared formatTime projection: float arithmetic, truncated decimal integers,
 * source last-two-character padding, and the 3600-second hours threshold.
 * Negative values preserve the original substring behavior. Finite values
 * whose component conversions fit target signed int only. No allocation. */
bool ab_time_format(float seconds, char *output, size_t capacity, AbError *error);

#endif
