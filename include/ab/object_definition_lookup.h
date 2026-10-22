#ifndef AB_OBJECT_DEFINITION_LOOKUP_H
#define AB_OBJECT_DEFINITION_LOOKUP_H

#include "ab/lua_data.h"

typedef struct {
    AbDataValue objects, block_table;
    void *context;
    /* Original missing-object print: "Object not found:" followed immediately
     * by these name bytes. Return failure for an unavailable/rejected service. */
    bool (*missing)(void *context, AbSpan name, AbError *error);
} AbObjectDefinitionLookup;

/* gamelogic.getObject: index objects.world by an ordinary scalar/table key.
 * Nil and NaN keys miss. No diagnostic, coercion, metatable or allocation.
 * Borrowed table/key storage stays valid; output commits on success. */
bool ab_object_world_lookup(AbObjectDefinitionLookup *lookup, AbDataValue key,
                            AbDataValue *output, AbError *error);

/* gamelogic.getObjectDefinition for string names and ordinary tables. Missing
 * world[name] prints then returns nil. False entries fail when indexed.
 * Otherwise capture blockTable.blocks before rereading world[name].definition;
 * return that indexed value unchanged, including nil. No material fallback.
 * Scalar/table-identity keys are supported; numeric NaNs never match. No
 * allocation, coercion, metatables or VM. Data remains borrowed and stable;
 * output commits on success. Diagnostics may mutate state but must not free
 * storage or reenter. Earlier callback effects survive failure. */
bool ab_object_definition_lookup(AbObjectDefinitionLookup *lookup, AbSpan name,
                                 AbDataValue *output, AbError *error);

#endif
