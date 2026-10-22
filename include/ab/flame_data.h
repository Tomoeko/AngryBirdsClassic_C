#ifndef AB_FLAME_DATA_H
#define AB_FLAME_DATA_H

#include "ab/flame_effect.h"
#include "ab/lua_data.h"

typedef struct {
    AbSpan *sprites;
    size_t sprite_capacity;
    AbFlameOffset *offsets;
    size_t offset_capacity;
} AbFlameStorage;

typedef struct {
    AbFlameDefinition definition;
    float timer_speed, scale;
} AbFlameConfiguration;

/* Resolve a plain components.flame table into caller-owned ordered sprite and
 * named-offset arrays, retaining borrowed string bytes. Require dense 1-based
 * sprite keys, string sprite names and finite numeric x/y offsets. Nil/false
 * speed or scale defaults to one; numeric zero is retained. Other typed values,
 * sparse arrays, numeric strings and metatable behavior are outside this view.
 * Configuration commits only on success; scratch arrays may change on failure.
 */
bool ab_flame_data(const AbDataTable *component, AbFlameStorage storage,
                   AbFlameConfiguration *output, AbError *error);

#endif
