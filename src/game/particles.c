#include "ab/particles.h"

#include "ab/rotation.h"
#include "ab/script_math.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const AbDataTable *sprites;
    AbVec2 minimum_velocity, maximum_velocity, gravity;
    float minimum_angle, maximum_angle;
    float minimum_begin, maximum_begin, minimum_end, maximum_end, lifetime;
    float minimum_speed, maximum_speed, minimum_direction, maximum_direction;
    uint32_t animation_loops;
    bool randomized_birth, circle, animated, loaded;
} ParticleSystem;

struct AbParticles {
    AbParticleConfig config;
    ParticleSystem *systems;
    AbParticle *lists[3];
    size_t counts[3], bytes;
    bool calling;
};

static AbSpan span(const char *text) {
    return (AbSpan){(const uint8_t *)text, strlen(text)};
}

static AbDataValue field(const AbDataTable *table, const char *name) {
    const AbDataValue *value =
        ab_data_get(table, (AbDataValue){.kind = AB_DATA_STRING, .string = span(name)});
    return value ? *value : (AbDataValue){0};
}

static bool valid_table(const AbDataTable *table) {
    return table && table->count <= table->capacity &&
           (!table->count || table->entries);
}

static bool same_span(AbSpan first, AbSpan second) {
    return first.size == second.size &&
           (!first.size || (first.data && second.data &&
                            !memcmp(first.data, second.data, first.size)));
}

static bool numeric(const AbDataTable *table, const char *key, float *output,
                    AbError *error) {
    AbDataValue value = field(table, key);
    if (value.kind != AB_DATA_NUMBER)
        return ab_fail(error, "Missing numeric particle field %s", key);
    memcpy(output, &value.bits, sizeof(*output));
    return isfinite(*output) || ab_fail(error, "Nonfinite particle field %s", key);
}

static bool box_velocity(const AbDataTable *table, const char *key, float *output,
                         AbError *error) {
    /* Native createParticles 0x305d08..0x305d40 uses getNumber's raw Lua
     * conversion. newHighScoreStars has an emitter_box with no XY fields;
     * nil becomes positive zero through LuaTable::toNumber/lua_tonumber. */
    if (field(table, key).kind == AB_DATA_NIL) {
        *output = 0;
        return true;
    }
    return numeric(table, key, output, error);
}

