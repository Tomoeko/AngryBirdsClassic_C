#ifndef AB_CLASSIC_BLUES_SPLIT_H
#define AB_CLASSIC_BLUES_SPLIT_H

#include "ab/classic_blues_child.h"
#include "ab/classic_birds.h"

typedef enum {
    /* Values1..8 retain AbClassicBluesChildStage. */
    AB_BLUES_SPLIT_TUTORIAL = 9,
    AB_BLUES_SPLIT_PUFF,
    AB_BLUES_SPLIT_NORMALIZE,
    AB_BLUES_SPLIT_TELEPORT,
    AB_BLUES_SPLIT_CAMERA,
    AB_BLUES_SPLIT_BEHAVIOR,
    AB_BLUES_SPLIT_AUDIO_NAME,
    AB_BLUES_SPLIT_AUDIO_PLAY,
} AbClassicBluesSplitStage;

typedef struct {
    AbClassicPlayfieldView playfield;
    AbClassicBirds *birds;
    AbClassicObjectRemoval *physical;
    AbSettingsStatistics *settings;
    AbAudioBank *audio;
    AbStringMap *hats, *teleporters;
    AbDataValue *flying;
    AbBuffer names;
    /* Optional read-only diagnostics before actual source calls, including
     * child stages1..8. Never mutate, reenter or release borrowed state here. */
    void *trace_context;
    void (*trace)(void *context, unsigned stage);
} AbClassicBluesSplitConfig;

typedef struct {
    size_t objects[3];
    AbSpan names[3];
    AbVec2 direction;
} AbClassicBluesSplitResult;

typedef struct {
    AbClassicBluesSplitConfig config;
    AbClassicBluesChild child;
    /* Read the last successful result before the next create call when the
     * enclosing owner must register spawned identities with later adapters.
     * A failed call retains this prior result, together with its failed flag. */
    AbClassicBluesSplitResult last_result;
    bool initialized, calling, failed, has_result;
} AbClassicBluesSplit;

/* triggerSpecialty125163's CLUSTER_BOMB branch (124..211), with actual source
 * child129468 and utils.vNormalize10029. Borrow actual canonical owners/maps,
 * settings and shared flying slot. The stable name pool survives all retained
 * registry/table/event aliases. Keep this binding at its initialized address.
 * Initialize has no source effects or defaults.
 * The playfield must already borrow birds' canonical otherBirds field.
 *
 * Create publishes tutorial state and puff before normalization and children;
 * transfers the actual hat handle, calls the native teleporter boundary, then
 * replaces otherBirds, publishes flyingBird, starts the actual Blues camera,
 * sets originalCluster and plays actual definition audio in source order.
 * The original parent remains alive: the enclosing specialty performs its hook,
 * event, parent behavior dispatch and final removeBird after this branch.
 *
 * New identities register with shot/camera/physical/bird owners without rerunning
 * initial catalog setup. Simulation/contact/post/character registration remains
 * the caller's responsibility before its next corresponding pass. No frame
 * scheduling or input policy is supplied. Active parent teleporters explicitly
 * reject until their native cloning services are implemented; absence is read
 * from the supplied actual map. Finite binary32 numeric domain only. No Lua
 * execution or frame allocation beyond bounded world/renderer growth. Failure
 * retains earlier effects, leaves output unchanged and stops this binding.
 */
bool ab_classic_blues_split_initialize(AbClassicBluesSplitConfig config,
                                       AbClassicBluesSplit *output, AbError *error);
bool ab_classic_blues_split_create(AbClassicBluesSplit *split, size_t parent,
                                   AbClassicBluesSplitResult *output, AbError *error);

#endif
