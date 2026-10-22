#ifndef AB_CLASSIC_LEVEL_ENDING_H
#define AB_CLASSIC_LEVEL_ENDING_H

#include "ab/classic_character_services.h"
#include "ab/classic_level_finish.h"
#include "ab/classic_scores.h"
#include "ab/floating_scores.h"

typedef struct AbClassicLevelEnding AbClassicLevelEnding;

typedef struct {
    float *entries;
    size_t count, capacity;
} AbClassicBirdBonusScores;

typedef struct {
    AbDataValue skip_ending, eagle_darkness;
    AbDataValue do_not_wait_delay, do_not_wait;
    AbDataValue cheats_enabled, quad_click, level_name, eagle_score;
    float score_increment, floating_scale, default_delay, extensions;
    bool before_script_called, delay_set;
} AbClassicLevelEndingState;

typedef struct {
    AbClassicShots *shots;
    AbClassicSling *sling;
    AbClassicScores *scores;
    AbFloatingScores *floating;
    AbAudioBank *audio;
    AbEventManager *events;
    AbClassicCharacterServices *conditions;
    AbClassicLevelFinishState *finish;
    AbClassicLevelEndingState *state;
    AbClassicBirdBonusScores *bonus;
    const AbDataValue *eagle_launched, *bird_sound;
    const AbDataTable *stars;
    void *context;
    /* Resolved level onBeforeLevelEnding binding; NULL is explicitly absent. */
    bool (*before_ending)(void *context, AbError *error);
    size_t notifications, storage_bytes;
} AbClassicLevelEndingConfig;

/* Concrete ordinary updateLevelEnding101384/beforeLevelEnding104006 bindings.
 * Borrow actual initialized state, fixed level owners, bonus append storage,
 * globals and shared audio/RNG until close. ScoreTable.birds.score comes from
 * the actual root score owner; individualScores is a distinct bounded list.
 * Conditions must borrow this state's do_not_wait and the same timeout/motion/
 * eagle globals. Bootstrap sets source defaults; open invents no values/effects.
 * Queued result payloads retain identity until close. Drain borrowing events
 * before closing or replacing source strings/state/owners. No frame allocation.
 * Fixed ordinary objects and string-key star records only; no replaced methods,
 * dynamic object creation or arbitrary score records. Output starts NULL.
 */
bool ab_classic_level_ending_open(AbClassicLevelEndingConfig config,
                                  AbClassicLevelEnding **output, AbError *error);
void ab_classic_level_ending_close(AbClassicLevelEnding *ending);
size_t ab_classic_level_ending_bytes(const AbClassicLevelEnding *ending);

/* Preserve source order: skip gate; bonus countdown/query/counter/score append/
 * actual voice/floating sprite or before-ending hook; root score; optional
 * literal-true cheat override; delay/motion gates; timeout then actual win query
 * and EID_LEVEL_ENDED queue. Completed paths do not generally subtract the
 * ending timer while bonus birds remain. Negative steps and repeated eligible
 * notifications retain original behavior. Used arithmetic must be finite.
 * Partial writes, RNG, audio, sprites and events survive failure; the owner then
 * rejects later/reentrant calls. Results UI, persistence and enclosing native
 * frame scheduling remain separate services.
 */
bool ab_classic_level_ending_update(AbClassicLevelEnding *ending, float seconds,
                                    AbError *error);

/* skipToLevelEnd103193: stop the ordinary ending path, mark completion,
 * recompute unused-bird bonuses from the live sling/current bird, update the
 * canonical score and queue the actual retained result. Existing individual
 * bonus records remain, matching the source append behavior. The GUI owns the
 * goals-cleared/gesture/eagle visibility gates. No bird sound or floating score
 * is emitted by this shortcut. Borrowed bonus storage must cover these appends;
 * failure retains its prefix and stops the owner as in update. */
bool ab_classic_level_ending_skip(AbClassicLevelEnding *ending, AbError *error);

#endif