static bool prepare_system(AbParticles *owner, size_t index, AbError *error) {
    const AbDataTable *raw = owner->config.sources[index].table;
    if (!valid_table(raw))
        return ab_fail(error, "Invalid particle definition table");
    AbDataValue reload = field(raw, "alwaysLoad");
    if (owner->systems[index].loaded &&
        !(reload.kind == AB_DATA_BOOLEAN && reload.bits))
        return true;
    AbDataValue composite = field(raw, "isCompoSprite");
    AbDataValue animated = field(raw, "animation");
    if ((composite.kind != AB_DATA_NIL &&
         (composite.kind != AB_DATA_BOOLEAN || composite.bits)) ||
        field(raw, "particle_target").kind == AB_DATA_TABLE)
        return ab_fail(error,
                       "Unsupported composite/animated/target particle definition");
    AbDataValue box = field(raw, "emitter_box"), sprites = field(raw, "sprites");
    if (sprites.kind != AB_DATA_TABLE || !valid_table(sprites.table) ||
        !sprites.table->count || sprites.table->count > UINT32_C(0x1000000))
        return ab_fail(error, "Unsupported particle emitter or sprite list");
    ParticleSystem result = {
        .sprites = sprites.table, .animation_loops = 1, .loaded = true};
    if (animated.kind == AB_DATA_STRING) {
        if (!same_span(animated.string, span("lifeTime")))
            return ab_fail(error, "Unsupported particle animation mode");
        result.animated = true;
    } else if (animated.kind == AB_DATA_NUMBER) {
        return ab_fail(error, "Unsupported numeric particle animation mode");
    }
    AbDataValue loop = field(raw, "animationLoop");
    if (loop.kind == AB_DATA_NUMBER) {
        float count = 0;
        if (!numeric(raw, "animationLoop", &count, error))
            return false;
        count = roundf(count);
        if (!(count > 0) || count >= 0x1p31f ||
            (double)count * (double)sprites.table->count > 16777216.0)
            return ab_fail(error, "Unsupported particle animation loop count");
        result.animation_loops = (uint32_t)count;
    }
    AbDataValue randomize = field(raw, "randomizeBirthPosition");
    result.randomized_birth = randomize.kind != AB_DATA_BOOLEAN || randomize.bits;
    if (!numeric(raw, "gravityX", &result.gravity.x, error) ||
        !numeric(raw, "gravityY", &result.gravity.y, error) ||
        !numeric(raw, "minAngleVel", &result.minimum_angle, error) ||
        !numeric(raw, "maxAngleVel", &result.maximum_angle, error) ||
        !numeric(raw, "minScaleBegin", &result.minimum_begin, error) ||
        !numeric(raw, "maxScaleBegin", &result.maximum_begin, error) ||
        !numeric(raw, "minScaleEnd", &result.minimum_end, error) ||
        !numeric(raw, "maxScaleEnd", &result.maximum_end, error) ||
        !numeric(raw, "lifeTime", &result.lifetime, error))
        return false;
    if (box.kind == AB_DATA_TABLE) {
        if (!valid_table(box.table) ||
            !box_velocity(box.table, "minVelX", &result.minimum_velocity.x, error) ||
            !box_velocity(box.table, "maxVelX", &result.maximum_velocity.x, error) ||
            !box_velocity(box.table, "minVelY", &result.minimum_velocity.y, error) ||
            !box_velocity(box.table, "maxVelY", &result.maximum_velocity.y, error))
            return false;
    } else {
        AbDataValue circle = field(raw, "emitter_circle");
        if (circle.kind != AB_DATA_TABLE || !valid_table(circle.table) ||
            !numeric(circle.table, "minVel", &result.minimum_speed, error) ||
            !numeric(circle.table, "maxVel", &result.maximum_speed, error))
            return ab_fail(error, "Unsupported particle circle emitter");
        result.circle = true;
        result.maximum_direction = 360;
        if ((field(circle.table, "minAngleEmitter").kind == AB_DATA_NUMBER &&
             !numeric(circle.table, "minAngleEmitter", &result.minimum_direction,
                      error)) ||
            (field(circle.table, "maxAngleEmitter").kind == AB_DATA_NUMBER &&
             !numeric(circle.table, "maxAngleEmitter", &result.maximum_direction,
                      error)))
            return false;
    }
    if (!(result.lifetime > 0))
        return ab_fail(error, "Unsupported nonpositive particle lifetime");
    owner->systems[index] = result;
    return true;
}

static bool random_float(AbParticles *owner, float *output, AbError *error) {
    double value;
    if (!ab_engine_random_next(owner->config.random, &value))
        return ab_fail(error, "Invalid particle engine RNG state");
    *output = (float)value;
    return true;
}

static bool range(AbParticles *owner, float minimum, float maximum, float *output,
                  AbError *error) {
    float unit = 0;
    if (!random_float(owner, &unit, error))
        return false;
    float difference = maximum - minimum;
    float scaled = difference * unit;
    *output = minimum + scaled;
    return isfinite(*output) || ab_fail(error, "Particle random range overflow");
}

static bool sprite_name(ParticleSystem *system, size_t index, AbSpan *output,
                        AbError *error) {
    float key_number = (float)(index + 1);
    AbDataValue key = {.kind = AB_DATA_NUMBER};
    memcpy(&key.bits, &key_number, sizeof(key_number));
    const AbDataValue *sprite = ab_data_get(system->sprites, key);
    if (!sprite || sprite->kind != AB_DATA_STRING ||
        (sprite->string.size && !sprite->string.data))
        return ab_fail(error, "Invalid dense particle sprite entry");
    *output = sprite->string;
    return true;
}

static bool resolve_sprite(AbParticles *owner, AbParticle *particle, AbError *error) {
    if (!owner->config.sprite)
        return ab_fail(error, "Missing particle resource lookup service");
    return owner->config.sprite(owner->config.context, particle->sprite,
                                &particle->sprite_handle, error);
}

