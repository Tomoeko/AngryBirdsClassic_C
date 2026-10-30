#include "ab/classic_museum_collisions.h"

#include <math.h>
#include <string.h>

typedef struct {
    size_t index;
    AbSpan name;
    AbDataTable *physical;
    AbObjectDefinition definition;
    uint32_t collider;
} MuseumObject;

static AbSpan span(const char *name) {
    return (AbSpan){(const uint8_t *)name, strlen(name)};
}

static AbDataValue text(AbSpan value) {
    return (AbDataValue){.kind = AB_DATA_STRING, .string = value};
}

static AbDataValue number(float value) {
    AbDataValue result = {.kind = AB_DATA_NUMBER};
    memcpy(&result.bits, &value, sizeof(value));
    return result;
}

static AbDataValue field(const AbDataTable *table, const char *name) {
    const AbDataValue *value = ab_data_get(table, text(span(name)));
    return value ? *value : (AbDataValue){0};
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL && (value.kind != AB_DATA_BOOLEAN || value.bits);
}

static bool same_span(AbSpan first, AbSpan second) {
    return first.size == second.size &&
           (!first.size || (first.data && second.data &&
                            !memcmp(first.data, second.data, first.size)));
}

static bool finite_number(AbDataValue value, float *output, AbError *error) {
    if (value.kind != AB_DATA_NUMBER)
        return ab_fail(error, "Non-numeric Museum collision property");
    float result;
    memcpy(&result, &value.bits, sizeof(result));
    if (!isfinite(result))
        return ab_fail(error, "Nonfinite Museum collision property");
    *output = result;
    return true;
}

static bool resolve(AbClassicMuseumCollisions *owner, AbSpan name, MuseumObject *output,
                    AbError *error) {
    size_t index = 0;
    while (index < owner->level.bodies->count &&
           !same_span(owner->level.bodies->objects[index].name, name))
        ++index;
    if (index == owner->level.bodies->count)
        return ab_fail(error, "Unknown initial Museum collision object");
    AbDataTable *physical = &owner->level.properties->objects[index].physical;
    AbDataValue current;
    if (!ab_object_world_lookup(&owner->level.properties->lookup, text(name), &current,
                                error))
        return false;
    AbDataValue live_name = field(physical, "name");
    AbDataValue identifier = field(physical, "definition");
    if (current.kind != AB_DATA_TABLE || current.table != physical ||
        live_name.kind != AB_DATA_STRING || !same_span(live_name.string, name) ||
        identifier.kind != AB_DATA_STRING)
        return ab_fail(error, "Missing or rebound Museum collision identity");
    MuseumObject result = {.index = index, .name = name, .physical = physical};
    float collider = 0;
    if (!ab_object_definition_open(owner->level.catalog, identifier.string, NULL,
                                   &result.definition, error) ||
        !finite_number(result.definition.collider, &collider, error))
        return false;
    if (collider < 0 || collider >= (float)AB_GAME_COLLIDER_TYPES ||
        floorf(collider) != collider)
        return ab_fail(error, "Invalid Museum definition collider");
    result.collider = (uint32_t)collider;
    *output = result;
    return true;
}

static bool assign(MuseumObject *object, const char *key, AbDataValue value,
                   AbError *error) {
    AbObjectPropertyAccess access = {
        .physics_object = {.kind = AB_DATA_TABLE, .table = object->physical}};
    return ab_object_property_set(&access, span(key), value, error);
}

static bool script(AbClassicMuseumCollisions *owner, const MuseumObject *object,
                   bool *present, AbError *error) {
    AbDataValue value = field(object->physical, "gameObjectScript");
    if (value.kind == AB_DATA_NIL) {
        *present = false;
        return true;
    }
    if (value.kind != AB_DATA_TABLE ||
        value.table != &owner->level.properties->objects[object->index].script)
        return ab_fail(error, "Unsupported Museum collision script receiver");
    *present = true;
    return true;
}

static bool momentum_body(const MuseumObject *object, AbLegacyCollisionBody *output,
                          AbError *error) {
    return finite_number(field(object->physical, "mass"), &output->mass, error) &&
           finite_number(field(object->physical, "xVel"), &output->velocity.x, error) &&
           finite_number(field(object->physical, "yVel"), &output->velocity.y, error);
}

