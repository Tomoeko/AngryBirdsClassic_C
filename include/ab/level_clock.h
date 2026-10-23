#ifndef AB_LEVEL_CLOCK_H
#define AB_LEVEL_CLOCK_H

#include "ab/calendar_time.h"
#include "ab/lua_data.h"

/* LevelGE_20 onLoadLevel9849, after the caller's getCurrentTime boundary.
 * Borrow fresh decoded loadedObjects and mutate existing placement x/y fields
 * before body construction. Missing/falsy clock objects skip. Reached objects
 * are distinct plain tables with finite numeric positions; time has native
 * minute/second ranges. Retain source binary32 steps and its minute-angle use
 * for the light object; the source's unused seconds angle has no effect. Errors
 * preserve prior writes. No allocation, resource lookup or physics work. */
bool ab_level_clock_prepare(const AbLuaData *level, const AbCalendarTime *time,
                            AbError *error);

#endif
