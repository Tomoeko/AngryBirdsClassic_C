#ifndef AB_OBJECT_PROPERTY_ACCESS_H
#define AB_OBJECT_PROPERTY_ACCESS_H

#include "ab/lua_data.h"

typedef struct {
    AbDataValue name, physics_object;
} AbObjectPropertyAccess;

typedef struct {
    void *context;
    bool (*definition)(void *context, AbDataValue current_object_name,
                       AbDataValue *output, AbError *error);
    AbDataValue materials;
} AbObjectPropertyAccessHost;

/* Museum ObjectFunctions.getObjectProperty for plain typed values. An absent
 * object or falsy physicsObject returns nil before any definition query. The
 * first definition query must be truthy even when the physical field exists.
 * Then return the first truthy physical, newly queried definition, or material
 * field. False falls through; zero, empty strings and tables remain truthy.
 *
 * Read names, fields and bindings at the original boundaries. Material fallback
 * caches the current materials table before a third definition query, then
 * indexes it by that result's materialName. Earlier callbacks may mutate later
 * state and replace bindings; cached tables must stay alive. Repeated queries
 * are intentional. A malformed value rejects only when indexed. No metatables,
 * coercion, allocation, Lua or implicit defaults. Output commits on success;
 * preceding callback effects remain on failure. Names/keys/table storage are
 * borrowed, valid, disjoint from output and stable in lifetime. Do not reenter.
 */
bool ab_object_property_get(AbObjectPropertyAccess *object, AbSpan key,
                            const AbObjectPropertyAccessHost *host, AbDataValue *output,
                            AbError *error);

/* ObjectFunctions.setObjectProperty assigns directly into the current physical
 * table without a definition lookup or truthiness guard. The caller owns its
 * mutable entries/capacity. Existing keys retain their slots, including nil
 * writes; new keys require one free entry even when the value is nil. No table
 * hash iteration order is modeled here. Failure preserves the physical table.
 */
bool ab_object_property_set(AbObjectPropertyAccess *object, AbSpan key,
                            AbDataValue value, AbError *error);

#endif
