#ifndef AB_DAMAGE_PARTICLES_H
#define AB_DAMAGE_PARTICLES_H

#include "ab/ambient_particle.h"
#include "ab/lua_data.h"

/* CollisionCallbacks' damage-state particle helper. Resolve raw_definition from
 * blockTable.blocks[object.definition], independently of a merged definition.
 * A nil damageStateChangedParticles skips without reading position or hosts.
 * Otherwise convert the current physics position before reading the cached
 * descriptor's name/amount, then emit the original three zero/two false trailing
 * arguments through the shared particle boundary. A NULL transform host uses
 * the measured fixed physics-to-world conversion.
 *
 * Borrow well-formed plain tables, position and names through callback return.
 * The transform host may edit descriptor fields or replace the raw definition's
 * binding; the cached descriptor remains selected. Name must be a string and
 * amount finite numeric; malformed non-nil descriptors fail after conversion.
 * No allocation, RNG or particle simulation occurs. A later error preserves
 * prior host effects. */
bool ab_damage_state_particles(const AbDataTable *raw_definition,
                               const AbVec2 *object_position,
                               AbAmbientParticleHost host, AbError *error);

#endif
