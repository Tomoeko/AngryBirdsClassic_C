#ifndef AB_AMBIENT_ANIMATION_DATA_H
#define AB_AMBIENT_ANIMATION_DATA_H

#include "ab/ambient_animation.h"
#include "ab/lua_data.h"

typedef struct {
    AbAmbientAnimationVariant *variants;
    size_t variant_capacity;
    AbSpan *sprites;
    size_t sprite_capacity;
} AbAmbientAnimationStorage;

typedef struct {
    AbAmbientAnimationVariant *variants;
    size_t count, sprite_count;
} AbAmbientAnimationConfiguration;

/* Resolve components.ambientAnimations in an explicit complete native pairs
 * permutation of its entries. Each row becomes an independent mutable variant
 * with borrowed, dense sprite strings. Caller storage remains stable while used.
 * Require finite numeric speed, numeric delay or nil/false plus finite min/max,
 * and an optional finite numeric timer. Numeric strings, metatables, sparse
 * sprite arrays and shared mutable descriptor aliases are outside this view.
 * Output commits only on success; all storage arrays are scratch on failure.
 */
bool ab_ambient_animation_data(const AbDataTable *component, const size_t *entry_order,
                               AbAmbientAnimationStorage storage,
                               AbAmbientAnimationConfiguration *output, AbError *error);

#endif
