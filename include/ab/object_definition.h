#ifndef AB_OBJECT_DEFINITION_H
#define AB_OBJECT_DEFINITION_H

#include "ab/lua_data.h"

typedef struct {
    const AbDataTable *source;
    const AbDataTable *material;
    const AbDataTable *overrides;
    AbDataValue collider;
    AbDataValue z_order;
} AbObjectDefinition;

/* Classic object-definition view over recovered blocks.lua globals. Tables and
 * string bytes remain borrowed and immutable. Optional instance overrides apply
 * to density and z_order with Lua truthiness, including numeric zero. Material
 * fallback is limited to the fields observed in ObjectFunctions.createObject;
 * collider selects the first non-nil definition/material value, then zero.
 * Missing draw order follows the target's collider/density branches. This
 * resolves properties only; it does not create bodies or execute Lua behavior.
 * Invalid catalogs or unknown names preserve output and report an error.
 */
bool ab_object_definition_open(const AbDataTable *catalog, AbSpan name,
                               const AbDataTable *overrides, AbObjectDefinition *output,
                               AbError *error);
AbDataValue ab_object_property(const AbObjectDefinition *definition, AbSpan field);

#endif