static bool factor(AbClassicMuseumCollisions *owner, const MuseumObject *bird,
                   const MuseumObject *target, const char *kind, float *output,
                   AbError *error) {
    AbDataValue all = field(owner->level.catalog, "damageFactors");
    AbDataValue group = ab_object_property(&bird->definition, span("damageFactors"));
    AbDataValue material =
        ab_object_property(&target->definition, span("materialName"));
    if (all.kind != AB_DATA_TABLE || !all.table)
        return ab_fail(error, "Missing Museum damage-factor catalog");
    const AbDataValue *definition = ab_data_get(all.table, group);
    if (!definition || definition->kind != AB_DATA_TABLE || !definition->table)
        return ab_fail(error, "Missing Museum bird damage-factor group");
    AbDataValue values = field(definition->table, kind);
    if (values.kind != AB_DATA_TABLE || !values.table)
        return ab_fail(error, "Missing Museum material multiplier table");
    const AbDataValue *value = ab_data_get(values.table, material);
    if (!value || !truthy(*value)) {
        *output = 1;
        return true;
    }
    return finite_number(*value, output, error);
}

static bool request_joint(void *context, AbSpan name, AbError *error) {
    AbClassicMuseumCollisions *owner = context;
    if (!owner->config.break_joint)
        return ab_fail(error, "Missing Museum deferred joint-break service");
    return owner->config.break_joint(owner->config.context, name, error);
}

static bool notify_bird(AbClassicMuseumCollisions *owner, const AbGameContactCall *call,
                        const MuseumObject *bird, const MuseumObject *target,
                        AbMuseumCollisionKind kind, float force, float damage,
                        AbError *error) {
    AbMuseumCallbackContact geometry;
    if (!ab_museum_callback_contact(kind, call->normal, call->point, &geometry))
        return ab_fail(error, "Invalid Museum bird contact geometry");
    AbBirdCollisionNotifyInput input = {.contact = {.first = bird->name,
                                                    .second = target->name,
                                                    .force = force,
                                                    .damage = damage,
                                                    .normal = geometry.normal,
                                                    .point = geometry.point},
                                        .point_present = geometry.point_present};
    if (!owner->config.bird)
        return ab_fail(error, "Missing complete owned Museum bird callback");
    return owner->config.bird(owner->config.context, &input, error);
}

static bool single_bird(AbClassicMuseumCollisions *owner, AbGameContactCall *call,
                        MuseumObject *bird, MuseumObject *target,
                        AbMuseumCollisionKind kind, AbError *error) {
    AbLegacyCollisionBody momentum = {0};
    AbBirdDamageInput input = {.velocity_modifier = 1};
    if (!momentum_body(bird, &momentum, error) ||
        !factor(owner, bird, target, "damageMultiplier", &input.damage_multiplier,
                error) ||
        !factor(owner, bird, target, "velocityMultiplier", &input.velocity_multiplier,
                error) ||
        !script(owner, bird, &input.bird_script_present, error) ||
        !script(owner, target, &input.target_script_present, error))
        return false;
    input.mass = momentum.mass;
    input.velocity = momentum.velocity;
    if (input.bird_script_present) {
        AbDataValue modifier;
        if (!ab_level_property_get(owner->level.properties, bird->index,
                                   span("velocityMultiplierModifier"), &modifier,
                                   error))
            return false;
        if (truthy(modifier) &&
            !finite_number(modifier, &input.velocity_modifier, error))
            return false;
    }
    AbDataValue strength = field(target->physical, "strength");
    input.has_target_strength = truthy(strength);
    if (input.has_target_strength) {
        if (!finite_number(strength, &input.target_strength, error))
            return false;
        if (input.target_script_present) {
            AbDataValue defence;
            if (!ab_level_property_get(owner->level.properties, target->index,
                                       span("defence"), &defence, error))
                return false;
            if (truthy(defence) &&
                !finite_number(defence, &input.target_defence, error))
                return false;
        }
        if (input.bird_script_present) {
            AbDataValue ignore;
            if (!input.target_script_present)
                return ab_fail(error, "Missing Museum target ignore-damage getter");
            if (!ab_level_property_get(owner->level.properties, target->index,
                                       span("ignoreAllDamage"), &ignore, error))
                return false;
            input.target_ignores_damage = truthy(ignore);
        }
    }
    input.legacy_velocity =
        truthy(ab_object_property(&bird->definition, span("useLegacyCollisionPath")));
    AbBirdDamage damage;
    if (!ab_bird_collision_damage(&input, &damage))
        return ab_fail(error, "Unsupported owned Museum bird damage");
    if (!ab_classic_joints_break(owner->level.joints, target->name, damage.joint_force,
                                 request_joint, owner, error))
        return false;
    if (input.has_target_strength &&
        damage.remaining_strength != input.target_strength) {
        if (!assign(target, "strength", number(damage.remaining_strength), error))
            return false;
        owner->level.character_objects[target->index].goal.strength =
            damage.remaining_strength;
    }
    if (damage.mark_dead &&
        !ab_string_map_set(owner->config.dead_blocks, target->name,
                           (uintptr_t)&owner->config.physical_values[target->index],
                           true, error))
        return false;
    if (damage.velocity_change == AB_BIRD_VELOCITY_IMMEDIATE) {
        size_t body = owner->level.bodies->objects[bird->index].physics.body;
        if (!ab_world_has_body(owner->level.world, body) ||
            !ab_body_velocity_set(&owner->level.world->storage.bodies[body].motion,
                                  damage.velocity))
            return ab_fail(error, "Cannot assign owned Museum collision velocity");
    } else if (damage.velocity_change == AB_BIRD_VELOCITY_DEFERRED) {
        owner->velocity_entries[0] =
            (AbDataEntry){text(span("birdName")), text(bird->name)};
        owner->velocity_entries[1] = (AbDataEntry){
            text(span("afterCollisionVelocityX")), number(damage.velocity.x)};
        owner->velocity_entries[2] = (AbDataEntry){
            text(span("afterCollisionVelocityY")), number(damage.velocity.y)};
    }
    if (!notify_bird(owner, call, bird, target, kind, damage.collision_force,
                     damage.callback_damage, error))
        return false;
    call->response.should_disable_contact = damage.disable_contact;
    if (damage.velocity_change == AB_BIRD_VELOCITY_DEFERRED)
        call->response.new_collision_parameters =
            (AbDataValue){.kind = AB_DATA_TABLE, .table = &owner->velocity};
    return true;
}

