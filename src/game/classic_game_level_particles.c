#include "ab/classic_game_level_particles.h"

#include <math.h>
#include <string.h>

static AbDataValue text(const char *name) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)name, strlen(name)}};
}

static AbDataValue field(const AbDataTable *table, const char *name) {
    const AbDataValue *value = ab_data_get(table, text(name));
    return value ? *value : (AbDataValue){0};
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL && (value.kind != AB_DATA_BOOLEAN || value.bits);
}

static bool numeric(AbDataValue value, float *output, AbError *error) {
    if (value.kind != AB_DATA_NUMBER)
        return ab_fail(error, "Unsupported game level-particle number");
    memcpy(output, &value.bits, sizeof(*output));
    return isfinite(*output) || ab_fail(error, "Nonfinite game level-particle number");
}

static bool table(AbDataValue value, AbDataTable **output, AbError *error) {
    if (value.kind != AB_DATA_TABLE || !value.table ||
        value.table->count > value.table->capacity ||
        (value.table->count && !value.table->entries))
        return ab_fail(error, "Unsupported game level-particle table");
    *output = value.table;
    return true;
}

static bool amount(const AbDataTable *source, float *output, AbError *error) {
    AbDataTable *settings = NULL;
    return table(field(source, "settingsFrame"), &settings, error) &&
           numeric(field(settings, "amount"), output, error);
}

static bool emit(AbClassicGameLevelParticleConfig *config, const AbDataTable *source,
                 bool first, AbError *error) {
    AbDataValue definition = field(source, "particles");
    if (definition.kind != AB_DATA_STRING ||
        (definition.string.size && !definition.string.data))
        return ab_fail(error, "Unsupported game level-particle definition name");
    AbParticleEmission emission = {.definition = definition.string, .amount = 1};
    if (first) {
        if (!amount(source, &emission.amount, error))
            return false;
        emission.amount *= 15;
        if (!isfinite(emission.amount))
            return ab_fail(error, "Game level-particle burst amount overflow");
    }
    if (!numeric(field(source, "x"), &emission.position.x, error) ||
        !numeric(field(source, "y"), &emission.position.y, error) ||
        !numeric(field(source, "width"), &emission.size.x, error))
        return false;
    if (first) {
        if (!truthy(field(source, "startAtGroundLevel")))
            emission.size.y = emission.position.y;
        emission.position.y /= 3;
    } else {
        AbDataValue height = field(source, "height");
        if (truthy(height) && !numeric(height, &emission.size.y, error))
            return false;
    }
    return ab_particles_emit_level(config->particles, &emission, error);
}

static bool update(AbClassicGameLevelParticleConfig *config, float seconds,
                   AbError *error) {
    if (config->level_particles->kind == AB_DATA_NIL || !truthy(*config->enabled))
        return true;
    AbDataTable *source = NULL;
    if (!table(*config->level_particles, &source, error))
        return false;
    AbDataValue first = field(source, "firstFrame");
    if (first.kind == AB_DATA_BOOLEAN && first.bits) {
        if (!emit(config, source, true, error))
            return false;
        const AbDataValue *slot = ab_data_get(source, text("firstFrame"));
        /* The literal-true branch proves the key exists. Preserve table identity
         * and its key slot; only the original Boolean assignment changes. */
        if (!slot)
            return ab_fail(error, "Rebound firstFrame during level-particle emission");
        for (size_t index = 0; index < source->count; ++index) {
            if (&source->entries[index].value == slot) {
                source->entries[index].value = (AbDataValue){.kind = AB_DATA_BOOLEAN};
                break;
            }
        }
        return true;
    }
    float timer = *config->timer + seconds;
    if (!isfinite(*config->timer) || !isfinite(seconds) || !isfinite(timer))
        return ab_fail(error, "Unsupported levelParticlesTimer arithmetic");
    *config->timer = timer;
    float rate = 0;
    if (!amount(source, &rate, error))
        return false;
    float interval = 1.0f / rate;
    if (!(interval < timer) || !*config->physics_enabled)
        return true;
    *config->timer = 0;
    return emit(config, source, false, error);
}

bool ab_classic_game_level_particles_initialize(AbClassicGameLevelParticleConfig config,
                                                AbClassicGameLevelParticles *output,
                                                AbError *error) {
    if (!output || !config.particles || !config.level_particles || !config.enabled ||
        !config.physics_enabled || !config.timer)
        return ab_fail(error, "Invalid concrete game level-particle bindings");
    *output = (AbClassicGameLevelParticles){.config = config, .initialized = true};
    return true;
}

bool ab_classic_game_level_particles_update(AbClassicGameLevelParticles *particles,
                                            float seconds, float time, AbError *error) {
    if (!particles || !particles->initialized || particles->updating ||
        particles->failed)
        return ab_fail(error, "Invalid, failed or reentrant game level particles");
    particles->updating = true;
    AbClassicGameLevelParticleConfig *config = &particles->config;
    bool okay = update(config, seconds, error);
    if (okay && config->extra_update)
        okay = config->extra_update(config->context, seconds, time, error);
    particles->updating = false;
    particles->failed = !okay;
    return okay;
}
