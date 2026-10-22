#include "ab/ambient_particle.h"

#include "ab/coordinates.h"
#include "ab/script_math.h"

#include <math.h>

static bool emit_particle(AbAmbientParticle *particle, float angle,
                          const AbVec2 *object_position, AbAmbientParticleHost host,
                          AbError *error) {
    float cosine, sine;
    if (!object_position || !isfinite(object_position->x) ||
        !isfinite(object_position->y) || !isfinite(particle->offset.x) ||
        !isfinite(particle->offset.y) || !ab_script_cosine(angle, &cosine) ||
        !ab_script_sine(angle, &sine)) {
        return ab_fail(error, "Invalid ambient particle rotation or position");
    }
    AbVec2 offset = {cosine * particle->offset.x - sine * particle->offset.y,
                     sine * particle->offset.x + cosine * particle->offset.y};
    AbVec2 point = {object_position->x + offset.x, object_position->y + offset.y};
    AbAmbientParticleEmission emission = {0};
    if (!isfinite(point.x) || !isfinite(point.y)) {
        return ab_fail(error, "Ambient particle position exceeds finite range");
    }
    bool converted =
        host.physics_to_world
            ? host.physics_to_world(host.context, point, &emission.position, error)
            : ab_coordinate_point(NULL, AB_SPACE_PHYSICS, AB_SPACE_WORLD, point,
                                  &emission.position);
    if (!converted) {
        return ab_fail(error, "Ambient particle world conversion failed");
    }
    emission.particles = particle->particles;
    emission.amount = particle->amount;
    if ((emission.particles.size && !emission.particles.data) ||
        !isfinite(emission.amount) || !isfinite(emission.position.x) ||
        !isfinite(emission.position.y) || !host.emit) {
        return ab_fail(error, "Invalid ambient particle emission");
    }
    return host.emit(host.context, &emission, error);
}

bool ab_ambient_particles_update(AbAmbientParticle *particles, size_t count,
                                 float seconds, float body_angle,
                                 const AbVec2 *object_position, AbRandom48 *random,
                                 AbAmbientParticleHost host, AbError *error) {
    if ((count && !particles) || count > UINT32_C(16777216) ||
        (count && !isfinite(seconds))) {
        return ab_fail(error, "Invalid ambient particle update");
    }
    for (size_t index = 0; index < count; ++index) {
        AbAmbientParticle *particle = particles + index;
        if (!particle->timer_present) {
            float timer = particle->delay;
            if (!particle->delay_present) {
                float unit;
                if (!isfinite(particle->min_delay) || !isfinite(particle->max_delay) ||
                    ab_script_random(random, 0, 0, 0, &unit) != AB_SCRIPT_RANDOM_OK) {
                    return ab_fail(error, "Invalid ambient particle random delay");
                }
                timer = particle->min_delay +
                        unit * (particle->max_delay - particle->min_delay);
            }
            if (!isfinite(timer)) {
                return ab_fail(error, "Invalid ambient particle initial timer");
            }
            particle->timer = timer;
            particle->timer_present = true;
        }
        float timer = particle->timer - seconds;
        if (!isfinite(timer)) {
            return ab_fail(error, "Ambient particle timer exceeds finite range");
        }
        particle->timer = timer;
        if (timer <= 0) {
            particle->timer_present = false;
            if (!emit_particle(particle, body_angle, object_position, host, error)) {
                return false;
            }
        }
    }
    return true;
}
