#ifndef AB_LEVEL_PROPERTIES_H
#define AB_LEVEL_PROPERTIES_H

#include "ab/level_bodies.h"
#include "ab/object_definition_lookup.h"
#include "ab/object_property_access.h"
#include "ab/object_properties.h"

enum { AB_LEVEL_PHYSICAL_PROPERTY_CAPACITY = 96, AB_LEVEL_EXTRA_SPRITE_CAPACITY = 16 };

typedef struct {
    AbDataTable physical, script, behaviors, launch_velocity;
    AbDataEntry physical_entries[AB_LEVEL_PHYSICAL_PROPERTY_CAPACITY];
    AbDataEntry script_entries[3];
    AbDataEntry launch_entries[2];
    AbDataTable extra_sprites;
    AbDataEntry extra_sprite_entries[AB_LEVEL_EXTRA_SPRITE_CAPACITY];
    bool launch_published;
} AbLevelPropertyObject;

typedef struct {
    AbLevelPropertyObject *objects;
    AbDataEntry *world_entries;
    size_t capacity, count;
    AbDataTable world, registry;
    AbDataEntry registry_entries[2];
    AbDataTable dynamic;
    AbObjectDefinitionLookup lookup;
} AbLevelProperties;

/* Bind initial Museum physical name/definition/dimensions and GameObject.init's
 * name, physicsObject, behaviors and physical gameObjectScript identity. All
 * arrays are caller-owned, disjoint and stable until the last borrowed view is
 * released. Names/catalog remain borrowed. This is the property projection of
 * existing bodies, not full script creation or behavior registration. No VM,
 * allocation, hash iteration, dynamic insertion or other physical fields.
 * Failure may change caller storage; discard the binding. */
bool ab_level_properties_initialize(AbLevelProperties *properties,
                                    const AbLevelBodies *bodies,
                                    const AbDataTable *catalog, AbError *error);

/* Bind the initial objects.dynamic table after ordinary property construction.
 * ObjectCreation's old/new paths include every authored object whose resolved
 * collider differs from collider_types.static; queued static physics bodies
 * therefore still belong. Generated ground bypasses this constructor table.
 * Entries remain caller-owned/stable and disjoint from other storage until all
 * property/registry views release.
 * No body effects, Lua, allocation, script hooks or later insertions. Rebinding
 * an existing dynamic table rejects. Failure may change supplied scratch only;
 * success publishes the second canonical registry entry. Source membership is
 * independent of later body-type changes and world/dynamic removal stays split. */
bool ab_level_properties_bind_dynamic(AbLevelProperties *properties,
                                      const AbLevelBodies *bodies, AbDataEntry *entries,
                                      size_t capacity, AbError *error);

/* Project GameObject.init1030 for a raw newly created physical object. Publish
 * its current truthy name (or source GameObject_Unnamed), fresh behaviors,
 * physicsObject and physical gameObjectScript link. Existing script/behavior
 * bindings reject to protect retained identities. This constructs no behavior
 * instances or world/birds/dynamic entries. No allocation; a late physical-table
 * capacity error retains earlier script-field writes. */
bool ab_level_properties_bind_script(AbLevelProperties *properties, size_t object,
                                     AbError *error);

/* Museum property dispatch through the real world/definition/material lookup.
 * Script fields are read anew; physical name writes do not rename the cached
 * script name or the registry key. Set writes only the physical table and can
 * consume the bounded spare entries. Borrowed keys/values must outlive storage.
 * Outputs commit on success; earlier diagnostic effects remain on failure. */
bool ab_level_property_get(AbLevelProperties *properties, size_t object, AbSpan key,
                           AbDataValue *output, AbError *error);
bool ab_level_property_set(AbLevelProperties *properties, size_t object, AbSpan key,
                           AbDataValue value, AbError *error);

/* Publish the physical owner's cached x/y at the resolved constructor,
 * setPosition or updateObjectProperties phase. Write the owned physical table
 * directly, independently of a rebound script.physicsObject. Numeric bits pass
 * unchanged. No body/history changes; later bounded failure retains earlier
 * field writes. This does not initialize other physical fields. */
bool ab_level_properties_publish_position(AbLevelProperties *properties, size_t object,
                                          AbVec2 position, AbError *error);
/* Publish the normalized native setRotation result into the canonical physical
 * table, independently of script.physicsObject. */
bool ab_level_properties_publish_angle(AbLevelProperties *properties, size_t object,
                                       float angle, AbError *error);
/* Original gamelogic.updateObjectProperties, prototype 161393: x, y, xVel,
 * yVel, angle, sleeping, outsideBoundaries, in that order. No world changes;
 * partial storage failure retains preceding fields. Numeric bits are unchanged. */
bool ab_level_properties_publish_motion(AbLevelProperties *properties, size_t object,
                                        const AbObjectProperties *motion,
                                        AbError *error);

/* Publish the first launchVelocity vector after onLaunch returns. Own its
 * stable table in the level, so camera teardown does not invalidate references.
 * Keep the field absent before this call. Existing/non-nil or previously
 * published vectors reject; repeated launch generations require separate
 * lifetime recovery. No allocation; failure preserves the published fields. */
bool ab_level_properties_publish_launch(AbLevelProperties *properties, size_t object,
                                        AbVec2 velocity, AbError *error);

/* Clear Lua-world membership explicitly, independently of physical body/map
 * removal. Keep physical/script tables alive and permit later reads to produce
 * the original missing-object diagnostic through lookup.missing. Idempotent;
 * no physical removal, script destruction or table-order claim. */
bool ab_level_properties_forget(AbLevelProperties *properties, size_t object,
                                AbError *error);

#endif
