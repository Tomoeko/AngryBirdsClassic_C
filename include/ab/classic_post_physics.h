#ifndef AB_CLASSIC_POST_PHYSICS_H
#define AB_CLASSIC_POST_PHYSICS_H

#include "ab/classic_level.h"
#include "ab/base.h"
#include "ab/lua_data.h"

typedef struct AbClassicPostPhysics AbClassicPostPhysics;

typedef struct {
    AbDataValue pink_timer, bomb_timer, shoot_delay, in_bubble;
} AbClassicPostObject;

typedef enum {
    AB_ROLLING_AUDIO_QUERY,
    AB_ROLLING_AUDIO_PLAY,
    AB_ROLLING_AUDIO_GAIN,
    AB_ROLLING_AUDIO_STOP,
} AbRollingAudioOperation;

typedef struct {
    AbRollingAudioOperation operation;
    AbSpan name;
    float gain;
    bool playing;
} AbRollingAudioCall;

/* QUERY returns playing. PLAY uses the recovered resource boundary: looping,
 * track two, unit speed and three remaining positive-zero parameters. GAIN
 * applies to the named clip; STOP stops that name. Empty names never call the
 * host. Implement actual resource/mixer effects at this boundary. */
typedef bool (*AbRollingAudioHost)(void *context, AbRollingAudioCall *call,
                                   AbError *error);

typedef struct {
    AbDataValue *has_awake, *moving, *above_tolerance;
} AbClassicActivityBindings;

typedef struct {
    int32_t minimum_x, maximum_x;
    AbRollingAudioHost audio;
    void *context;
    size_t storage_bytes;
    size_t object_capacity;
    /* Optional canonical globals. Bind all three or none for isolated callers.
     * Publish actual Boolean values before the rolling-audio pass; disabled
     * physics preserves them. Borrow these stable addresses until close. */
    AbClassicActivityBindings activity;
} AbClassicPostConfig;

typedef struct {
    bool has_awake, moving, above_tolerance;
    bool initialized;
} AbClassicActivityState;

typedef struct {
    AbClassicPostObject *objects;
    size_t count;
    const size_t *source_indices;
    AbDataValue *ignored_definitions;
    const AbClassicActivityState *activity;
    /* Shared native rolling accumulator; physical removal writes present +0. */
    AbStringMap *rolling_levels;
} AbClassicPostView;

/* Own ordinary post-physics bindings in native lexicographic
 * object-name order. Borrow level/body/catalog storage until close. Seed the
 * four specialty fields nil; callers assign live values at their recovered
 * phases. Keep any supplied string/table bytes alive. Initial native rolling
 * names come from the material's truthy rollingSound, otherwise empty, following
 * ObjectCreation's native setter call rather than the script property view.
 * Non-string truthy names reject; coercion remains separate. Circle coefficient
 * is its native stored radius; boxes retain the -1 radius sentinel.
 *
 * Fixed circle/box slots and rolling names remain borrowed. Level-forgotten
 * rows skip publication/activity/rolling updates. Portal collider 5, joints,
 * general insertion/reinsertion, changed shape/radius/name and custom property
 * callbacks require separate recovery. No effects, RNG or world changes run
 * during open. storage_bytes bounds all allocations. Output starts NULL.
 */
bool ab_classic_post_physics_open(AbClassicLevel *level, AbClassicPostConfig config,
                                  AbClassicPostPhysics **output, AbError *error);
void ab_classic_post_physics_close(AbClassicPostPhysics *post);
size_t ab_classic_post_physics_bytes(const AbClassicPostPhysics *post);
AbClassicPostView ab_classic_post_physics_view(AbClassicPostPhysics *post);

/* Register the next canonical direct circle between updates. object_capacity
 * must have reserved its stable specialty row and sorted index storage at open
 * (zero retains the initial count). Native direct circles start with an empty
 * rolling name even after source code copies a material/definition field. Keep
 * that separate from ObjectCreation's initial material setter. Current native
 * collider/radius metadata is captured here; portal collider 5 remains outside
 * this adapter. No allocation, audio, publication or activity update. Refresh
 * the view's count/order after registration; objects retains creation indices. */
bool ab_classic_post_physics_register_circle(AbClassicPostPhysics *post, size_t object,
                                             AbError *error);

/* Enabled phase: publish eligible script properties, calculate activity and
 * shared per-name rolling maxima, then finish each object's old-awake flag.
 * Publish aggregate flags before ordered rolling audio requests. Empty names
 * skip; nonpositive volume stops; positive volume queries playing state, then
 * starts or updates gain. Clear the temporary map after successful dispatch.
 * Disabled physics leaves state and hosts untouched. No stepping, frame-clock
 * advance, body/pose changes, allocation or Lua execution. Required missing
 * audio hosts fail when reached. Later failure retains preceding changes.
 * Caller runs this after interpolation and before the original root update;
 * remaining outer-frame work and general scheduling are separate.
 */
bool ab_classic_post_physics_update(AbClassicPostPhysics *post, bool physics_enabled,
                                    AbError *error);

#endif