static bool create_particle(AbParticles *owner, size_t index,
                            const AbParticleEmission *emission, size_t list,
                            int32_t layer, AbError *error) {
    ParticleSystem *system = owner->systems + index;
    AbParticle result = {.definition = owner->config.sources[index].name,
                         .position = emission->position,
                         .lifetime = system->lifetime,
                         .layer = layer,
                         .frame = 1,
                         .system = index,
                         .animated = system->animated};
    if (system->randomized_birth) {
        float x = 0, y = 0;
        if (!random_float(owner, &x, error) || !random_float(owner, &y, error))
            return false;
        x = (x - 0.5f) * emission->size.x;
        y = (y - 0.5f) * emission->size.y;
        AbRotation rotation;
        if (!ab_rotation_from_angle(emission->angle, &rotation))
            return ab_fail(error, "Unsupported particle birth rotation");
        float position_y = result.position.y + x * rotation.sine;
        position_y = position_y + y * rotation.cosine;
        float position_x = result.position.x + x * rotation.cosine;
        position_x = position_x - y * rotation.sine;
        result.position = (AbVec2){position_x, position_y};
    }
    if (system->circle) {
        float speed = 0, direction = 0;
        if (!range(owner, system->minimum_speed, system->maximum_speed, &speed,
                   error) ||
            !range(owner, system->minimum_direction, system->maximum_direction,
                   &direction, error) ||
            !ab_script_polar(direction * 0x1.1df46ap-6f, speed, &result.velocity) ||
            !range(owner, system->minimum_direction, system->maximum_direction,
                   &direction, error))
            return ab_fail(error, "Unsupported particle circle velocity/angle");
        float radians = direction * (0x1.921fb6p+1f / 180.0f);
        float period = 0x1.921fb6p+1f + 0x1.921fb6p+1f;
        result.angle = radians - period * floorf(radians / period);
        result.angle = result.angle + 0.0f;
    } else if (!range(owner, system->minimum_velocity.x, system->maximum_velocity.x,
                      &result.velocity.x, error) ||
               !range(owner, system->minimum_velocity.y, system->maximum_velocity.y,
                      &result.velocity.y, error)) {
        return false;
    }
    if (!range(owner, system->minimum_angle, system->maximum_angle,
               &result.angular_velocity, error) ||
        !range(owner, system->minimum_begin, system->maximum_begin, &result.begin_scale,
               error) ||
        !range(owner, system->minimum_end, system->maximum_end, &result.end_scale,
               error))
        return false;
    result.scale = result.begin_scale;
    if (system->animated) {
        if (!sprite_name(system, 0, &result.sprite, error))
            return false;
    } else {
        double unit = 0;
        if (!ab_engine_random_next(owner->config.random, &unit))
            return ab_fail(error, "Invalid particle sprite RNG state");
        size_t sprite_index = (size_t)(unit * (double)system->sprites->count);
        if (!sprite_name(system, sprite_index, &result.sprite, error))
            return false;
    }
    if (!resolve_sprite(owner, &result, error))
        return false;
    size_t capacity = list == 2 ? owner->config.level_capacity : owner->config.capacity;
    if (owner->counts[list] == capacity)
        return ab_fail(error, "Particle list capacity exhausted");
    owner->lists[list][owner->counts[list]++] = result;
    return true;
}

