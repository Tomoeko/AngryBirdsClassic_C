#ifndef AB_CLASSIC_GAME_GLOBALS_H
#define AB_CLASSIC_GAME_GLOBALS_H

#include "ab/bird_shot.h"
#include "ab/lua_data.h"

/* Canonical shared globals, borrowed at stable addresses by goals, shots and
 * the root/controller. Bootstrap supplies their actual initial typed values;
 * zero initialization represents nil, not an invented numeric default. */
typedef struct {
    AbDataValue time, score, last_score_time, old_score;
    AbDataValue last_score_timer, level_timeout, time_spent;
    AbDataValue extra_tutorial_timer, specialty_available;
    AbDataValue bird_specialty;
    AbDataValue has_awake_objects, has_moving_objects,
        has_moving_objects_above_tolerance;
} AbClassicGameGlobals;

/* gamelogic.lua root update prototype 57000 instructions 289..297. Advance the
 * script clock only at this boundary, after physics/removal, menu/process and
 * AnimationWrapperNative updates. Nil/false initializes +0 without consuming
 * seconds; numeric zero is truthy and adds seconds. This clock is separate from
 * GameLua's native frame clock and updateGame's gameTimer. No full wrapper is
 * implied. Used arithmetic must be finite; prior contents survive failure. */
bool ab_classic_game_globals_advance_time(AbClassicGameGlobals *globals, float seconds,
                                          AbError *error);

/* GameUpdate prototype 110400 instructions 0..5, then (at a separate call after
 * tutorial/effects) 80..113. Preserve each assignment before a later unsupported
 * value; timeout always resets false first. The time argument is the function's
 * original value, which may differ from a subsequently changed global clock.
 * Nil/false and numeric state are supported; arithmetic requires finite numbers.
 * No implicit lastScoreTime reset or clock advance. */
bool ab_classic_game_globals_level_time(AbClassicGameGlobals *globals, float seconds,
                                        AbError *error);
bool ab_classic_game_globals_timeout(AbClassicGameGlobals *globals,
                                     AbDataValue do_not_timeout, float seconds,
                                     AbDataValue time, AbError *error);

/* The shot's float fields are a view of the canonical numeric globals above.
 * Refresh at entry and original service boundaries. Publish only the two writes
 * made immediately before EID_ALL_BIRDS_SHOT. Tutorial reset and specialty
 * assignment publish through the shot flags boundary. Copies use memcpy rather than
 * type-punning AbDataValue bits. Refresh failure preserves the view. */
bool ab_classic_game_globals_refresh_shot(const AbClassicGameGlobals *globals,
                                          AbShotState *shot, AbError *error);
bool ab_classic_game_globals_publish_shot(AbClassicGameGlobals *globals,
                                          const AbShotState *shot, AbError *error);
bool ab_classic_game_globals_publish_score(AbClassicGameGlobals *globals, float score,
                                           AbError *error);

#endif
