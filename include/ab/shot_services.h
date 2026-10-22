#ifndef AB_SHOT_SERVICES_H
#define AB_SHOT_SERVICES_H

#include "ab/event_manager.h"
#include "ab/settings_statistics.h"
#include "ab/slingshot_launch.h"

typedef struct AbShotServices AbShotServices;

typedef struct {
    AbSettingsStatistics *statistics;
    AbEventManager *events;
    AbShotHost remaining_shot;
    AbSlingLaunchHost remaining_sling;
    void *context;
    size_t notifications, storage_bytes;
} AbShotServicesConfig;

/* Bind actual shared statistics and EventManager owners to both launch routes.
 * Own stable notification tables until close. Queue/listener payload aliases
 * therefore survive a launch return. The manager, statistics, bound vector
 * tables and fallback context remain borrowed; drain/remove their observers
 * before closing this owner. No frame allocation or storage reuse; notification
 * exhaustion errors explicitly. Output starts NULL; open fires no services.
 *
 * This provides statistics and notification effects. Other shot/sling services
 * require their real supplied implementation when reached. Listeners are
 * registered by the session, with their observed order and lifetime. No default
 * listeners, particle/audio substitutes, save writes or online responses. */
bool ab_shot_services_open(AbShotServicesConfig config, AbShotServices **output,
                           AbError *error);
void ab_shot_services_close(AbShotServices *services);
size_t ab_shot_services_bytes(const AbShotServices *services);
size_t ab_shot_services_notification_count(const AbShotServices *services);

/* Bind the original vector identities for the next launch. Their mutable x/y
 * number fields must match the launch's C vectors when BIRD_SHOT is reached.
 * The event borrows these exact tables, including aliasing. Notification writes
 * to those fields are published back into the C vectors even after a listener
 * failure. Replacing event.launchPosition/Impulse does not replace the original
 * input vector identity. The session maintains later band/impulse table changes
 * for retained observers. Do not bind temporaries or replace storage in a host.
 * Generic metatables, numeric coercion and non-number vector writes reject. */
bool ab_shot_services_bind_vectors(AbShotServices *services, AbDataTable *position,
                                   AbDataTable *impulse, AbError *error);

/* Host signatures for AbClassicShotConfig and ab_classic_shots_launch_sling.
 * Earlier increments/events survive any later error. Do not reenter either
 * route from a callback or close/rebind its borrowed owners during dispatch. */
bool ab_shot_services_shot(void *context, AbShotCall *call, AbError *error);
bool ab_shot_services_sling(void *context, AbSlingLaunchCall *call, AbError *error);

#endif
