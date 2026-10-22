#ifndef AB_CLASSIC_LAUNCH_HOST_H
#define AB_CLASSIC_LAUNCH_HOST_H

#include "ab/classic_shot_effects.h"
#include "ab/classic_sling.h"

typedef struct {
    AbClassicLevel *level;
    AbClassicShots **shots;
    AbClassicSling **sling;
    AbAudioBank *audio;
    AbCameraSystem *const *camera;
    const AbDataValue *next_bird_sound;
    const AbPowerupUses *powerups;
    const AbDataValue *powerups_enabled, *red_special_available;
    void *context;
    /* Actual named level/catalog hooks outside the implemented redBirdShot
     * route. Mandatory only when the corresponding source call reaches. */
    AbShotHost level_hook, definition_hook;
} AbClassicLaunchHostConfig;

typedef struct {
    AbClassicLaunchHostConfig config;
    bool initialized, dispatching, servicing, failed;
} AbClassicLaunchHost;

/* Concrete ordinary shot/audio/effect binding for production construction.
 * Borrow stable shot/sling OWNER SLOTS to resolve their construction cycle;
 * initialize before opening generic AbShotServices, then create shots and sling
 * at those addresses. Resources and raw flags remain borrowed/live. No state
 * defaults, constructors, effects, allocation or successful substitute callbacks.
 * An unbound reached owner/hook fails and stops this host.
 *
 * Bind remaining to AbShotServicesConfig.remaining_shot with context=this.
 * Statistics/events/vectors remain in the actual generic owner. Bind dispatch
 * to AbClassicShotConfig.service, also with context=this; it runs the actual
 * sling vector-publication route before generic services. Default-sling numeric
 * queries already belong to AbClassicSling; custom queries are not synthesized.
 */
bool ab_classic_launch_host_initialize(AbClassicLaunchHostConfig config,
                                       AbClassicLaunchHost *output, AbError *error);
bool ab_classic_launch_host_remaining(void *context, AbShotCall *call, AbError *error);
bool ab_classic_launch_host_dispatch(void *context, AbShotCall *call, AbError *error);
/* Direct AbClassicSlingConfig callbacks for the Museum two-snap profile:
 * castle-to-bird distance first, then published screen center to bird snap.
 * Snap order is the actual Museum bootstrap bird/castle order. Reload audio
 * resolves the live script name, then calls the resource boundary with the
 * supplied gain/non-looping/track0, preserving shared audio RNG. No allocation. */
bool ab_classic_launch_host_camera_distances(void *context, float *castle_to_bird,
                                             float *to_bird, AbError *error);
bool ab_classic_launch_host_reload_audio(void *context, float gain, AbError *error);

#endif
