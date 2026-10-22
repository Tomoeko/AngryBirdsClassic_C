#ifndef AB_CLASSIC_GAME_START_H
#define AB_CLASSIC_GAME_START_H

#include "ab/classic_game_updates.h"
#include "ab/classic_sling.h"

typedef struct {
    AbDataValue right_sweep, left_sweep;
    float quad_click_timer, level_start_timer;
} AbClassicGameStartState;

typedef enum {
    AB_CLASSIC_GAME_START_PREVIOUS_LEVEL,
    AB_CLASSIC_GAME_START_NEXT_LEVEL,
    AB_CLASSIC_GAME_START_ANGER_KEY,
    AB_CLASSIC_GAME_START_EDITOR_RESTART,
} AbClassicGameStartDebug;

typedef struct {
    AbClassicSling *sling;
    AbAudioBank *audio;
    AbClassicGameUpdates *updates;
    AbClassicGameStartState *state;
    const AbDataValue *release_build, *cheats_enabled, *started_from_editor;
    const AbDataValue *level_start_sound, *main_menu_song, *current_music;
    const AbDataTable *keys;
    bool *physics_enabled;
    void *context;
    /* Active debug/editor branches require their actual selected operation;
     * ordinary release input reaches none of these services. */
    bool (*debug)(void *context, AbClassicGameStartDebug operation, AbError *error);
    /* Resolved current level onUpdate; NULL is explicitly absent. */
    bool (*level_update)(void *context, float seconds, float time, AbError *error);
} AbClassicGameStartConfig;

typedef struct {
    AbClassicGameStartConfig config;
    bool initialized, updating, failed;
} AbClassicGameStart;

/* GameUpdate110400 instructions142..311: reset sweep flags, quad timer, debug
 * input gates, actual sling-disabled query, startup audio/queue, ambient music,
 * real function registry, resolved level hook and physics-enable threshold.
 * Borrow stable canonical state/keys/owners; bootstrap supplies source values,
 * registers actual callbacks and establishes the private update table via clear.
 * The physics flag must be the same value read by simulation/controller.
 * Initial fixed world, string audio names and plain stable key table only.
 * No effects at initialize. Output is unchanged on initialization failure.
 */
bool ab_classic_game_start_initialize(AbClassicGameStartConfig config,
                                      AbClassicGameStart *output, AbError *error);
/* Run after the concrete prelude, before the progress/input/tail phases.
 * Native setPhysicsEnabled is a direct flag write (GameLua ELF0x306060).
 * Preserve direct resource startup audio versus script ambient playback.
 * Finite used arithmetic; no frame allocation/device I/O/implicit time advance.
 * Earlier writes/audio/callbacks survive late failure; failed bindings stop.
 * Full startup/bootstrap and native complete-frame comparison remain separate.
 */
bool ab_classic_game_start_update(AbClassicGameStart *start, float seconds, float time,
                                  AbError *error);

#endif
