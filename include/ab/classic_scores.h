#ifndef AB_CLASSIC_SCORES_H
#define AB_CLASSIC_SCORES_H

#include "ab/classic_level.h"
#include "ab/block_collision.h"
#include "ab/bird_collision_prelude.h"

typedef struct AbClassicScores AbClassicScores;

typedef struct {
    AbClassicLevel *level;
    AbBlockCollisionScores *blocks;
    float *birds;
    AbBirdCollisionScoreEntry *objects;
    size_t count, storage_bytes;
    size_t object_capacity;
} AbClassicScoreConfig;

typedef struct {
    AbBlockCollisionScores *blocks;
    float *birds;
    AbBirdCollisionScoreEntry *objects;
    size_t count;
} AbClassicScoreView;

/* Own scoreTable's bounded string-map membership/order over borrowed stable
 * numeric records. Initially register blocks then birds, as in Museum loadLevel
 * prototype 2711 instructions 239..259. The borrowed birds value is the unused-bird
 * bonus total; collision credits remain per named object. Per-object records must be
 * absent at open and are created at the actual positive bird-collision boundary.
 * Borrow level names, blocks and object rows until close. Native hash growth
 * uses the existing measured string map; mutation allocates nothing. Registration
 * retains entries after physical removal, so final totals retain spent birds.
 *
 * This is the closed ordinary score-table shape. Arbitrary record replacement,
 * nonnumeric score fields, dynamic object creation and reserved-name aliases
 * are unsupported and reject. Open preserves numeric records and has no game/RNG
 * effects. Output starts NULL and partial allocation is released on failure.
 */
bool ab_classic_scores_open(AbClassicScoreConfig config, AbClassicScores **output,
                            AbError *error);
void ab_classic_scores_close(AbClassicScores *owner);
size_t ab_classic_scores_bytes(const AbClassicScores *owner);
AbClassicScoreView ab_classic_scores_view(const AbClassicScores *owner);
/* Extend the canonical prefix into pre-reserved borrowed score rows. This does
 * not create a scoreTable entry; its original collision boundary still does. */
bool ab_classic_scores_register_object(AbClassicScores *owner, size_t object,
                                       AbError *error);

/* The original collision callback creates scoreTable[name]={}, then .score=0
 * before adding damage. Existing rows preserve score. Only actual initial names
 * can create entries; unknown/rebound state rejects at this boundary. */
bool ab_classic_scores_entry(AbClassicScores *owner, AbSpan name,
                             AbBirdCollisionScoreEntry **output, AbError *error);
/* updateScore prototype 136245: assign +0, then add each current record in target
 * hash-slot order with binary32 rounding. Preserve partial output on a later
 * malformed/unsupported value. Collision and individual destruction arrays are
 * separate data and are not summed again here. Finite score records only. */
bool ab_classic_scores_update(AbClassicScores *owner, float *total, AbError *error);

#endif