bool ab_particles_open(AbParticleConfig config, AbParticles **output, AbError *error) {
    if (!output)
        return ab_fail(error, "Missing particle owner output");
    *output = NULL;
    if (!config.random || (config.source_count && !config.sources) ||
        config.capacity > UINT32_C(0x1000000) ||
        config.level_capacity > UINT32_C(0x1000000) ||
        config.storage_bytes < sizeof(AbParticles))
        return ab_fail(error, "Invalid particle owner binding or byte limit");
    size_t remaining = config.storage_bytes - sizeof(AbParticles);
    if (config.source_count > remaining / sizeof(ParticleSystem))
        return ab_fail(error, "Particle system cache exceeds byte limit");
    remaining -= config.source_count * sizeof(ParticleSystem);
    if (config.capacity > remaining / (2 * sizeof(AbParticle)))
        return ab_fail(error, "Particle lists exceed byte limit");
    remaining -= config.capacity * 2 * sizeof(AbParticle);
    if (config.level_capacity > remaining / sizeof(AbParticle))
        return ab_fail(error, "Level particle list exceeds byte limit");
    AbParticles *owner = calloc(1, sizeof(*owner));
    if (!owner)
        return ab_fail(error, "Cannot allocate particle owner");
    owner->config = config;
    owner->bytes = sizeof(*owner) + config.source_count * sizeof(ParticleSystem) +
                   config.capacity * 2 * sizeof(AbParticle) +
                   config.level_capacity * sizeof(AbParticle);
    if (config.source_count)
        owner->systems = calloc(config.source_count, sizeof(*owner->systems));
    if (config.capacity) {
        owner->lists[0] = calloc(config.capacity, sizeof(*owner->lists[0]));
        owner->lists[1] = calloc(config.capacity, sizeof(*owner->lists[1]));
    }
    if (config.level_capacity)
        owner->lists[2] = calloc(config.level_capacity, sizeof(*owner->lists[2]));
    if ((config.source_count && !owner->systems) ||
        (config.capacity && (!owner->lists[0] || !owner->lists[1])) ||
        (config.level_capacity && !owner->lists[2])) {
        ab_particles_close(owner);
        return ab_fail(error, "Cannot allocate particle arrays");
    }
    *output = owner;
    return true;
}

void ab_particles_close(AbParticles *owner) {
    if (owner) {
        free(owner->systems);
        free(owner->lists[0]);
        free(owner->lists[1]);
        free(owner->lists[2]);
        free(owner);
    }
}

size_t ab_particles_bytes(const AbParticles *owner) {
    return owner ? owner->bytes : 0;
}

AbParticleAdmission *ab_particles_admission(AbParticles *owner) {
    return owner ? &owner->config.admission : NULL;
}

size_t ab_particles_count(const AbParticles *owner, bool alternate) {
    return owner ? owner->counts[alternate ? 1 : 0] : 0;
}

const AbParticle *ab_particles_at(const AbParticles *owner, bool alternate,
                                  size_t index) {
    size_t list = alternate ? 1u : 0u;
    return owner && index < owner->counts[list] ? owner->lists[list] + index : NULL;
}

static bool emit(AbParticles *owner, const AbParticleEmission *emission, size_t list,
                 int32_t layer, AbError *error) {
    if (!isfinite(emission->amount))
        return ab_fail(error, "Nonfinite particle emission amount");
    AbParticleEmission request = *emission;
    if (!request.ignore_limits) {
        AbParticleAdmission admission = owner->config.admission;
        if (!isfinite(admission.soft_multiplier) || owner->counts[list] > INT32_MAX)
            return ab_fail(error, "Invalid particle admission state");
        float count = (float)owner->counts[list];
        if (count + request.amount > (float)admission.soft_limit)
            request.amount = request.amount * admission.soft_multiplier;
        if ((float)admission.hard_limit < count + request.amount)
            request.amount =
                (float)((int64_t)admission.hard_limit - (int64_t)owner->counts[list]);
        if (!(request.amount > 0))
            return true;
    }
    if (!isfinite(request.position.x) || !isfinite(request.position.y) ||
        !isfinite(request.size.x) || !isfinite(request.size.y) ||
        !isfinite(request.angle))
        return ab_fail(error, "Nonfinite particle emission geometry");
    size_t index = 0;
    while (index < owner->config.source_count &&
           !same_span(request.definition, owner->config.sources[index].name))
        ++index;
    if (index == owner->config.source_count) {
        size_t length = request.definition.size < 128 ? request.definition.size : 128;
        return ab_fail(error, "Unknown particle definition: %.*s", (int)length,
                       request.definition.data ? (const char *)request.definition.data
                                               : "");
    }
    bool okay = prepare_system(owner, index, error);
    size_t created = 0;
    while (okay && (float)created < request.amount) {
        okay = create_particle(owner, index, &request, list, layer, error);
        ++created;
    }
    return okay;
}

bool ab_particles_emit(AbParticles *owner, const AbParticleEmission *emission,
                       AbError *error) {
    if (!owner || !emission || owner->calling || !isfinite(emission->amount))
        return ab_fail(error, "Invalid or reentrant particle emission");
    owner->calling = true;
    bool okay = emit(owner, emission, emission->alternate ? 1u : 0u, 0, error);
    owner->calling = false;
    return okay;
}

