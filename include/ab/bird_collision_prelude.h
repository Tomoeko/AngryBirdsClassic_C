#ifndef AB_BIRD_COLLISION_PRELUDE_H
#define AB_BIRD_COLLISION_PRELUDE_H

#include "ab/bird_collision_notify.h"

typedef struct {
    AbSpan object_name;
} AbBirdCollisionHat;

typedef struct {
    bool present;
    float score;
} AbBirdCollisionScoreEntry;

typedef struct {
    AbVec2 position;
    AbSpan text;
    float score, time, lifetime, max_scale, xs;
    /* Shared floatingScores queue also accepts destruction sprite records.
     * Bird prelude requests have text_present=true and nil sprite. */
    AbDataValue sprite;
    bool text_present;
} AbBirdCollisionFloatingScore;

typedef struct {
    AbDataValue delta_based_slingshot, bird_specialty_available;
    float floating_score_scaling;
    float *collision_scores;
    size_t count, capacity;
    /* Optional canonical availability binding shared with later bird phases.
     * When present, this replaces the local value for reads. */
    const AbDataValue *specialty_available_binding;
} AbBirdCollisionPreludeState;

typedef struct {
    void *context;
    /* Pure borrowed registry lookups. A NULL result denotes an absent entry. */
    bool (*hat)(void *context, AbSpan name, AbBirdCollisionHat **output,
                AbError *error);
    bool (*object)(void *context, AbSpan name, AbBirdCollisionObject **output,
                   AbError *error);
    bool (*score_entry)(void *context, AbSpan incoming_name,
                        AbBirdCollisionScoreEntry **output, AbError *error);
    bool (*score)(void *context, float damage, float *output, AbError *error);
    /* Text is borrowed only for this call. Later state is read after the host. */
    bool (*floating)(void *context, const AbBirdCollisionFloatingScore *request,
                     AbError *error);
    bool (*cooldown)(void *context, AbError *error);
    /* Optional achievementProcessor:collisionCheck method, receiver bound here. */
    bool (*achievement)(void *context, AbBirdCollisionObject *first,
                        AbBirdCollisionObject *second, AbError *error);
} AbBirdCollisionPreludeHost;

/* Continue a successful, non-projectile bird notification prefix through the
 * related-hat guard, first bird's shot-hat damage reset, floating damage score,
 * per-incoming-name score accumulation, collision-score append, cooldown and
 * optional achievement notification. This is instructions 156..300 of the
 * original bird callback; flying-bird/specialty/effect phases remain external.
 *
 * Both hats can suppress the rest of the callback, but only the first shot
 * object's hat has defence/strength reset. An unresolved first shot hat skips
 * that reset. Positive damage alone requests scoring. A previously absent score
 * entry starts at +0. Cooldown uses current Lua truthiness and hasCollided,
 * after score callbacks, without assigning either collision flag here.
 *
 * Borrow stable objects, state and names; hosts may mutate later fields and
 * bindings, but may not reenter or release cached objects. Used numbers must be
 * finite. C numeric locale/round-to-nearest are required for score text. No
 * allocation or Lua. Output commits only on success; earlier state/host effects
 * remain on error, including total assignment before append-capacity failure.
 */
bool ab_bird_collision_prelude(AbSpan incoming_first, AbBirdCollisionObject *first,
                               AbBirdCollisionObject *second, float damage,
                               AbBirdCollisionPreludeState *state,
                               const AbBirdCollisionPreludeHost *host,
                               bool *related_hat, AbError *error);

#endif
