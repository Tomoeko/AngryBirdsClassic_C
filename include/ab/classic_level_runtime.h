#ifndef AB_CLASSIC_LEVEL_RUNTIME_H
#define AB_CLASSIC_LEVEL_RUNTIME_H

#include "ab/classic_playfield.h"

typedef struct AbClassicLevelRuntime AbClassicLevelRuntime;

typedef struct {
    const AbZip *archive;
    const char *level_path;
    const AbClassicLevelSource *source;
    AbRandom48 *random;
    AbWorldDefinition world;
    AbClassicLevelLimits level_limits;
    size_t resource_bytes;
    /* Reserve additional stable adapter rows per authored bird. The dynamic
     * bodies still use level_limits.objects; zero retains initial-only rows. */
    size_t dynamic_objects_per_bird;
    size_t additional_dynamic_objects;
    /* Derive further stable slots from the loaded definitions, before adapters
     * borrow any arrays. This result is added to the reservations above and must
     * fit level_limits.objects. No world mutation or simulation runs here. */
    bool (*reserve_objects)(void *context, AbClassicLevelView level, size_t *additional,
                            AbError *error);
    void *reserve_context;
    AbClassicInitialBirdHost initial_birds;
    AbClassicPlayfieldConfig playfield;
    void *prepare_context;
    bool (*prepare)(void *context, const AbLuaData *level, const AbLuaData *catalog,
                    AbError *error);
} AbClassicLevelRuntimeConfig;

typedef struct {
    AbClassicLevel *level;
    AbClassicPlayfield *playfield;
    size_t first_bird;
    float placement_scale;
    size_t object_capacity;
} AbClassicLevelRuntimeView;

/* Fresh ordinary Museum world/playfield ownership, independent of render API.
 * Borrow the selected APK, preloaded actual sprite provider, shared RNG/audio,
 * settings/events, canonical globals/controller and camera log service.
 * Construct the authored world, prepare the actual follow/start/end and powerup
 * camera resources and snaps,
 * then create the playfield with the final updateValues force (-925) and
 * initGame's 0.05 physics scale. initGame calls updateValues after its earlier
 * temporary -800 assignment; that temporary value must not reach gameplay.
 * Resolve first-bird pose from live physical fields and the actual back sprite.
 * No idle-transfer skip, pre-fired bird or fixture launch state is installed.
 *
 * Caller owns source pre-load global resets, resource-group selection, old-world
 * release, notifications, mode/level scripts and subsequent startLevel/frame/GUI
 * scheduling. This owner covers their world-construction phase, not the entire
 * MuseumMode.loadLevel method or a complete playable session. playfield.level,
 * camera source pointers/loader and start fields are supplied here, except the
 * retained allowResetToBirdCamera capture. Other playfield domain limits apply.
 * Retained allocations use the explicit component limits; packed-data workspaces
 * have their existing separate bounds. Failure frees partial owned storage;
 * source global/renderer/controller effects can survive, requiring discard.
 * Drain consumers and clear borrowed controller/flying references before close.
 * No frame allocation or I/O after open. */
bool ab_classic_level_runtime_open(AbClassicLevelRuntimeConfig config,
                                   AbClassicLevelRuntime **output, AbError *error);
void ab_classic_level_runtime_close(AbClassicLevelRuntime *runtime);
AbClassicLevelRuntimeView
ab_classic_level_runtime_view(const AbClassicLevelRuntime *runtime);
/* Refresh Museum's data-derived snaps and camera drawable geometry between
 * frames. Preserve the live world, camera movement/sequence and all assets.
 * This application geometry boundary does not dispatch root/UI resize events. */
bool ab_classic_level_runtime_resize(AbClassicLevelRuntime *runtime, AbVec2 pixel_size,
                                     AbError *error);

#endif
