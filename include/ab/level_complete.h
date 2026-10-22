#ifndef AB_LEVEL_COMPLETE_H
#define AB_LEVEL_COMPLETE_H

#include "ab/base.h"

typedef struct {
    const char *clear_audio_group;
    float current_bird_index;
    float birds_shot;
    float remaining_bird_index;
    float bonus_timer;
    float ending_timer;
    bool current_bird_present;
    bool current_bird_shot_literal_true;
    bool special_mode;
    bool disable_bird_score;
    bool eagle_launched;
    bool has_level_hook;
    bool completed;
    bool completing;
} AbLevelCompleteState;

typedef enum {
    AB_LEVEL_COMPLETE_NOTIFY = 1,
    AB_LEVEL_COMPLETE_CANCEL_DRAG,
    AB_LEVEL_COMPLETE_QUERY_BIRD,
    AB_LEVEL_COMPLETE_CAMERA,
    AB_LEVEL_COMPLETE_RESOLVE_AUDIO,
    AB_LEVEL_COMPLETE_PLAY_AUDIO,
    AB_LEVEL_COMPLETE_SCRIPT,
} AbLevelCompleteOperation;

typedef struct {
    AbLevelCompleteOperation operation;
    const char *text;
    float bird_index;
    bool bird_present;
} AbLevelCompleteCall;

typedef bool (*AbLevelCompleteHost)(void *context, AbLevelCompleteCall *call,
                                    AbError *error);

/* Original initLevelComplete transition. Notify then cancel drag before setting
 * completed and deriving the remaining index from current index/shot metadata.
 * Special mode or disabled bird score selects index zero. QUERY_BIRD returns
 * non-nil presence (including false); a present bird sets bonus_timer to 3.5
 * before CAMERA requests LevelEndSequence. Resolve then play the level-clear
 * sound at gain one/no loop, assign ending_timer to one (two for truthy eagle),
 * invoke the optional argument-free level hook, and finally set completing.
 *
 * The host may mutate state; later phases reread used fields. RESOLVE_AUDIO
 * returns borrowed text retained through PLAY_AUDIO; all identifiers stay
 * borrowed. Host adapters execute actual events, drag, camera, audio and scripts.
 * No allocation or interpreter. Used numeric values/results must be finite.
 * A failed host call or unsupported value leaves preceding work applied.
 */
bool ab_level_complete_begin(AbLevelCompleteState *state, AbLevelCompleteHost host,
                             void *context, AbError *error);

#endif
