#ifndef AB_MUSEUM_IMPACT_H
#define AB_MUSEUM_IMPACT_H

#include "ab/collision_damage.h"
#include "ab/object_body.h"
#include "ab/object_definition.h"

typedef struct {
    AbObjectBody physics;
    AbLegacyCollisionBody damage;
    AbObjectDefinition definition;
    AbDeferredVelocity deferred;
    float velocity_modifier;
    bool script_present;
    bool collided;
    bool dead;
} AbMuseumImpactObject;

typedef struct {
    size_t bird, target;
    float force, damage;
    AbMuseumCallbackContact contact;
} AbMuseumImpactBirdCall;

typedef struct {
    AbMuseumImpactObject *objects;
    size_t count;
    const AbDataTable *catalog;
    float damage_score;
    void *context;
    bool (*bird_collision)(void *context, const AbMuseumImpactBirdCall *call,
                           AbError *error);
} AbMuseumImpact;

/* Numeric/world projection of ordinary MuseumCollisions begin-contact handling.
 * The caller resolves fixture userdata to the two object indices in contact
 * order. Logical velocities/masses and script getter flags are caller-owned;
 * they need not equal the body's current values. Update logical velocities at
 * the measured end-of-frame boundary, before held-bird motion resets the body.
 * Definition views/catalog and all object storage are borrowed and stable.
 *
 * Apply strength/dead flags, immediate/deferred bird velocities, contact disable,
 * bird hasCollided, and the ordered damage-score addition. Existing pending
 * flags are retained for the caller's post-step removal pass. This is not the
 * complete collision callback: joints, definition/level/script hooks, hats,
 * damage sprites, audio, particles, achievements and special bird pairs remain
 * separate. An optional bird_collision host receives the resolved generic bird
 * callback after damage/contact/velocity changes and before generic hasCollided
 * and damage-score publication. Its vectors preserve the single-bird helper's
 * point-to-normal/nil-point forwarding. This host supports ordinary nonempty
 * contact manifolds with both script tables present; unsupported bindings reject
 * when reached. Earlier numeric effects remain on host failure. It must not
 * reenter or change physical membership/storage. Full level/definition/script
 * and presentation effects remain host work. Without the host this remains a
 * numeric projection. It rejects bird pairs, invalid
 * indices, nonmatching body order and unsupported data/numeric values.
 *
 * This may run from a locked world's begin listener. Numeric/callback-boundary
 * failure can follow earlier state changes; the caller must stop its replay.
 * No allocation or body destruction occurs. Removal order and break scoring
 * belong to the enclosing controller. Source evidence and the 24-frame ordinary
 * first-level comparison are recorded in docs/evidence.md.
 */
bool ab_museum_impact_contact(AbMuseumImpact *state, AbPhysicsWorld *world,
                              size_t contact, size_t first, size_t second,
                              AbError *error);

#endif
