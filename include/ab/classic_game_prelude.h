#ifndef AB_CLASSIC_GAME_PRELUDE_H
#define AB_CLASSIC_GAME_PRELUDE_H

#include "ab/classic_game_effects.h"
#include "ab/classic_shots.h"
#include "ab/settings_statistics.h"

typedef struct {
    AbClassicShots *shots;
    AbClassicCamera *camera;
    AbClassicGameEffects *effects;
    AbSettingsStatistics *settings;
    const AbDataValue *flying, *cheats_enabled, *touch_count, *do_not_timeout;
    AbVec2 *cursor, *cursor_physics, *cursor_world, *old_cursor_world;
    void *context;
    bool (*notify_toy_activated)(void *context, AbError *error);
} AbClassicGamePreludeConfig;

typedef struct {
    AbClassicGamePreludeConfig config;
    bool initialized, updating, failed;
} AbClassicGamePrelude;

/* Stable borrowed binding for GameUpdate prototype 110400 instructions 0..141:
 * level time, cheat event gate, actual definition/settings tutorial lookups,
 * early effects, timeout, then old/current cursor publication. SettingsWrapper's
 * nil-only tutorials initialization uses its actual shared bounded store.
 * Required active cheat notification rejects if missing. Bootstrap must bind
 * canonical shot globals and publish the real camera viewport beforehand.
 * Initial canonical physical objects and ordinary string tutorial keys only;
 * no metatables, foreign world objects, numeric keys or replaced settings methods.
 * Output is unchanged on initialization failure; no game effects at initialize.
 */
bool ab_classic_game_prelude_initialize(AbClassicGamePreludeConfig config,
                                        AbClassicGamePrelude *output, AbError *error);

/* Run after the root wrapper advances global time at its recovered boundary.
 * time is the original updateGame argument, not reread from global time after
 * callbacks. The concrete frame binding requires finite numeric time. Earlier
 * writes/effects survive later failure; failed bindings stop.
 * No allocation, device I/O, implicit delta clamp or clock advance. Remaining
 * input/level-ending prelude and the complete native frame remain separate.
 */
bool ab_classic_game_prelude_update(AbClassicGamePrelude *prelude, float seconds,
                                    AbDataValue time, AbError *error);

#endif
