#ifndef AB_CLASSIC_GAME_MODE_H
#define AB_CLASSIC_GAME_MODE_H

#include "ab/classic_root.h"

typedef struct AbClassicGameMode AbClassicGameMode;

typedef struct {
    void *context;
    bool (*call)(void *context, const AbDataValue *arguments, size_t count,
                 AbError *error);
} AbClassicModeFunction;

typedef enum {
    AB_CLASSIC_PLAY_MODE_MUSEUM,
    AB_CLASSIC_PLAY_MODE_OTHER,
} AbClassicPlayModeKind;

typedef struct {
    AbClassicPlayModeKind kind;
    AbUiMenuManager *menus;
    /* Other actual classes require their resolved isInLevel method. Museum
     * always queries the real MenuManager/Frame owner directly. */
    void *context;
    bool (*is_in_level)(void *context, AbDataValue *result, AbError *error);
} AbClassicPlayMode;

typedef struct {
    AbClassicModeFunction **current;
    AbClassicModeFunction *update_game, *draw_game;
    AbClassicPlayMode **play_mode, **museum_mode;
    const AbDataValue *accelerometer_control;
    bool *physics_enabled;
    size_t paused_functions, storage_bytes, depth_limit;
    void *context;
    bool (*motion_cursor)(void *context, bool enabled, AbError *error);
    /* Optional observation AFTER the real native flag write, matching a native
     * setPhysicsEnabled boundary. Observation may fail but must not mutate state. */
    bool (*observe_physics)(void *context, bool enabled, AbError *error);
} AbClassicGameModeConfig;

/* Own distinct bounded draw-only function records for setGamePaused161885;
 * borrow live currentGameMode/playMode/Museum identities, raw accelerometer flag
 * and actual update/draw methods. No Lua VM or implementation defaults. Each
 * pause creates a new function identity; keep aliases valid until close.
 * Current-mode functions and play-mode instances must be prepared actual owners,
 * not foreign objects. Their storage and callback contexts outlive this owner.
 * Complete ModeSystem construction/Adventure and platform cursor remain separate.
 * No effects at open. No allocation during calls; stop after reached failure. */
bool ab_classic_game_mode_open(AbClassicGameModeConfig config,
                               AbClassicGameMode **output, AbError *error);
void ab_classic_game_mode_close(AbClassicGameMode *mode);
size_t ab_classic_game_mode_bytes(const AbClassicGameMode *mode);
size_t ab_classic_game_mode_paused_count(const AbClassicGameMode *mode);

/* Source setGameMode65192: publish first, then update the actual motion cursor
 * when its raw flag is truthy, enabling it only when function identity differs from
 * updateGame. */
bool ab_classic_game_mode_set(AbClassicGameMode *mode, AbClassicModeFunction *function,
                              AbError *error);
/* Source isIngame161794 -> current play-mode isInLevel. Preserve raw nil/Boolean
 * result. Museum uses actual source-backed menu/show query. */
bool ab_classic_game_mode_in_level(AbClassicGameMode *mode, AbDataValue *result,
                                   AbError *error);
/* Query isIngame first; if falsy, proceed only outside the Museum identity.
 * Pause: physics false BEFORE publishing a fresh draw-only mode. Resume:
 * publish updateGame BEFORE physics true. Raw numeric zero paused is truthy. */
bool ab_classic_game_mode_pause(AbClassicGameMode *mode, AbDataValue paused,
                                AbError *error);
/* Direct LevelStartScreenFlow.set_paused binding. */
bool ab_classic_game_mode_set_paused(void *context, bool paused, AbError *error);
/* Actual currentGameMode call; capture its identity then invoke with supplied
 * arguments. A paused closure calls the live drawGame method with ZERO args. */
bool ab_classic_game_mode_call(AbClassicGameMode *mode, const AbDataValue *arguments,
                               size_t count, AbError *error);
/* Direct AbClassicRootCall adapter. Result is nil; arguments pass unchanged. */
bool ab_classic_game_mode_root(void *context, const AbDataValue *arguments,
                               size_t count, AbDataValue *result, AbError *error);

#endif
