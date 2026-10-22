#ifndef AB_WATER_EFFECT_H
#define AB_WATER_EFFECT_H

#include "ab/lua_data.h"

typedef struct {
    float level;
    float kill_level;
    bool enabled;
    bool freezing;
    bool kill_level_present;
} AbWaterEffect;

/* Initial native WaterEffect view. A numeric top-level waterLevel takes
 * precedence over the water table and disables freezing/kill-level options.
 * Otherwise a water table enables the effect even without a numeric waterLevel;
 * missing/non-number levels convert to zero. Freezing requires literal Boolean
 * true; killLevel requires a number. Numeric strings reject because their native
 * coercion is outside this reader. Used numbers must be finite. Disabled/unused
 * numeric outputs normalize to positive zero; native getters are undefined then.
 * Failure preserves output. No buoyancy, body changes, Lua or allocation. */
bool ab_water_effect(const AbDataTable *level, AbWaterEffect *output, AbError *error);

#endif
