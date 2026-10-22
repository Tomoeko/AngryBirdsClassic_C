#ifndef AB_GAME_OBJECT_REMOVAL_H
#define AB_GAME_OBJECT_REMOVAL_H

#include "ab/classic_joints.h"

typedef enum {
    AB_GAME_TELEPORTER_SIMPLE,
    AB_GAME_TELEPORTER_PORTAL,
} AbGameTeleporterKind;

typedef struct {
    AbGameTeleporterKind kind;
    /* Portal's native byte 0x86. Simple teleporters always remove. The portal
     * update state machine owns changes to this flag, outside this boundary. */
    bool keep_on_object_removal;
} AbGameTeleporter;

typedef struct {
    AbGameObjectCoreRegistry *objects;
    AbClassicJoints *joints;
    /* Non-null borrowed AbGameTeleporter handles keyed by physical object name.
     * Retained portals may refer to a physical key that has already gone. */
    AbStringMap *teleporters;
    const AbGameObjectCoreHost *object_host;
    void *teleporter_context;
    /* Optional for borrowed fixed storage. Destroy the selected teleporter
     * before its named registry entry is erased. Must not fail or reenter. */
    void (*destroy_teleporter)(void *context, AbGameTeleporter *teleporter);
} AbGameObjectRemoval;

/* Compose GameLua.removeObject in order: physical prefix, deferred/immediate
 * joint-record cleanup, then conditional teleporter destruction/map erasure.
 * Both suffixes run even when the physical object key was absent. Simple
 * teleporters always remove; portals retain only while keep_on_object_removal
 * is true. Unknown teleporter kinds fail after earlier physical/joint effects.
 *
 * Caller owns stable context, disjoint storage, pinned names/metadata, healthy
 * maps/world and no reentry/rehash. Phase bindings are borrowed and read when
 * reached. Destruction services release owned scene/animation/teleporter data;
 * fixed-storage tests may leave them absent. Success commits physical_removed;
 * failure retains earlier mutations and leaves that output unchanged. Script
 * maps, spawning, portal updates and owned session integration are separate.
 */
bool ab_game_object_remove(AbGameObjectRemoval *removal, AbSpan name,
                           bool *physical_removed, AbError *error);

#endif
