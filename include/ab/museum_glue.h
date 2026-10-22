#ifndef AB_MUSEUM_GLUE_H
#define AB_MUSEUM_GLUE_H

#include "ab/lua_data.h"
#include "ab/physics_geometry.h"

typedef struct {
    AbSpan name, plunger_name;
    AbDataValue grabber, grabbed_egg, glued_to_block;
} AbMuseumGlueObject;

typedef struct {
    void *context;
    bool (*collider)(void *context, AbSpan name, AbDataValue *output, AbError *error);
    bool (*object)(void *context, AbSpan name, AbMuseumGlueObject **output,
                   AbError *error);
    bool (*velocity)(void *context, AbSpan name, AbVec2 value, AbError *error);
} AbMuseumGlueHost;

typedef struct {
    bool disable_contact, has_pair;
    AbSpan first, second;
} AbMuseumGlueResult;

/* Resolved MuseumCollisions glue helper. The enclosing callback selects the
 * glue object first, after primary damage and its dead-object checks. Skip a
 * truthy other.grabber before querying its definition; only the exact bird and
 * egg numeric colliders proceed. Resolve glue.plunger_name, set reciprocal egg
 * fields when appropriate, request positive-zero velocity, set glued_to_block,
 * then return disable=true and the current plunger/other names in that order.
 * Already-glued flags do not replace the grabber guard.
 *
 * Plain typed state only; no metatables, coercion, damage, routing, body mutation
 * or queue consumption occurs here. Objects may alias. Host callbacks may change
 * their fields in native order; keep all returned names/objects alive through
 * response consumption. No allocation occurs. Output commits only on success;
 * host failure retains earlier object/host effects. NULL resolved plungers are
 * unsupported and reject before egg/velocity effects. The caller stops on error.
 */
bool ab_museum_glue_contact(AbMuseumGlueObject *glue, AbMuseumGlueObject *other,
                            AbMuseumGlueHost host, AbMuseumGlueResult *output,
                            AbError *error);

typedef struct {
    AbMuseumGlueObject *objects[2];
    AbDataValue colliders[2], dead[2];
} AbMuseumGlueRoute;

/* Enclosing onCollision's post-damage glue branch. Supply definition collider
 * values and original-name dead-map lookups after primary damage has finished.
 * Either truthy dead entry skips. Select the first exact glue collider, otherwise
 * the second, and call the helper with that glue object first. A rejected first
 * glue helper does not fall through to a second glue object.
 *
 * No selected helper preserves the existing response. A selected helper replaces
 * disable/pair even when it returns false/nil, potentially clearing an earlier
 * primary disable decision. The separate new-collision-parameters field is not
 * part of this response and remains the caller's prior value. This does not run
 * primary damage, resolve dead-map keys, mutate definition tables or schedule
 * physics/glue consumption. The helper's borrowing/failure contracts apply.
 */
bool ab_museum_glue_after_damage(const AbMuseumGlueRoute *route, AbMuseumGlueHost host,
                                 AbMuseumGlueResult *response, AbError *error);

#endif
