#ifndef AB_AMBIENT_PARTICLE_DATA_H
#define AB_AMBIENT_PARTICLE_DATA_H

#include "ab/ambient_particle.h"
#include "ab/lua_data.h"

/* Resolve components.ambientParticles in an explicit complete native pairs
 * entry permutation into independent mutable caller-owned descriptors. Borrow
 * string bytes. Require string particle identifiers and finite numeric amount,
 * x/y, present timer and delay (including zero). Nil/false timer is absent;
 * nil/false delay uses finite min/max. Other coercions, metatables and mutable
 * raw-table aliases remain outside this view. Output count commits on success;
 * descriptor storage is scratch on failure. No allocation, effects or RNG.
 */
bool ab_ambient_particle_data(const AbDataTable *component, const size_t *entry_order,
                              AbAmbientParticle *particles, size_t capacity,
                              size_t *output_count, AbError *error);

#endif
