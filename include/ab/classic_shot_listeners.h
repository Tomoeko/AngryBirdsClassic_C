#ifndef AB_CLASSIC_SHOT_LISTENERS_H
#define AB_CLASSIC_SHOT_LISTENERS_H

#include "ab/event_manager.h"
#include "ab/settings_statistics.h"

typedef struct AbClassicShotListeners AbClassicShotListeners;

typedef struct {
    void *context;
    bool (*save_settings)(void *context, const AbDataTable *settings, AbError *error);
    bool (*powerup_used)(void *context, const char *name, AbDataValue *output,
                         AbError *error);
    bool (*level_type)(void *context, AbDataValue level, AbDataValue *output,
                       AbError *error);
    bool (*level_text)(void *context, AbDataValue level, AbDataValue *output,
                       AbError *error);
    /* The original RovioAnalytics.logEvent boundary. Implement the local request
     * effect explicitly; no network transport or server response is supplied. */
    bool (*analytics)(void *context, const char *name, AbDataTable *parameters,
                      AbError *error);
} AbClassicShotListenerHost;

typedef struct {
    AbDataValue bird_slinger_limit;
    float current_bird_index, score;
    AbDataValue powerups_enabled, level_name, analytics_present, ios;
} AbClassicShotListenerState;

typedef struct {
    AbSettingsStatistics *settings;
    AbEventManager *events;
    AbClassicShotListenerHost host;
    size_t notifications, storage_bytes;
} AbClassicShotListenerConfig;

/* Own the two recovered EID_BIRD_SHOT listener bindings, stable achievement
 * notifications and analytics parameter tables. Borrow settings, manager, host
 * context and string bytes until close. Initialize the measured ordinary
 * AchievementProcessor limit to 5000; other state starts nil/zero and must be
 * bound to actual controller values before notification. Register achievement
 * then ClassicFlurry explicitly with bind. This is the observed ordinary shot
 * registration state, not other events/modes or subsystem lifecycle scheduling.
 *
 * Shot achievements use the real status graph, always notify unlocked events,
 * mark/save only on the first unlock, and preserve earlier effects on failure.
 * ClassicFlurry first obtains/caches analyticsSessionId through gameStarts,
 * then follows the actual index/score/powerup/level gates. Required services
 * fail when reached if unbound. No dummy persistence or telemetry is provided.
 * The Android iOS==false achievement path returns before the iOS queue; other
 * achievement queue platforms reject explicitly after preceding effects.
 * Level text remains the required original resource-query boundary.
 * Constructor/handler native validation is recorded separately in the ledger.
 *
 * Allocate bounded storage once. No frame allocation or slot reuse. Capacity
 * failure is explicit. Drain/remove observers before close; close fires no game
 * events and does not unregister borrowed bindings. Never reenter/close in a
 * callback. Output starts NULL, failure releases partial storage. */
bool ab_classic_shot_listeners_open(AbClassicShotListenerConfig config,
                                    AbClassicShotListeners **output, AbError *error);
void ab_classic_shot_listeners_close(AbClassicShotListeners *listeners);
size_t ab_classic_shot_listeners_bytes(const AbClassicShotListeners *listeners);
AbClassicShotListenerState *
ab_classic_shot_listeners_state(AbClassicShotListeners *listeners);
bool ab_classic_shot_listeners_bind(AbClassicShotListeners *listeners, AbError *error);

#endif
