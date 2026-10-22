#ifndef AB_CLASSIC_GAME_PROGRESS_H
#define AB_CLASSIC_GAME_PROGRESS_H

#include "ab/classic_level_ending.h"

typedef struct AbClassicGameProgress AbClassicGameProgress;

typedef struct {
    AbClassicLevelFinish *finish;
    AbClassicLevelEnding *ending;
    AbClassicLevelFinishState *finish_state;
    AbClassicCharacterServices *conditions;
    AbEventManager *events;
    const AbDataValue *cheats_enabled, *key_c, *key_gaming_a, *quad_click;
    AbDataValue *level_will_end_triggered;
    size_t notifications, storage_bytes;
} AbClassicGameProgressConfig;

/* Concrete GameUpdate110400 instructions312..360. Borrow the same actual
 * finish, ending, condition and event bindings at stable addresses until close.
 * Bind live keyPressed.C/KEY_GAMING_A and the shared cheat/quad-click/trigger
 * values; bootstrap supplies their initial values. No effects at
 * open. Retained will-end payloads must outlive their borrowing listeners.
 * No replaced win/loss/finish methods or dynamic world rebinding are supported.
 */
bool ab_classic_game_progress_open(AbClassicGameProgressConfig config,
                                   AbClassicGameProgress **output, AbError *error);
void ab_classic_game_progress_close(AbClassicGameProgress *progress);
size_t ab_classic_game_progress_bytes(const AbClassicGameProgress *progress);

/* A positive ending timer runs the actual ending owner. Otherwise query win,
 * apply its cheat fallback, initialize completion when selected, then always
 * query loss. A true loss overwrites the earlier announcement decision before
 * failure initialization. Set the trigger before the retained notification.
 * Finite used timer arithmetic only; no frame allocation or implicit clocks.
 * Reentry rejects. Late errors retain prior state/effects and stop this owner.
 * Other GameUpdate phases and complete root/native frame remain separate.
 */
bool ab_classic_game_progress_update(AbClassicGameProgress *progress, float seconds,
                                     AbError *error);

#endif
