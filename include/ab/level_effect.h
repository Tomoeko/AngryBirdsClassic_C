#ifndef AB_LEVEL_EFFECT_H
#define AB_LEVEL_EFFECT_H

#include "ab/lua_data.h"

typedef enum {
    AB_LEVEL_EFFECT_COLLISION_SOUND,
    AB_LEVEL_EFFECT_DAMAGE_SOUND,
    AB_LEVEL_EFFECT_DESTROYED_SOUND,
} AbLevelEffectSound;

/* The base LevelEffect birdRemovalVolume method returns binary32 one without
 * reading its receiver or bird. Water and other concrete policies are separate;
 * select this method only after resolving the active effect's actual class. */
float ab_level_effect_base_bird_volume(void);

/* Base LevelEffect's three sound methods return the corresponding field of the
 * supplied definition unchanged, including nil/false/non-string values. Their
 * receiver and object arguments are unused. Plain stable definition tables only;
 * missing fields return nil and non-tables fail without changing output. No
 * material fallback, random selection, name resolution, audio or allocation. */
bool ab_level_effect_base_sound(AbLevelEffectSound sound, AbDataValue definition,
                                AbDataValue *output, AbError *error);

#endif
