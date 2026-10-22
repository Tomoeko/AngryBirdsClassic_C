#ifndef AB_CLASSIC_REMOVALS_H
#define AB_CLASSIC_REMOVALS_H

#include "ab/block_removal_pass.h"
#include "ab/block_collision.h"
#include "ab/classic_behaviors.h"
#include "ab/classic_object_removal.h"
#include "ab/classic_particles.h"
#include "ab/audio_bank.h"
#include "ab/floating_scores.h"
#include "ab/object_removal_notify.h"

typedef struct AbClassicRemovals AbClassicRemovals;

typedef struct {
    AbClassicLevel *level;
    AbClassicBehaviors *behaviors;
    AbClassicObjectRemoval *physical;
    AbClassicParticles *particles;
    AbAudioBank *audio;
    AbFloatingScores *floating;
    /* scoreTable.blocks has one total, but distinct collisionScores and
     * individualDestructionScores arrays. The latter's total is a bridge view
     * of scores->total; it is refreshed/published at service boundaries. */
    AbBlockCollisionScores *scores;
    AbBlockRemovalScores *destruction_scores;
    AbStringMap *dead_blocks, *level_goals;
    AbRandom48 *random;
    AbDataValue *level_name;
    AbObjectRemovalNotifyHost *notifications;
    AbBlockRemovalSpawnHost *spawns;
    float block_increment, goal_increment, floating_scaling;
    size_t definition_fields, storage_bytes;
    void *context;
    bool (*destroyed_sound)(void *context, size_t object, AbDataValue definition,
                            AbDataValue *output, AbError *error);
    bool (*update_destroyed)(void *context, size_t object, AbError *error);
    bool (*remove_pig)(void *context, AbSpan name, AbError *error);
    bool (*rocket)(void *context, AbDataValue name, AbError *error);
    bool (*explosion)(void *context, AbDataValue object, AbDataValue definition,
                      AbDataValue sound, AbError *error);
    bool (*collect_gift)(void *context, AbDataValue level, AbDataValue x, AbDataValue y,
                         AbError *error);
    bool (*functions)(void *context, size_t object, AbUiPointerValue *output,
                      AbError *error);
    bool (*call)(void *context, AbUiPointerValue function, size_t object,
                 AbError *error);
} AbClassicRemovalConfig;

/* Compose the native-matched removeBlocks phases over actual canonical state.
 * Own bounded stable normalized definition tables; borrow all supplied owners,
 * maps, values and services until close. The raw immutable catalog stays separate
 * for notification/spawn lookup. Definition aliases share a retained normalized
 * table; material fallback follows ObjectCreation's supported fields.
 *
 * Dispatch real behavior destruction, breaking scores, typed audio, copied
 * floating metadata, shared RNG, new/legacy particles, physical/scene/joint
 * removal and world/dynamic/goal/dead cleanup. Active level hooks, statistics,
 * goal checks, explosions, rockets, gifts and optional removal functions require
 * their actual supplied service when reached. No assumed empty registry or
 * success substitute. Absent removal functions skip from their actual raw field.
 *
 * Open has no game/RNG effects. Calls allocate nothing; do not reenter or mutate
 * borrowed storage topology. Dead-map values must remain pinned until close.
 * Errors retain earlier effects, including a completed native physical removal.
 * This is the after-step removal phase, not an outer frame/session or parity
 * claim for complete gameplay. */
bool ab_classic_removals_open(AbClassicRemovalConfig config, AbClassicRemovals **output,
                              AbError *error);
void ab_classic_removals_close(AbClassicRemovals *owner);
size_t ab_classic_removals_bytes(const AbClassicRemovals *owner);
bool ab_classic_removals_run(AbClassicRemovals *owner, AbError *error);

/* Standalone removePig prefix for an outside-boundaries goal. If its current
 * world value is truthy, add breaking score to the same shared block total and
 * destruction list, notify removal, remove its real physical/scene object, then clear
 * world, dynamic and goals in that order. It does not run removeBlocks destruction,
 * floating, statistics or particle phases. Always run the configured goal/time/
 * old-score continuation afterward, including a missing world member. */
bool ab_classic_removals_remove_pig(void *context, AbSpan name, AbError *error);

#endif