bool ab_classic_museum_collisions_open(AbClassicMuseumCollisionConfig config,
                                       AbClassicMuseumCollisions *output,
                                       AbError *error) {
    AbClassicLevelView level = ab_classic_level_view(config.level);
    if (!output || !level.bodies || !level.properties || !config.blocks ||
        !config.dead_blocks || level.bodies->count > config.physical_capacity ||
        (level.bodies->count && !config.physical_values))
        return ab_fail(error, "Invalid owned Museum collision binding");
    *output = (AbClassicMuseumCollisions){.config = config, .level = level};
    output->velocity = (AbDataTable){output->velocity_entries, 3, 3, NULL};
    for (size_t index = 0; index < level.bodies->count; ++index)
        config.physical_values[index] = (AbDataValue){
            .kind = AB_DATA_TABLE, .table = &level.properties->objects[index].physical};
    return true;
}

static bool dispatch(AbClassicMuseumCollisions *owner, AbGameContactCall *call,
                     AbError *error) {
    MuseumObject objects[2];
    if (!resolve(owner, call->first, objects, error) ||
        !resolve(owner, call->second, objects + 1, error))
        return false;
    if (objects[0].collider == AB_COLLIDER_GLUE ||
        objects[1].collider == AB_COLLIDER_GLUE) {
        if (!owner->config.glue)
            return ab_fail(error, "Missing complete owned Museum glue collision route");
        return owner->config.glue(owner->config.context, call, error);
    }
    AbMuseumCollisionKind kind;
    if (!ab_museum_collision_kind(objects[0].collider, objects[1].collider, &kind))
        return ab_fail(error, "Invalid Museum collision route");
    if (kind == AB_MUSEUM_BLOCK_COLLISION)
        return ab_classic_block_collisions_contact(owner->config.blocks, call, error);
    if (kind != AB_MUSEUM_BIRD_PAIR_COLLISION) {
        size_t first = kind == AB_MUSEUM_FIRST_BIRD_COLLISION ? 0 : 1;
        return single_bird(owner, call, objects + first, objects + (1 - first), kind,
                           error);
    }
    AbLegacyCollisionBody bodies[2] = {0};
    float force = 0;
    return momentum_body(objects, bodies, error) &&
           momentum_body(objects + 1, bodies + 1, error) &&
           (ab_bird_pair_force(bodies, &force) ||
            ab_fail(error, "Unsupported owned Museum bird-pair force")) &&
           notify_bird(owner, call, objects, objects + 1, kind, force, 0, error);
}

bool ab_classic_museum_collisions_contact(void *context, AbGameContactCall *call,
                                          AbError *error) {
    AbClassicMuseumCollisions *owner = context;
    if (!owner || !call || owner->calling || (call->first.size && !call->first.data) ||
        (call->second.size && !call->second.data))
        return ab_fail(error, "Invalid or reentrant owned Museum contact");
    owner->calling = true;
    bool okay = dispatch(owner, call, error);
    owner->calling = false;
    return okay;
}
