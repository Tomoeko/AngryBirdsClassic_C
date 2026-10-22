#ifndef AB_OBJECT_FORCE_DATA_H
#define AB_OBJECT_FORCE_DATA_H

#include "ab/object_definition.h"
#include "ab/object_force_step.h"
#include "ab/object_friction.h"

typedef struct {
    AbObjectFriction friction;
    AbObjectForces forces;
    AbObjectForceInterval interval;
    AbObjectForceSound sound;
    const AbDataTable *sound_choices;
    bool prepare_forces;
    bool interval_present;
} AbObjectForceData;

/* Initial ObjectCreation force fields from an already resolved definition.
 * Retain Lua truthiness for the six-field native preparation gate, including
 * numeric zero. Numbers must be finite. Other numeric-field types resolve to
 * missing/zero except strings, whose unverified numeric coercion rejects.
 * Object mass is the creation-time cached value; gravity is a separate native
 * flag. Numeric intervals require current/applied string sprites. Sound is
 * absent, a string or a nonempty table of numbered string choices; numeric-name
 * coercion is outside this initial-data reader. Choice spans remain unresolved
 * in sound.choices; the caller fills them by numeric key from sound_choices.
 * All returned tables/strings borrow the source graph. Failure preserves output.
 * No Lua, resource effects, RNG draws or allocation execute. */
bool ab_object_force_data(const AbObjectDefinition *definition, float initial_mass,
                          bool gravity_enabled, AbDataValue current_sprite,
                          AbObjectForceData *output, AbError *error);

/* Resolve a nonempty initial sound table into numeric 1..count order. All keys
 * must be exact contiguous positive integers and all values valid string spans.
 * The caller provides count disjoint spans; scratch may change on failure.
 * No allocation, string coercion or random selection executes. */
bool ab_object_force_choices(const AbDataTable *table, AbSpan *choices, size_t capacity,
                             AbError *error);

#endif
