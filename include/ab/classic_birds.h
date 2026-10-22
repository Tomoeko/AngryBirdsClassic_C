#ifndef AB_CLASSIC_BIRDS_H
#define AB_CLASSIC_BIRDS_H

#include "ab/classic_shots.h"
#include "ab/classic_object_removal.h"
#include "ab/classic_particles.h"
#include "ab/bird_collision_prelude.h"
#include "ab/bird_retirement.h"
#include "ab/bird_remove.h"
#include "ab/audio_bank.h"

typedef struct AbClassicBirds AbClassicBirds;

typedef enum {
    AB_CLASSIC_BIRD_BOMB_TIMER,
    AB_CLASSIC_BIRD_PINK_TIMER,
    AB_CLASSIC_BIRD_SUPERSEED,
} AbClassicBirdUpdate;

typedef struct {
    AbClassicLevel *level;
    AbClassicShots *shots;
    AbClassicBehaviors *behaviors;
    AbClassicCamera *camera;
    AbClassicObjectRemoval *physical;
    AbClassicParticles *particles;
    AbAudioBank *audio;
    AbStringMap *hats;
    AbDataValue *flying_object, *specialty_available, *level_effect;
    const float *remove_speed_squared, *remove_time;
    void *context;
    bool (*removed_hook)(void *context, AbDataValue name, size_t object,
                         AbError *error);
    bool (*removal_volume)(void *context, AbDataValue receiver, size_t object,
                           AbDataValue *output, AbError *error);
    bool (*specialty_update)(void *context, AbClassicBirdUpdate kind, size_t object,
                             float seconds, bool *alive, AbError *error);
    size_t storage_bytes;
    /* Zero retains the initial count/launch-table bounds. Other table entry
     * capacity includes retained nil string keys as well as numeric names. */
    size_t object_capacity, other_generations, other_entries;
} AbClassicBirdConfig;

typedef struct {
    AbDataTable *birds, *other_birds;
    /* Stable canonical typed slots for source child insertion and live Blues
     * lookups. Borrow until close; other_birds_field changes on replacement. */
    const AbDataValue *birds_field, *other_birds_field;
} AbClassicBirdView;

/* Own birds/otherBirds table projections and bounded hat views.
 * Borrow canonical level/shot/behavior/camera/particle/audio/physical owners,
 * actual hat records and shared globals. The initial otherBirds constructor is
 * empty. Defaults reserve one fresh table per initial bird launch plus that
 * constructor. Explicit capacities support later identities and source split
 * table replacements; retained old aliases remain valid until close. Bind the
 * corresponding shots owner at open and unbind at close.
 * Exposed tables remain stable until close.
 *
 * Actual hats project only the field read by removeBird, preserving the shared
 * named physical hat. Camera's field projection binds the supplied real C camera
 * method; it does not invent a camera service. Open fires no effects/RNG.
 * All calls are serialized and allocate nothing. Retain dead tables and borrowed
 * strings until close, and close before source owners. Failure retains prior
 * physical/script/audio/particle effects; stop the session after failure.
 * Supported shared current/flying identities are owned names/canonical tables
 * or nil. Used timers and numeric globals must be finite numbers.
 */
bool ab_classic_birds_open(AbClassicBirdConfig config, AbClassicBirds **output,
                           AbError *error);
void ab_classic_birds_close(AbClassicBirds *owner);
size_t ab_classic_birds_bytes(const AbClassicBirds *owner);
AbClassicBirdView ab_classic_birds_view(AbClassicBirds *owner);

/* Register the next canonical level identity after its shot/camera and native
 * physical-map bindings. This extends receiver/name resolution only; the source
 * child owns birds[name] insertion, script construction and raw fields. Never
 * add a launch queue entry, rerun ObjectCreation or seed character timers here.
 * No allocation, callbacks, field publication or RNG. Reject gaps/duplicates,
 * removed identities and exhausted explicit object capacity. */
bool ab_classic_birds_register_object(AbClassicBirds *owner, size_t object,
                                      AbError *error);

/* Refresh shot/camera views of canonical flying/specialty fields between source
 * phases, e.g. after the outer split assigns flyingBird and before camera init.
 * No global field writes, gameplay callbacks or RNG; reached binding errors
 * retain preceding cache effects. Do not call from an active bird operation. */
bool ab_classic_birds_sync(AbClassicBirds *owner, AbError *error);

/* Source otherBirds={name1,...}: publish the next fresh dense numeric table.
 * Inputs are string names; their bytes remain borrowed until close, including
 * through retained old table aliases. Entry bounds must also accommodate later
 * removeBird string-key nil writes. No lookup, effects, allocation or RNG.
 * Invalid input/exhaustion preserves the current table and unused generation. */
bool ab_classic_birds_replace_other(AbClassicBirds *owner, const AbDataValue *names,
                                    size_t count, AbError *error);

/* Fresh otherBirds={} at the source launch boundary, after the optional
 * all-birds notification and before flying sprite/particle/audio effects.
 * It is normally invoked by the bound owned shot publisher. No game callback,
 * RNG or allocation. A bounded generation exhaustion fails without rebinding. */
bool ab_classic_birds_reset_other(AbClassicBirds *owner, AbError *error);

/* Bind complete source removeBird to actual services, including script onDestroy,
 * optional definition hook, hat writes, real particles/camera/audio/physical
 * removal, shared current/flying flags and world/dynamic/bird table cleanup.
 * No breaking score or destruction counters are added. */
bool ab_classic_birds_remove(AbClassicBirds *owner, size_t object,
                             AbDataValue play_sound, AbError *error);

/* Ordinary Museum updateBird after its timed-specialty gates. Resolve current
 * script velocity/definition/timers, publish timer changes before the camera
 * query, then remove through the complete owned path. Return the surviving
 * object gate for the subsequent boundary/effect phases. Active bomb/pink/
 * superseed paths require their real supplied continuation when reached.
 * This is the object controller phase; it does not schedule the root frame. */
bool ab_classic_birds_update(AbClassicBirds *owner, size_t object, float seconds,
                             bool *alive, AbError *error);

#endif
