#ifndef AB_NATIVE_EVENTS_H
#define AB_NATIVE_EVENTS_H

#include "ab/base.h"

typedef struct AbNativeEvents AbNativeEvents;

typedef struct {
    void *context;
    bool (*call)(void *context, AbError *error);
} AbNativeEventCall;

typedef struct {
    size_t pending, delayed, processing, cursor;
} AbNativeEventState;

/* The native main event queue is distinct from the script EventManager.
 * Own bounded queue storage once. Copy callable records when posted; their
 * contexts remain borrowed until drained or closed. Single-threaded caller
 * projection only; native worker queues/thread synchronization remain open.
 * Empty callable records fail when reached, after advancing the cursor.
 */
bool ab_native_events_open(size_t capacity, size_t storage_bytes,
                           AbNativeEvents **output, AbError *error);
void ab_native_events_close(AbNativeEvents *events);
size_t ab_native_events_bytes(const AbNativeEvents *events);
bool ab_native_events_post(AbNativeEvents *events, float delay_seconds,
                           AbNativeEventCall call, AbError *error);
/* Negative delay posts to pending; zero (including -0) uses the delayed list.
 * Process pending first. A positive delay is decremented without firing until
 * a later process call, even when the subtraction reaches/passes zero.
 * Posts made by callbacks wait until the next call. Return pending count only.
 * Finite signed timing; failure retains prior effects and stops this owner.
 */
bool ab_native_events_process(AbNativeEvents *events, float seconds, size_t *pending,
                              AbError *error);
bool ab_native_events_state(const AbNativeEvents *events, AbNativeEventState *state);
bool ab_native_events_delay(const AbNativeEvents *events, size_t index, float *seconds,
                            AbNativeEventCall *call);

typedef struct {
    AbNativeEvents *events;
    const size_t *system_count;
    bool *updating_systems;
    bool updating, failed;
} AbNativeAnimation;

/* AnimationWrapper.update processes native events with 0, then sets its system
 * flag while visiting the host registry. This scoped binding accepts the actual
 * empty registry observed in initialized Level1. Nonempty systems require their
 * real component owner and reject after the original preceding event effects.
 * No allocation. Seconds are unused in this empty-system profile.
 */
bool ab_native_animation_initialize(AbNativeEvents *events, const size_t *system_count,
                                    bool *updating_systems, AbNativeAnimation *output,
                                    AbError *error);
bool ab_native_animation_update(AbNativeAnimation *animation, float seconds,
                                AbError *error);

#endif
