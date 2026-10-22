#ifndef AB_CLASSIC_COLLISION_ACHIEVEMENTS_H
#define AB_CLASSIC_COLLISION_ACHIEVEMENTS_H

#include "ab/classic_level.h"
#include "ab/event_manager.h"
#include "ab/settings_statistics.h"

typedef struct AbClassicCollisionAchievements AbClassicCollisionAchievements;

typedef struct {
    AbClassicLevel *level;
    AbSettingsStatistics *settings;
    AbEventManager *events;
    const AbDataValue *eagle_bait_launched, *special_game_mode;
    void *context;
    /* Preserve the source's two distinct class bindings. global_class is true
     * for a shot second object; false selects gamelua.SlingshotSystem. The
     * returned position uses the game's cached physics coordinates. */
    bool (*start_position)(void *context, bool global_class, AbVec2 *output,
                           AbError *error);
    size_t notifications, storage_bytes;
} AbClassicCollisionAchievementConfig;

/* Own stable event payloads for AchievementProcessor.collisionCheck (prototype
 * 1331). Bind real canonical objects, settings and EventManager. SettingsWrapper
 * setters write wilhelmTell/bullsEye without saving (prototypes 1503/1684).
 * Preserve collision-flag/setting/event order and literal-true eagle gates.
 * Earlier effects survive a later error, including a flag before notification.
 *
 * Allocate once; no frame allocation or payload reuse. Initial indices and all
 * config contexts remain borrowed until close. Drain event references before
 * close. No achievement-unlock listener, persistence, slingshot bootstrap or
 * frame scheduling is invented. Missing start-position service fails only on a
 * reached bullseye query. Serialize calls and stop the session after failure. */
bool ab_classic_collision_achievements_open(AbClassicCollisionAchievementConfig config,
                                            AbClassicCollisionAchievements **output,
                                            AbError *error);
void ab_classic_collision_achievements_close(AbClassicCollisionAchievements *owner);
size_t
ab_classic_collision_achievements_bytes(const AbClassicCollisionAchievements *owner);
bool ab_classic_collision_achievements_call(void *context, size_t first, size_t second,
                                            AbError *error);

#endif