bool ab_particles_emit_level(AbParticles *owner, const AbParticleEmission *emission,
                             AbError *error) {
    if (!owner || !emission || owner->calling)
        return ab_fail(error, "Invalid or reentrant level particle emission");
    owner->calling = true;
    double layer = 0;
    bool okay = ab_engine_random_next(owner->config.random, &layer);
    if (okay)
        okay = emit(owner, emission, 2, layer > 0.5 ? 1 : 2, error);
    else
        ab_fail(error, "Invalid level particle engine RNG state");
    owner->calling = false;
    return okay;
}

size_t ab_particles_level_count(const AbParticles *owner) {
    return owner ? owner->counts[2] : 0;
}

const AbParticle *ab_particles_level_at(const AbParticles *owner, size_t index) {
    return owner && index < owner->counts[2] ? owner->lists[2] + index : NULL;
}

static bool update_list(AbParticles *owner, float seconds, size_t list, bool alternate,
                        AbError *error) {
    size_t index = 0;
    while (index < owner->counts[list]) {
        AbParticle *particle = owner->lists[list] + index;
        particle->time = particle->time + seconds;
        if (particle->time > particle->lifetime ||
            (!alternate && particle->position.y > 0)) {
            size_t following = owner->counts[list] - index - 1;
            if (following)
                memmove(particle, particle + 1, following * sizeof(*particle));
            --owner->counts[list];
            continue;
        }
        const ParticleSystem *system = owner->systems + particle->system;
        float fraction = particle->time / particle->lifetime;
        particle->velocity.y = particle->velocity.y + seconds * system->gravity.y;
        particle->velocity.x = particle->velocity.x + seconds * system->gravity.x;
        float difference = particle->end_scale - particle->begin_scale;
        particle->position.y = particle->position.y + seconds * particle->velocity.y;
        particle->position.x = particle->position.x + seconds * particle->velocity.x;
        particle->scale = particle->begin_scale + difference * fraction;
        particle->angle = particle->angle + seconds * particle->angular_velocity;
        if (particle->animated) {
            size_t frame_count = system->sprites->count * system->animation_loops;
            float selected = ceilf(fraction * (float)frame_count);
            uint32_t frame = selected<1 ? 1u : selected>(float) frame_count
                                 ? (uint32_t)frame_count
                                 : (uint32_t)selected;
            if (frame != particle->frame) {
                size_t sprite_index = (frame - 1) % system->sprites->count;
                if (!sprite_name(owner->systems + particle->system, sprite_index,
                                 &particle->sprite, error) ||
                    !resolve_sprite(owner, particle, error))
                    return false;
                particle->frame = frame;
            }
        }
        if (!isfinite(particle->time) || !isfinite(particle->position.x) ||
            !isfinite(particle->position.y) || !isfinite(particle->velocity.x) ||
            !isfinite(particle->velocity.y) || !isfinite(particle->scale) ||
            !isfinite(particle->angle))
            return ab_fail(error, "Particle update arithmetic overflow");
        ++index;
    }
    return true;
}

bool ab_particles_update(AbParticles *owner, float seconds, bool alternate,
                         AbError *error) {
    if (!owner || owner->calling)
        return ab_fail(error, "Invalid or reentrant particle update");
    if (!owner->counts[alternate ? 1u : 0u])
        return true;
    if (!isfinite(seconds))
        return ab_fail(error, "Nonfinite particle update duration");
    owner->calling = true;
    bool okay = update_list(owner, seconds, alternate ? 1u : 0u, alternate, error);
    owner->calling = false;
    return okay;
}

bool ab_particles_update_level(AbParticles *owner, float seconds, AbError *error) {
    if (!owner || owner->calling)
        return ab_fail(error, "Invalid or reentrant level particle update");
    if (!owner->counts[2])
        return true;
    if (!isfinite(seconds))
        return ab_fail(error, "Nonfinite level particle update duration");
    owner->calling = true;
    bool okay = update_list(owner, seconds, 2, false, error);
    owner->calling = false;
    return okay;
}
