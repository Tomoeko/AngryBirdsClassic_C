#ifndef AB_BLOCK_COMPONENT_H
#define AB_BLOCK_COMPONENT_H

#include "ab/lua_data.h"

typedef enum {
    AB_BLOCK_COMPONENT_FLAME,
    AB_BLOCK_COMPONENT_AMBIENT_ANIMATIONS,
    AB_BLOCK_COMPONENT_AMBIENT_PARTICLES,
    AB_BLOCK_COMPONENT_COUNT,
} AbBlockComponentKind;

/* The three shipped canApply predicates require both definition.components
 * and its selected child to be tables. Non-table values, including numeric
 * zero, true and the string "table", do not qualify. Borrow the selected table
 * or return NULL. Plain tables only; no metatable/global-type callbacks.
 * Invalid storage/kinds preserve output. No effects, allocation or Lua. */
bool ab_block_component(const AbDataTable *definition, AbBlockComponentKind kind,
                        const AbDataTable **output, AbError *error);

#endif
