#ifndef AB_FLOATING_SCORES_H
#define AB_FLOATING_SCORES_H

#include "ab/bird_collision_prelude.h"
#include "ab/block_removal_score.h"

typedef struct AbFloatingScores AbFloatingScores;

typedef struct {
    size_t records, text_bytes, storage_bytes;
} AbFloatingScoreLimits;

/* Own bounded floatingScores records, immutable copies of their initial text
 * and sprite-name strings, and the dense active list. text_bytes bounds their
 * combined bytes per record. Other sprite values remain borrowed until close.
 * Records retain their identity and string bytes after
 * removal until close; used slots are not recycled. No RNG, renderer, coordinate
 * conversion or callback is supplied. Limits bound all retained allocations.
 * Open starts empty; output starts NULL and failure releases partial storage. */
bool ab_floating_scores_open(AbFloatingScoreLimits limits, AbFloatingScores **output,
                             AbError *error);
void ab_floating_scores_close(AbFloatingScores *scores);
size_t ab_floating_scores_bytes(const AbFloatingScores *scores);
size_t ab_floating_scores_count(const AbFloatingScores *scores);
size_t ab_floating_scores_used(const AbFloatingScores *scores);

/* Direct bird-prelude floating host. Original addFloatingScore appends only
 * for score > 0; other scores, including NaN, skip before unused fields/text.
 * Copy the complete typed descriptor and its text before appending. Positive
 * records require finite numeric fields and bounded text. No allocation or
 * implicit lifetime/default values. Failure preserves the active list. */
bool ab_floating_scores_add(void *context, const AbBirdCollisionFloatingScore *score,
                            AbError *error);
/* Direct destruction-score host. Absent/nonpositive/NaN score skips before
 * unused metadata. Positive records require finite numeric raw x/y and score
 * state; retain sprite/text presence and the source's .9/.6 lifetime/scale.
 * No score accumulation, sprite lookup, drawing or coordinate conversion. */
bool ab_floating_scores_add_removal(void *context,
                                    const AbBlockRemovalFloatingScore *score,
                                    AbError *error);

/* Original updateFloatingScores, prototype 152638. Advance time, calculate its
 * quarter-second growth/plateau/shrink scale, then remove only when time is
 * strictly greater than lifetime. Removed records retain the final time/scale.
 * Negative steps and short lifetimes preserve the original branch order.
 * No allocation, drawing or callbacks. Required used math must remain finite;
 * a late failure preserves preceding record/list writes. Serialize calls and
 * stop after failure. */
bool ab_floating_scores_update(AbFloatingScores *scores, float seconds, AbError *error);

/* Borrow a current active descriptor by dense index. Its address remains valid
 * until close, including after list removal. Numeric edits affect subsequent
 * update/draw consumers. Replaced text spans must remain borrowed and stable;
 * the initially copied text is retained independently. */
AbBirdCollisionFloatingScore *ab_floating_scores_at(AbFloatingScores *scores,
                                                    size_t index);

#endif
