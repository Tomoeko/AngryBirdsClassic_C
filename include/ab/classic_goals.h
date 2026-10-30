#ifndef AB_CLASSIC_GOALS_H
#define AB_CLASSIC_GOALS_H

#include "ab/classic_level.h"
#include "ab/event_manager.h"

typedef struct AbClassicGoals AbClassicGoals;

typedef struct {
    AbClassicLevel *level;
    AbEventManager *events;
    AbDataValue *disable_check, *notification_sent;
    AbDataValue *time, *score, *last_score_time, *old_score;
    size_t notifications, storage_bytes;
    size_t object_capacity;
} AbClassicGoalConfig;

/* Own initial levelGoals membership with stable canonical physical identities.
 * Insert in object-construction order into the recovered string map. Borrow
 * level, EventManager and live globals until close; notification payloads stay
 * stable until close, including payloads retained by listeners. No event/RNG
 * effects at open and no allocation on calls. Explicit object_capacity reserves
 * later stable physical identities. Drain/release borrowing events before close. */
bool ab_classic_goals_open(AbClassicGoalConfig config, AbClassicGoals **output,
                           AbError *error);
void ab_classic_goals_close(AbClassicGoals *owner);
size_t ab_classic_goals_bytes(const AbClassicGoals *owner);
AbStringMap *ab_classic_goals_map(AbClassicGoals *owner);
/* Source levelGoals[name]=physical (or nil) after a dynamic goal factory. Keep
 * canonical value addresses stable, including removed/reinserted membership. */
bool ab_classic_goals_set(AbClassicGoals *owner, size_t object, bool present,
                          AbError *error);
/* Actual checkLevelGoalsDestroyed prototype 150347. Notification precedes its
 * sent flag; a disabled check returns false, even with an empty table. */
bool ab_classic_goals_check(AbClassicGoals *owner, bool *cleared, AbError *error);
/* removePig's continuation after removeBlocks has erased world/dynamic/goals.
 * Check goals, conditionally copy time to lastScoreTime, then always score to
 * oldScore. A still-live world member rejects: its separate score/notification/
 * physical-removal prefix has not run and must not be silently skipped. */
bool ab_classic_goals_after_removal(void *context, AbSpan name, AbError *error);

#endif
