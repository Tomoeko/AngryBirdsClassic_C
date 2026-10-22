#ifndef AB_CLASSIC_GAME_MODE_BINDINGS_H
#define AB_CLASSIC_GAME_MODE_BINDINGS_H

#include "ab/classic_game_mode.h"
#include "ab/classic_game_update.h"
#include "ab/classic_game_draw.h"

typedef struct {
    AbClassicGameUpdate *game;
    AbClassicGameDraw *draw;
    const AbClassicGameTouchInput *touches;
    const AbCameraSystemInput *camera;
    const AbClassicGameGlobals *globals;
    const uint32_t *width, *height;
    AbDrawList *draws;
} AbClassicGameModeBindingConfig;

typedef struct {
    AbClassicGameModeBindingConfig config;
    AbClassicModeFunction update, draw;
} AbClassicGameModeBindings;

/* Actual updateGame and drawGame method records for the router. Stable borrowed
 * input snapshots, screen size, globals and draw list are reread at invocation.
 * update uses its two source arguments and actual composed owner; draw accepts
 * zero arguments and reads canonical script time. No injected success, time
 * advance, draw-list reset, initialization defaults or frame allocation. */
bool ab_classic_game_mode_bindings_initialize(AbClassicGameModeBindingConfig config,
                                              AbClassicGameModeBindings *output,
                                              AbError *error);

#endif
