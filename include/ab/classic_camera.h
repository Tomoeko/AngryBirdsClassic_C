#ifndef AB_CLASSIC_CAMERA_H
#define AB_CLASSIC_CAMERA_H

#include "ab/classic_level.h"
#include "ab/camera_system.h"

typedef struct AbClassicCamera AbClassicCamera;

typedef struct {
    AbClassicLevel *level;
    const AbCameraSnap *snaps;
    size_t snap_count;
    AbVec2 pixel_size;
    float absolute_bottom, horizontal_leeway;
    const float *gravity;
    const AbCameraSequence *shot_sequence, *end_sequence;
    void *resource_context;
    bool (*load_sequence)(void *context, const char *resource_name,
                          const AbCameraSequence **output, AbError *error);
    bool (*sequence_log)(void *context, AbSpan message, AbError *error);
    AbClassicImpactServices prediction;
    void *input_context;
    /* Actual input-handler update at the camera boundary, after gameplay has
     * resolved sling dragging and specialty state. NULL retains explicit
     * caller-prepared input for isolated phase users. */
    bool (*input)(void *context, AbCameraSystem *system, float seconds,
                  AbCameraSystemInput *input, AbError *error);
    size_t storage_bytes;
    /* Optional larger bound for other verified resources served by the same
     * loader, including LevelEndSequence and default/restart level-start
     * sequences. No frame allocation. Zero retains
     * the maximum of the two required follow-sequence sources. */
    size_t sequence_capacity;
    /* Zero retains the initial object count. Reserve stable follow rows for
     * later physical identities explicitly; registration allocates nothing. */
    size_t object_capacity;
} AbClassicCameraConfig;

typedef struct {
    AbCameraSystemView system;
    const AbCameraSequence *loaded_sequence;
    /* SIZE_MAX denotes a nil flying identity or no current follow target. */
    size_t flying_object, target_object;
    size_t object_count, object_capacity;
} AbClassicCameraView;

/* Own a camera system, stable follow targets, world/property service bindings
 * and reusable sequence stages for registered level objects. Borrow the
 * level, snaps, sequence source data, gravity and callback context until close;
 * close this owner before the level or sources. No allocation during updates.
 * Initial launchVelocity remains absent. This composes recovered components;
 * input/controller scheduling and catalog behavior registration stay external.
 * The source sequences and explicit bound determine reusable capacity. The loader
 * supplies the current source at each start; loading and both original log calls
 * precede playback. Loader/logger failures retain preceding effects. Do not reenter
 * updates or destroy borrowed data from these callbacks. Output starts NULL and partial
 * construction is released on failure. */
bool ab_classic_camera_open(const AbClassicCameraConfig *config,
                            AbClassicCamera **output, AbError *error);
void ab_classic_camera_close(AbClassicCamera *camera);

/* Borrow the owned system for ordinary camera actions/bootstrap. Do not close
 * it or replace its follow resolver/environment; those belong to this owner. */
AbCameraSystem *ab_classic_camera_system(AbClassicCamera *camera);
/* Borrowed state and the last loaded mutable stage projection, valid until
 * close or the next sequence start. An empty source can still be loaded. */
AbClassicCameraView ab_classic_camera_view(const AbClassicCamera *camera);

/* Append the next canonical physical identity into pre-reserved follow storage.
 * Does not select flyingBird, start a sequence or publish launchVelocity. Rows
 * and retained active follow pointers stay stable through later registrations.
 * Reject gaps, duplicates, removed objects and exhausted capacity. */
bool ab_classic_camera_register_object(AbClassicCamera *camera, size_t object,
                                       AbError *error);

/* Publish the current flyingBird identity separately from onLaunch. SIZE_MAX
 * represents nil. The launch handler starts the exact shot sequence immediately
 * without writing launchVelocity. Publish that vector through level_properties
 * only after the full shot callback has returned, then update the camera at the
 * recovered frame boundary. Blues initialization cancels before starting.
 * Earlier effects survive a later failure. No physical/script removal occurs. */
bool ab_classic_camera_set_flying(AbClassicCamera *camera, size_t object,
                                  AbError *error);
bool ab_classic_camera_on_launch(AbClassicCamera *camera, AbError *error);
bool ab_classic_camera_start_sequence(AbClassicCamera *camera,
                                      const char *resource_name, AbError *error);
bool ab_classic_camera_blues_initialize(AbClassicCamera *camera, AbError *error);
bool ab_classic_camera_remove_bird(AbClassicCamera *camera, size_t object,
                                   AbError *error);

/* Refresh the active target from the canonical cached script pose and current
 * physical launchVelocity, then execute the owned camera update. Registered-object
 * numeric launch vectors, ordinary world names, direct owned physical tables
 * and the flyingBird alias are supported. Named world entries take precedence;
 * unknown names clear following. The live pigWithMostStrength selector and
 * other truthy target types reject explicitly. Resolved input and the caller's
 * place in the overall controller update remain explicit. No stepping or pose
 * publication occurs here. */
bool ab_classic_camera_update(AbClassicCamera *camera, float seconds,
                              const AbCameraSystemInput *input, AbError *error);

#endif
