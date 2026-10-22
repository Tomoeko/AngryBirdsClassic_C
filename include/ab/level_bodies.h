#ifndef AB_LEVEL_BODIES_H
#define AB_LEVEL_BODIES_H

#include "ab/data_order.h"
#include "ab/object_body.h"

typedef struct {
    AbSpan name;
    AbSpan definition_name;
    const AbDataTable *placement;
    AbObjectBody physics;
    /* Explicit typed placement value, or incremented creation counter when nil.
     * Objects that are not controllable retain nil. */
    AbDataValue start_number;
    bool level_goal;
    /* Creation-time cached Lua mass, independent of later queued body types. */
    float initial_mass;
} AbLevelObject;

typedef struct {
    const AbLuaData *level;
    const AbDataTable *catalog;
    void *sprite_context;
    bool (*sprite_size)(void *context, AbSpan name, AbVec2 *size, AbError *error);
    float placement_scale;
    float physics_scale;
    AbVec2 ground_position;
    bool derive_ground_position;
    /* Native ObjectCreation reports each absent definition and returns without
     * creating an object. Notify in constructor traversal order before skipping.
     * The callback borrows the name only during this call and must not mutate
     * source data/world. Native diagnostic formatting is an outer boundary. */
    void *missing_context;
    void (*missing_definition)(void *context, AbSpan definition);
} AbLevelBodySource;

typedef struct {
    AbLevelObject *objects;
    size_t capacity;
    size_t count;
} AbLevelBodies;

typedef struct {
    AbDataEntry *entries;
    size_t entry_capacity;
    AbStringOrderWorkspace strings;
    size_t hash_capacity;
} AbLevelObjectOrder;

/* Initial museum-mode circle/box/convex-polygon body creation from statically read
 * level data. Iterate the original fixed-capacity world constructor, skip the
 * ground entry, create each body/fixture and apply its angle before the next
 * object, then create ground last. No Lua executes. Resource strings/tables are
 * borrowed and must outlive the result. Output objects remain in creation order;
 * each fixture's user data points to its stable output object. Keep the object
 * array alive and unmoved until the world's bodies/fixtures are removed.
 * The sprite callback supplies bounds for shapes without explicit catalog sizes;
 * a missing circle sprite may have zero width and therefore zero radius.
 * Both scale factors are explicit recovered caller inputs. Ground can use an
 * explicit position or MuseumMode's load bounds: initialize left/right to
 * +/-100000, compare x minus/plus each object's full width (including the -1
 * circle sentinel), then use (left+right)*0.5 and Y=5. Rotation is ignored.
 * These initial bounds are separate from later camera limits. No platform
 * configuration is selected.
 * The world must be empty and healthy. Nonempty chains, non-string world
 * keys, invalid catalogs and unsupported geometry reject explicitly. Unknown
 * definitions emit the optional notification and create no object/table entry,
 * matching the original early return. This is
 * physics creation only; scripts, renderers, force-adder/ghost effects, queue
 * ordering and sling motion are separate. Initial joints are created afterward
 * through level_joints.h. Caller supplies disjoint object/order
 * storage. No extra loader workspace is allocated; the world may grow its
 * broadphase within configured limits. On a later failure, count reports successfully
 * created objects and the world may contain partial work, possibly failed.
 */
bool ab_level_bodies_create(AbPhysicsWorld *world, const AbLevelBodySource *source,
                            AbLevelBodies *output, AbStringOrderWorkspace *order,
                            AbError *error);

/* Runtime world-table order after inserting created names into an empty table.
 * Unlike the level constructor, this table grows as objects are added. Output
 * strings.indices contains object-array indices; hash_capacity is the measured
 * final capacity. All workspace arrays are caller-owned, disjoint scratch and
 * may change on failure. No deletion/reinsertion or allocation is modeled.
 */
bool ab_level_bodies_order(const AbLevelBodies *bodies, AbLevelObjectOrder *workspace,
                           AbError *error);

/* Build the separate growing birds table by inserting literal-controllable
 * objects in the supplied runtime-world permutation. Return body-object indices
 * in birds-table iteration order, distinct from launch start numbers. Entries
 * cover bodies.count; index/node scratch covers the filtered table. Storage is
 * caller-owned and disjoint from bodies/insertion_order. Failure may change
 * scratch; count/hash_capacity commit only on success. No allocation, deletion
 * or reinsertion occurs. */
bool ab_level_birds_order(const AbLevelBodies *bodies, const size_t *insertion_order,
                          AbLevelObjectOrder *workspace, size_t *count, AbError *error);

/* The same initial-table construction for truthy levelGoal objects. */
bool ab_level_goals_order(const AbLevelBodies *bodies, const size_t *insertion_order,
                          AbLevelObjectOrder *workspace, size_t *count, AbError *error);

/* MuseumMode selects level.worldGravity or binary32 20, then sets X to zero.
 * Nil/false use the default; numeric zero remains zero. Finite numeric values
 * are supported. Other truthy values reject rather than guessing native boundary
 * coercion. Invalid input leaves output unchanged. */
bool ab_level_gravity(const AbDataTable *level, AbVec2 *output, AbError *error);

/* MuseumMode.loadLevel2711 instructions655..668: select authored
 * physicsToWorld, defaulting to100 only for nil, then divide by the actual
 * global physicsToWorld. This placement scale is separate from physicsScale.
 * False is present and fails arithmetic. Finite numeric inputs/results and a
 * nonzero denominator only; preserve output on failure. */
bool ab_level_placement_scale(const AbDataTable *level, float physics_to_world,
                              float *output, AbError *error);

#endif
