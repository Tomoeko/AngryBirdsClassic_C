#ifndef AB_COLLISION_DAMAGE_H
#define AB_COLLISION_DAMAGE_H

#include "ab/physics_geometry.h"
#include "ab/physics_step.h"

#include <stdint.h>

typedef enum {
    AB_MUSEUM_BLOCK_COLLISION,
    AB_MUSEUM_FIRST_BIRD_COLLISION,
    AB_MUSEUM_SECOND_BIRD_COLLISION,
    AB_MUSEUM_BIRD_PAIR_COLLISION,
} AbMuseumCollisionKind;

/* Primary MuseumCollisions dispatch uses bird/eagle definition colliders,
 * independently of engine body type and legacy velocity policy. Post-damage
 * glue handling is separate. Collider indices must be in the target's range.
 */
bool ab_museum_collision_kind(uint32_t first_collider, uint32_t second_collider,
                              AbMuseumCollisionKind *output);

typedef struct {
    AbVec2 normal;
    AbVec2 point;
    bool point_present;
} AbMuseumCallbackContact;

/* Preserve the measured enclosing callback mapping: a single-bird helper is
 * called with six arguments but forwards its final four formal parameters.
 * Thus its normal slots receive the incoming point and its point slots are nil.
 * Blocks and bird pairs retain both incoming vectors. No normal negation for
 * reversed objects. Finite vectors and a valid kind required; failure preserves
 * output. A missing point is represented by point_present=false and zero data.
 */
bool ab_museum_callback_contact(AbMuseumCollisionKind kind, AbVec2 normal, AbVec2 point,
                                AbMuseumCallbackContact *output);

typedef struct {
    AbVec2 velocity;
    float mass;
    float strength;
    float defence;
    bool has_strength;
    bool has_defence;
    bool ignore_ground_damage;
    bool ignore_all_damage;
    bool no_collision_damage;
    /* Name is ground or definition collider is static. Engine body type alone
     * does not establish this property: queued birds also have static bodies. */
    bool ground_or_static;
} AbLegacyCollisionBody;

typedef struct {
    float force;
    float remaining_strength[2];
    float callback_damage;
    bool mark_dead[2];
} AbLegacyBlockDamage;

/* Museum legacy non-bird collision calculation only. Compute the difference
 * of linear momenta, its length times binary32 0.1, then ordered strength loss
 * after defence/flag checks. callback_damage is floor of the ordered sum of
 * damage dealt. Missing strength remains absent in the caller's object state.
 * Results describe strength updates and dead-block insertions; callbacks,
 * breakable joints, score policy, destruction and nonlegacy/bird paths belong
 * to the enclosing game state. Inputs are borrowed and unchanged. Required
 * inputs, force and final strength/damage results must be finite; negative
 * mass also rejects. Failure leaves output unchanged.
 */
bool ab_legacy_block_damage(const AbLegacyCollisionBody bodies[2],
                            AbLegacyBlockDamage *output);

/* Two bird/eagle objects: choose greater squared speed (second on a tie),
 * multiply that velocity by its own mass, then take length / binary32 ten.
 * This callback-only branch does not apply bird strength or velocity changes.
 * Finite inputs, nonnegative masses and finite intermediate results required.
 * Failure preserves output.
 */
bool ab_bird_pair_force(const AbLegacyCollisionBody bodies[2], float *output);

typedef struct {
    AbVec2 velocity;
    float mass;
    float target_strength;
    float target_defence;
    float damage_multiplier;
    float velocity_multiplier;
    float velocity_modifier;
    bool has_target_strength;
    bool bird_script_present;
    bool target_script_present;
    bool target_ignores_damage;
    bool legacy_velocity;
} AbBirdDamageInput;

typedef enum {
    AB_BIRD_VELOCITY_UNCHANGED,
    AB_BIRD_VELOCITY_IMMEDIATE,
    AB_BIRD_VELOCITY_DEFERRED,
} AbBirdCollisionVelocity;

typedef struct {
    float joint_force;
    float collision_force;
    float remaining_strength;
    float callback_damage;
    AbVec2 velocity;
    AbBirdCollisionVelocity velocity_change;
    bool mark_dead;
    bool disable_contact;
} AbBirdDamage;

/* Museum bird-impact calculation. Multiplier/modifier defaults and material
 * selection are resolved by the caller. target_defence is the script getter's
 * effective value (zero when absent), unlike the non-bird object's field.
 * The joint callback receives the force before damage_multiplier. A lethal hit
 * disables contact and either requests an immediate legacy velocity change or
 * returns a deferred velocity for the enclosing native handler. The target's
 * ignore flag is consulted only when the bird has a script object.
 * This is numeric/state output only; callback routing, destruction, scoring and
 * live contact mutation are separate. Required inputs, force and final results
 * must be finite; mass is nonnegative. A missing target script with a present
 * bird script and target strength rejects, matching a target Lua error path.
 */
bool ab_bird_collision_damage(const AbBirdDamageInput *input, AbBirdDamage *output);

typedef struct {
    AbVec2 velocity;
    bool pending;
} AbDeferredVelocity;

/* basicBeginContact keeps the first proposal or a strictly slower later one.
 * Compare binary32 y*y + x*x in each vector; retain the previous proposal on a
 * tie, including overflow to equal infinities. Components must be finite; an
 * inactive previous value is not inspected. Failure preserves state.
 */
bool ab_collision_velocity_offer(AbDeferredVelocity *state, AbVec2 velocity);

/* After each fixed step's removals, assign a pending velocity with the recovered
 * body-type/wake rules and clear the flag. Static bodies still consume it.
 * No pending proposal needs no body. A rejected body/value preserves both.
 * The world/object iteration and removal order belong to the caller.
 */
bool ab_collision_velocity_flush(AbDeferredVelocity *state, AbBodyMotion *body);

#endif
