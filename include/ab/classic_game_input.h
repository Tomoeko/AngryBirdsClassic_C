#ifndef AB_CLASSIC_GAME_INPUT_H
#define AB_CLASSIC_GAME_INPUT_H

#include "ab/classic_bird_specialty.h"
#include "ab/classic_sling.h"
#include "ab/classic_touches.h"
#include "ab/game_update_tail.h"
#include "ab/string_map.h"

typedef struct AbClassicMenuInput AbClassicMenuInput;

typedef struct {
    AbDataValue skip_input, skip_trigger_specialty, touch_released_after_specialty;
    AbDataValue using_gestures, delta_sling, accelerometer, disable_drag;
    AbDataValue eagle_click_time, touch_count, cheats_enabled, hats;
    AbVec2 cursor, cursor_physics;
} AbClassicGameInputState;

typedef struct {
    AbClassicShots *shots;
    AbClassicSling *sling;
    AbClassicBirdSpecialty *specialty;
    AbClassicTouches *touches;
    AbCameraSystem *camera;
    AbGameTailState *tail;
    AbClassicGameInputState *state;
    const AbDataValue *flying;
    const bool *level_completed;
    const AbDataTable *pressed, *held, *released;
    const float *screen_width;
    void *context;
    bool (*release_hat)(void *context, AbDataValue name, AbError *error);
    bool (*complete_cheat)(void *context, AbError *error);
    /* When bound, fetch the actual filtered view/copy at the reached touch
     * boundary after hat/specialty callbacks. Prepared touches are then unused.
     * The raw key tables above must be the same actual menu raw key tables. */
    AbClassicMenuInput *menu_input;
    /* Actual canonical named hat registry, shared with bird retirement and
     * collisions. NULL retains the raw state.hats table for isolated callers. */
    AbStringMap *hats;
} AbClassicGameInputConfig;

typedef struct AbClassicGameInput {
    AbClassicGameInputConfig config;
    AbClassicLevelView level;
    bool initialized, updating, failed;
} AbClassicGameInput;

typedef struct {
    const AbClassicTouch *copied, *raw;
    size_t copied_count, raw_count;
} AbClassicGameTouchInput;

/* Source-backed GameUpdate504..1148, ordinary mouse controls. Borrow actual
 * canonical flags/cursor/key tables, resolved hat map, filtered touch snapshots,
 * camera and concrete sling/specialty/behavior owners. Initialization supplies
 * no flags, menu defaults or input. Optional hat/cheat services are mandatory
 * only when reached. Active gesture/delta/motion control is not bound here.
 */
bool ab_classic_game_input_initialize(AbClassicGameInputConfig config,
                                      AbClassicGameInput *output, AbError *error);
bool ab_classic_game_input_update(AbClassicGameInput *input,
                                  AbClassicGameTouchInput touches, float seconds,
                                  AbDataValue time, AbError *error);
/* Bind to the actual tail boundaries: cursor after object-script updates,
 * skip resets only after successful draw. Neither function advances time. */
bool ab_classic_game_input_finish_cursor(AbClassicGameInput *input, AbError *error);
void ab_classic_game_input_finish_draw(AbClassicGameInput *input);

#endif
