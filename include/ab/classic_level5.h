#ifndef AB_CLASSIC_LEVEL5_H
#define AB_CLASSIC_LEVEL5_H

#include "ab/lua_data.h"
#include "ab/physics_geometry.h"

typedef struct {
    size_t key_index;
} AbClassicLevel5;

typedef struct {
    const AbDataTable *world, *pressed;
    AbVec2 cursor_physics;
    bool double_click, desktop, accelerometer;
} AbClassicLevel5Input;

/* level_scripts.lua Level5 onLoadLevel3941 resets key_index to zero. Update3998
 * visits one sequence key, then applies the source mouse/key gates and strict
 * treasure-chest distance. Android uses double-click; desktop uses OPEN and an
 * accelerometer controller uses KEY_GAMING_B. Borrow canonical world/key tables.
 * The host executes goldenEggAchieved(LevelGE_15), including its unlock guard.
 * No allocation, implicit input, physics, unlock or save effects. */
bool ab_classic_level5_update(AbClassicLevel5 *state, AbClassicLevel5Input input,
                              bool (*achieve)(void *context, AbSpan name,
                                              AbError *error),
                              void *context, AbError *error);

#endif
