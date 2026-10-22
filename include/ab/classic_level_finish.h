#ifndef AB_CLASSIC_LEVEL_FINISH_H
#define AB_CLASSIC_LEVEL_FINISH_H

#include "ab/classic_sling.h"
#include "ab/level_complete.h"

typedef struct AbClassicLevelFinish AbClassicLevelFinish;

typedef struct {
    AbLevelCompleteState complete;
    float failed_timer;
} AbClassicLevelFinishState;

typedef struct {
    AbClassicShots *shots;
    AbClassicSling *sling;
    AbClassicCamera *camera;
    AbAudioBank *audio;
    AbEventManager *events;
    AbClassicLevelFinishState *state;
    const AbDataValue *clear_sound, *failed_sound;
    void *context;
    /* Resolved ordinary level-script onInitLevelComplete binding; NULL is an
     * explicitly absent hook. Later dynamic rebinding remains unsupported. */
    bool (*level_complete)(void *context, AbError *error);
    size_t notifications, storage_bytes;
} AbClassicLevelFinishConfig;

/* Bind initLevelComplete prototype104369 and initLevelFailed105431 to actual
 * sling cancellation, canonical bird metadata, getNextBird, LevelEndSequence,
 * retained EventManager payloads and shared decoded audio. Borrow all owners,
 * typed sound globals and initialized state at stable addresses until close.
 * No gameplay/RNG effects at open. State.complete's current bird/index/count
 * views refresh at their original boundaries; flags/timers stay caller-owned.
 * Bind controller's level_completed to state.complete.completed. The typed
 * clear_sound is authoritative instead of the helper's C-string group view.
 * Drain borrowing notifications before close. No allocation during transitions.
 * Fixed initial canonical objects and the bound default sling only; dynamic
 * world rebinding/classes and full game/frame/bootstrap remain separate.
 */
bool ab_classic_level_finish_open(AbClassicLevelFinishConfig config,
                                  AbClassicLevelFinish **output, AbError *error);
void ab_classic_level_finish_close(AbClassicLevelFinish *finish);
size_t ab_classic_level_finish_bytes(const AbClassicLevelFinish *finish);

/* Notify/cancel/completion/bonus-camera/audio/hook ordering follows the existing
 * native-matched helper. Init failure accumulates seconds; only strictly above
 * 1.5 and with no eagle sets ending_timer=.5/completing before direct resource
 * audio. It does not reset failed_timer. Repeated eligible calls retain the
 * original repeated sound behavior. Failure audio requires a resolved string.
 * Used arithmetic is finite. A late error retains effects and stops this owner;
 * transitions cannot reenter from listeners/hooks. Ending/bonus/results updates
 * and the original win/loss query scheduling are not supplied here.
 */
bool ab_classic_level_finish_complete(AbClassicLevelFinish *finish, AbError *error);
bool ab_classic_level_finish_failed(AbClassicLevelFinish *finish, float seconds,
                                    AbError *error);

#endif
