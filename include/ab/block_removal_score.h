#ifndef AB_BLOCK_REMOVAL_SCORE_H
#define AB_BLOCK_REMOVAL_SCORE_H

#include "ab/audio_play.h"
#include "ab/block_removal_cursor.h"
#include "ab/score.h"

typedef struct {
    AbDataValue x, y, sprite;
    AbSpan text;
    bool text_present;
    AbScoreNumber score;
    float time, lifetime, max_scale, xs;
} AbBlockRemovalFloatingScore;

typedef struct {
    AbScoreNumber total;
    float *entries;
    size_t count, capacity;
} AbBlockRemovalScores;

typedef struct {
    void *context;
    bool (*destroy)(void *context, AbDataValue cached_object, AbError *error);
    bool (*definition)(void *context, AbSpan captured_key, AbDataValue *output,
                       AbError *error);
    bool (*score)(void *context, AbDataValue cached_object, AbScoreNumber *output,
                  AbError *error);
    bool (*audio_name)(void *context, AbDataValue sound, AbDataValue *output,
                       AbError *error);
    bool (*audio)(void *context, AbAudioPlay request, AbError *error);
    /* Numeric text storage is borrowed for this call only. */
    bool (*floating)(void *context, const AbBlockRemovalFloatingScore *request,
                     AbError *error);
    AbScoreNumber floating_scaling;
    AbBlockRemovalScores *scores;
} AbBlockRemovalScoreHost;

typedef struct {
    AbDataValue definition;
    AbScoreNumber increment;
} AbBlockRemovalScored;

/* Continue selected ordinary removeBlocks instructions 43..159: required
 * gameObjectScript:onDestroy, merged definition by captured map key, breaking
 * score by cached object, optional goal damage sound, floating metadata and
 * destruction-total assignment/append. Do not repeat the cursor's eligibility
 * gates. Definition and score lookups precede later definition field indexing.
 *
 * Truthy levelGoal requests non-nil damageSound (including false) with volume
 * one, false loop and absent channel. Cache audio before resolving its name.
 * Goal score 5000 requests sprite metadata even with nil spriteScore; other
 * goal scores use text. Non-goals use any non-nil spriteScore, including false,
 * otherwise text. Forward x/y as values. Sprite metadata lasts .9 with scale
 * multiplier one; text lasts .6 with multiplier .25 + score/3000. Both use zero
 * time/xs. Total is read after the floating callback and assigned before append.
 *
 * Plain stable tables/keys, numeric-or-absent scores/scaling and original ARM
 * binary32 arithmetic, including signed zeros, infinities and NaN payloads.
 * A missing score fails at text concatenation or later addition, not at query
 * return. Callback mutations stay live; incoming key/object are captured once.
 * No allocation, Lua, rendering, body destruction or later removal effects.
 * Missing required hosts fail when reached; preceding writes/callbacks survive.
 * Output commits on success. Borrow source data and retain cached tables; do not
 * reenter or free storage. Score text requires C numeric locale/nearest rounding.
 */
bool ab_block_removal_score(const AbBlockRemovalEntry *entry,
                            AbBlockRemovalScoreHost *host, AbBlockRemovalScored *output,
                            AbError *error);

#endif
