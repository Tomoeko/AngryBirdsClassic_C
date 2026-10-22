#ifndef AB_CLASSIC_BALLOONS_H
#define AB_CLASSIC_BALLOONS_H

#include "ab/classic_game_updates.h"
#include "ab/classic_level.h"

typedef struct AbClassicBalloons AbClassicBalloons;

typedef struct {
    AbClassicLevel *level;
    AbClassicGameUpdates *updates;
    AbStringMap *dead_blocks;
    size_t nodes, storage_bytes;
} AbClassicBalloonConfig;

/* Bind the resulting fixed initial objects.balloons registry from initBalloons
 * 162709, its creator163046 and updater163129. Initial members select actual
 * raw catalog materialName=="balloon" and insert canonical physical values in
 * original creation order, excluding separately generated ground. No definition
 * or creator overrides, dynamic slot creation/reuse or constructor interleaving.
 * This projection does not implement the complete ObjectFunctions factory.
 *
 * Register the actual removeBalloonsTooHigh callback in the shared, already
 * cleared GameUpdate registry, even for zero balloons. An existing callback of
 * that name rejects this initial binding. All source owners, names and tables
 * remain borrowed. Release/clear the update registry and dead-map borrowers
 * before closing this owner, whose stable physical value wrappers they borrow.
 * Constructor effects can include registration only after storage succeeds.
 */
bool ab_classic_balloons_open(AbClassicBalloonConfig config, AbClassicBalloons **output,
                              AbError *error);
void ab_classic_balloons_close(AbClassicBalloons *balloons);
size_t ab_classic_balloons_bytes(const AbClassicBalloons *balloons);
const AbStringMap *ab_classic_balloons_members(const AbClassicBalloons *balloons);
/* Concrete creator boundary for an existing canonical object slot/name. Caller
 * has already selected the registered balloon-definition creator. */
bool ab_classic_balloons_created(AbClassicBalloons *balloons, size_t object,
                                 AbError *error);
/* Strict nil world lookup drops stale membership. Otherwise stored y<-200
 * appends/replaces the same canonical physical value in the real dead map.
 * Destruction stays at the later physics removal boundary. Numeric NaN/Inf
 * comparisons pass through; nonnumeric y rejects. No allocation/clock/RNG;
 * late failure stops this owner and retains prior map effects. */
bool ab_classic_balloons_update(AbClassicBalloons *balloons, AbError *error);

#endif
