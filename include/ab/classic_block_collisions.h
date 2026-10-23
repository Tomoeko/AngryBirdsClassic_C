#ifndef AB_CLASSIC_BLOCK_COLLISIONS_H
#define AB_CLASSIC_BLOCK_COLLISIONS_H

#include "ab/audio_bank.h"
#include "ab/block_event.h"
#include "ab/classic_behaviors.h"
#include "ab/classic_visuals.h"
#include "ab/damage_particles.h"
#include "ab/game_contact.h"

typedef struct AbClassicBlockCollisions AbClassicBlockCollisions;

typedef struct {
    AbClassicLevel *level;
    AbClassicVisuals *visuals;
    AbClassicBehaviors *behaviors;
    AbAudioBank *audio;
    /* Actual previously applied BlockScriptManager attachments, one per initial
     * object in creation order. An absent array fails when dispatch is reached. */
    AbBlockEventObject *script_objects;
    size_t script_object_count;
    AbBlockCollisionScores *scores;
    /* Shared removal map. Inserted handles borrow this owner's stable physical
     * value rows; drain/release that map before closing this owner. */
    AbStringMap *dead_blocks;
    AbClassicJointBreakRequest break_joint;
    const float *sound_force_threshold;
    AbStringOrderWorkspace *sprite_workspace;
    void *context;
    bool (*level_hook)(void *context, size_t first, size_t second,
                       const AbBlockCollisionNotice *notice, AbDataValue *result,
                       AbError *error);
    /* Resolve the named binding on every call; an absent binding may skip. */
    bool (*named)(void *context, AbDataValue name, size_t first, size_t second,
                  const AbBlockCollisionNotice *notice, AbError *error);
    /* Resolve the active level-effect class. The retained definition includes
     * constructor material fallbacks through ab_object_property. No effect
     * class or underwater policy is selected implicitly. */
    bool (*sound)(void *context, AbBlockCollisionSound kind, size_t object,
                  const AbObjectDefinition *definition, AbDataValue *output,
                  AbError *error);
    AbAmbientParticleHost particles;
    size_t storage_bytes;
    size_t object_capacity;
} AbClassicBlockCollisionConfig;

bool ab_classic_block_collisions_register_object(AbClassicBlockCollisions *owner,
                                                 size_t object, AbError *error);

/* Bind the recovered block callback to canonical physical tables, actual
 * behavior groups, attached block scripts, damage visuals, audio bank, particle
 * boundary and shared score state. Own only bounded stable adapter rows; all
 * configuration pointers, names, attachments and service contexts remain
 * borrowed until close. Match behavior/attachment owners to this same level.
 * Open performs no script, visual, RNG, audio or world effects.
 *
 * Fixed initial registry keys and canonical physical identities only. Unknown
 * names, renamed physical names and replaced world/script tables reject when
 * reached. Nil/false gameObjectScript skips its method; other truthy receivers
 * must be the corresponding owned script. Level hooks may be absent; remaining
 * services are required only at their recovered boundary. Do not fabricate empty
 * attachments, particle emissions or successful missing services.
 *
 * Calls allocate nothing. Publish changed damage/blink/smile cache fields before
 * sound/particle services; refresh canonical values after each successful
 * external service so later phases see mutations. Cached definition identities
 * survive later physical definition rebinding. The five event slots start with
 * the actual other physical table; object methods receive the seven original
 * slots. Sprite queries use the supplied reusable disjoint workspace.
 * Stop the session after a failure; earlier effects remain. Serialize calls;
 * services may edit canonical fields and shared scores/threshold, but must not
 * release/replace storage or reenter this owner. Close before borrowed owners.
 * The direct callback does not compute contact damage; the separate contact
 * route below supplies that phase. Body destruction, frame scheduling and
 * missing level scripts/effects/particle simulation remain separate. */
bool ab_classic_block_collisions_open(AbClassicBlockCollisionConfig config,
                                      AbClassicBlockCollisions **output,
                                      AbError *error);
void ab_classic_block_collisions_close(AbClassicBlockCollisions *collisions);
size_t ab_classic_block_collisions_bytes(const AbClassicBlockCollisions *collisions);
bool ab_classic_block_collisions_call(AbClassicBlockCollisions *collisions,
                                      const AbBlockCollisionInput *input,
                                      AbError *error);

/* Ordinary Museum non-bird/non-glue damage route, suitable as the collision
 * service of AbClassicContacts when that domain has been selected. Resolve live
 * world/definition/script getters and creation-time mass/cached velocity; apply
 * ordered strength/dead-map writes, request actual deferred joint breaks, then
 * run the callback above. Update the level's canonical goal health alongside its
 * raw physical strength. No duplicate damage-score accumulation occurs here.
 * Bird/eagle/glue colliders reject before damage; the enclosing Museum dispatcher
 * must route them to their complete handlers. Missing dead/joint services fail
 * only when reached. Earlier strength/map/host effects remain on failure. No
 * allocation, physics step, deferred removal, velocity or geometry synthesis. */
bool ab_classic_block_collisions_contact(void *context, AbGameContactCall *call,
                                         AbError *error);

#endif
