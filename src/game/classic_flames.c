#include "ab/classic_flames.h"
#include "ab/classic_controller.h"
#include "ab/game_sprite.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    size_t object;
    const AbDataTable *component;
    AbFlameState state;
    AbFlameConfiguration configuration;
    AbSpan *frames;
    AbFlameOffset *offsets;
    AbLuaAssetSpriteBinding *bindings;
    char *key;
    size_t key_size;
    AbDataTable effect, offset;
    AbDataEntry fields[9], point[2];
    AbObjectUpdates updates;
    AbObjectUpdateList promotion;
    const AbObjectUpdateFunction *functions[2];
    AbExtraDrawFunction draw;
    bool applied;
} FlameObject;

struct AbClassicFlames {
    AbClassicFlameConfig config;
    AbClassicLevelView level;
    FlameObject *rows;
    AbClassicObjectHooks *registered;
    AbObjectUpdateFunction update;
    size_t count, bytes;
    bool bound, failed;
};

static AbDataValue text(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)value, strlen(value)}};
}

static AbDataValue field(const AbDataTable *table, const char *name) {
    const AbDataValue *value = ab_data_get(table, text(name));
    return value ? *value : (AbDataValue){0};
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL && (value.kind != AB_DATA_BOOLEAN || value.bits);
}

static AbDataValue number(float value) {
    AbDataValue result = {.kind = AB_DATA_NUMBER};
    memcpy(&result.bits, &value, sizeof(value));
    return result;
}

static bool numeric(AbDataValue value, float *output, AbError *error) {
    if (value.kind != AB_DATA_NUMBER)
        return ab_fail(error, "Flame requires its numeric source property");
    memcpy(output, &value.bits, sizeof(*output));
    return true;
}

static bool reserve(AbClassicFlames *owner, size_t count, size_t element,
                    AbError *error) {
    if (count > (owner->config.storage_bytes - owner->bytes) / element)
        return ab_fail(error, "FlameEffect exceeds its retained storage bound");
    owner->bytes += count * element;
    return true;
}

static const AbDataTable *component(const AbDataTable *definition) {
    AbDataValue components = field(definition, "components");
    AbDataValue flame = components.kind == AB_DATA_TABLE
                            ? field(components.table, "flame")
                            : (AbDataValue){0};
    return flame.kind == AB_DATA_TABLE ? flame.table : NULL;
}

static bool definition(AbClassicFlames *owner, size_t object,
                       const AbDataTable **output, AbError *error) {
    AbSpan name = owner->level.bodies->objects[object].name;
    *output = NULL;
    if (name.size == 6 && !memcmp(name.data, "ground", 6))
        return true;
    AbDataValue value;
    if (!ab_object_definition_lookup(&owner->level.properties->lookup, name, &value,
                                     error))
        return false;
    if (value.kind == AB_DATA_TABLE)
        *output = value.table;
    return true;
}

static bool allocate_row(AbClassicFlames *owner, FlameObject *row,
                         const AbDataTable *source, AbError *error) {
    row->component = component(source);
    AbDataValue sprites = field(row->component, "sprites");
    AbDataValue offsets = field(row->component, "offset");
    AbSpan name = owner->level.bodies->objects[row->object].name;
    static const char prefix[] = "drawFlameEffect_";
    if (sprites.kind != AB_DATA_TABLE || !sprites.table ||
        offsets.kind != AB_DATA_TABLE || !offsets.table ||
        name.size > SIZE_MAX - sizeof(prefix))
        return ab_fail(error, "Invalid actual flame component storage");
    size_t frames = sprites.table->count, points = offsets.table->count;
    row->key_size = sizeof(prefix) - 1 + name.size;
    if (!reserve(owner, frames, sizeof(*row->frames) + sizeof(*row->bindings), error) ||
        !reserve(owner, points, sizeof(*row->offsets), error) ||
        !reserve(owner, row->key_size + 1, 1, error))
        return false;
    if (frames) {
        row->frames = calloc(frames, sizeof(*row->frames));
        row->bindings = calloc(frames, sizeof(*row->bindings));
    }
    if (points)
        row->offsets = calloc(points, sizeof(*row->offsets));
    row->key = malloc(row->key_size + 1);
    if ((frames && (!row->frames || !row->bindings)) || (points && !row->offsets) ||
        !row->key)
        return ab_fail(error, "Cannot allocate actual flame resources/metadata");
    memcpy(row->key, prefix, sizeof(prefix) - 1);
    memcpy(row->key + sizeof(prefix) - 1, name.data, name.size);
    row->key[row->key_size] = 0;
    if (!ab_flame_data(row->component,
                       (AbFlameStorage){row->frames, frames, row->offsets, points},
                       &row->configuration, error))
        return false;
    for (size_t frame = 0; frame < frames; ++frame)
        if (!ab_lua_asset_sprite_bind(owner->config.sprites, row->frames[frame],
                                      row->bindings + frame, error))
            return false;
    row->effect = (AbDataTable){row->fields, 9, 9, NULL};
    row->offset = (AbDataTable){row->point, 2, 2, NULL};
    row->promotion = (AbObjectUpdateList){row->functions, 0, 2};
    const char *const names[] = {"definition", "timer",  "timerSpeed",
                                 "scale",      "sprite", "newAngle",
                                 "oldAngle",   "angle",  "offset"};
    for (size_t index = 0; index < 9; ++index)
        row->fields[index].key = text(names[index]);
    row->point[0].key = text("x");
    row->point[1].key = text("y");
    return true;
}

static bool update_object(void *context, void *receiver, float seconds, AbError *error);

bool ab_classic_flames_open(AbClassicFlameConfig config, AbClassicFlames **output,
                            AbError *error) {
    if (!output)
        return ab_fail(error, "Missing FlameEffect owner output");
    *output = NULL;
    AbClassicLevelView level = ab_classic_level_view(config.level);
    if (!level.bodies || !level.properties ||
        config.storage_bytes < sizeof(AbClassicFlames))
        return ab_fail(error, "Invalid FlameEffect owners/storage");
    AbClassicFlames *owner = calloc(1, sizeof(*owner));
    if (!owner)
        return ab_fail(error, "Cannot allocate FlameEffect owner");
    owner->config = config;
    owner->level = level;
    owner->bytes = sizeof(*owner);
    owner->update = (AbObjectUpdateFunction){owner, update_object};
    for (size_t object = 0; object < level.bodies->count; ++object) {
        const AbDataTable *source;
        if (!definition(owner, object, &source, error))
            goto release;
        owner->count += component(source) != NULL;
    }
    if (owner->count &&
        (!config.random || !config.extra_draws || !config.sprites ||
         ab_classic_render_scene_level(config.scene) != config.level ||
         !isfinite(config.physics_scale) || config.physics_scale == 0)) {
        ab_fail(error, "Active FlameEffect requires its real services/physics scale");
        goto release;
    }
    if (!reserve(owner, owner->count, sizeof(*owner->rows), error))
        goto release;
    if (owner->count) {
        owner->rows = calloc(owner->count, sizeof(*owner->rows));
        if (!owner->rows) {
            ab_fail(error, "Cannot allocate flame effect/hook rows");
            goto release;
        }
    }
    size_t row = 0;
    for (size_t object = 0; object < level.bodies->count; ++object) {
        const AbDataTable *source;
        if (!definition(owner, object, &source, error))
            goto release;
        if (!component(source))
            continue;
        owner->rows[row].object = object;
        if (!allocate_row(owner, owner->rows + row++, source, error))
            goto release;
    }
    if (!ab_classic_object_hooks_open(config.level, config.storage_bytes - owner->bytes,
                                      &owner->registered, error))
        goto release;
    owner->bytes += ab_classic_object_hooks_bytes(owner->registered);
    *output = owner;
    return true;
release:
    ab_classic_flames_close(owner);
    return false;
}

void ab_classic_flames_close(AbClassicFlames *owner) {
    if (!owner)
        return;
    if (owner->rows)
        for (size_t index = 0; index < owner->count; ++index) {
            free(owner->rows[index].bindings);
            free(owner->rows[index].frames);
            free(owner->rows[index].offsets);
            free(owner->rows[index].key);
        }
    ab_classic_object_hooks_close(owner->registered);
    free(owner->rows);
    free(owner);
}

size_t ab_classic_flames_bytes(const AbClassicFlames *owner) {
    return owner ? owner->bytes : 0;
}

static void publish(FlameObject *row) {
    AbFlameState *state = &row->state;
    AbDataValue values[] = {
        {.kind = AB_DATA_TABLE, .table = (AbDataTable *)row->component},
        number(state->timer),
        number(state->timer_speed),
        number(state->scale),
        state->sprite_present
            ? (AbDataValue){.kind = AB_DATA_STRING, .string = state->sprite}
            : (AbDataValue){0},
        state->angle_present ? number(state->new_angle) : (AbDataValue){0},
        state->angle_present ? number(state->old_angle) : (AbDataValue){0},
        state->angle_present ? number(state->angle) : (AbDataValue){0},
        state->angle_present
            ? (AbDataValue){.kind = AB_DATA_TABLE, .table = &row->offset}
            : (AbDataValue){0}};
    for (size_t field = 0; field < 9; ++field)
        row->fields[field].value = values[field];
    row->point[0].value = number(state->offset.x);
    row->point[1].value = number(state->offset.y);
}

static bool physical_set(AbClassicFlames *owner, FlameObject *row, const char *name,
                         AbDataValue value, AbError *error) {
    AbObjectPropertyAccess access = {
        .physics_object = {
            .kind = AB_DATA_TABLE,
            .table = &owner->level.properties->objects[row->object].physical}};
    return ab_object_property_set(&access, text(name).string, value, error);
}

static bool read_state(FlameObject *row, AbError *error) {
    AbDataValue definition = field(&row->effect, "definition");
    if (definition.kind != AB_DATA_TABLE || definition.table != row->component)
        return ab_fail(error,
                       "Flame definition replacement requires its resource binding");
    if (!numeric(field(&row->effect, "timer"), &row->state.timer, error) ||
        !numeric(field(&row->effect, "timerSpeed"), &row->state.timer_speed, error) ||
        !numeric(field(&row->effect, "scale"), &row->state.scale, error))
        return false;
    AbDataValue angle = field(&row->effect, "angle");
    row->state.angle_present = truthy(angle);
    if (row->state.angle_present && !numeric(angle, &row->state.angle, error))
        return false;
    return true;
}

static bool body_step(AbClassicFlames *owner, FlameObject *row, float seconds,
                      AbFlameStep *output, AbError *error) {
    const AbDataTable *physical =
        &owner->level.properties->objects[row->object].physical;
    AbDataValue name = field(physical, "name");
    AbSpan expected = owner->level.bodies->objects[row->object].name;
    if (name.kind != AB_DATA_STRING || name.string.size != expected.size ||
        (expected.size &&
         (!name.string.data || memcmp(name.string.data, expected.data, expected.size))))
        return ab_fail(error, "Flame requires its canonical named native body");
    size_t body = owner->level.bodies->objects[row->object].physics.body;
    if (!ab_world_has_body(owner->level.world, body))
        return ab_fail(error, "Flame native angle/velocity body is absent");
    AbDataValue sprite = field(physical, "sprite");
    if (sprite.kind != AB_DATA_NIL && sprite.kind != AB_DATA_STRING)
        return ab_fail(error, "Unsupported live flame object sprite");
    const AbBodyMassState *motion =
        &owner->level.world->storage.bodies[body].motion.mass;
    *output = (AbFlameStep){.seconds = seconds,
                            .body_angle = motion->angle,
                            .velocity = motion->linear_velocity,
                            .object_sprite = sprite.string,
                            .object_sprite_present = sprite.kind == AB_DATA_STRING};
    return true;
}

static bool update_object(void *context, void *receiver, float seconds,
                          AbError *error) {
    AbClassicFlames *owner = context;
    size_t index = 0;
    while (index < owner->count &&
           receiver !=
               &owner->level.properties->objects[owner->rows[index].object].physical)
        ++index;
    if (owner->failed || index == owner->count)
        return ab_fail(error, "Invalid or stopped flame update receiver");
    FlameObject *row = owner->rows + index;
    AbDataValue effect = field(receiver, "flameEffect");
    if (effect.kind != AB_DATA_TABLE || effect.table != &row->effect)
        return ab_fail(error, "Flame update has a rebound effect table");
    AbFlameStep step;
    bool okay = read_state(row, error) &&
                body_step(owner, row, seconds, &step, error) &&
                ab_flame_update(&row->state, &row->configuration.definition, &step,
                                owner->config.random, error);
    if (okay)
        publish(row);
    owner->failed = !okay;
    return okay;
}

typedef struct {
    AbClassicFlames *owner;
    FlameObject *row;
} ApplyCall;

static bool register_update(void *context, AbFlameState *state,
                            const AbFlameDefinition *definition, AbError *error) {
    ApplyCall *call = context;
    FlameObject *row = call->row;
    (void)state;
    (void)definition;
    publish(row);
    if (!physical_set(call->owner, row, "flameEffect",
                      (AbDataValue){.kind = AB_DATA_TABLE, .table = &row->effect},
                      error) ||
        !ab_object_update_add(&row->updates, &call->owner->update, &row->promotion,
                              error) ||
        !physical_set(call->owner, row, "updateFunction",
                      (AbDataValue){.kind = AB_DATA_OBJECT, .object = &row->updates},
                      error))
        return false;
    return ab_classic_object_hooks_register(call->owner->registered, row->object,
                                            &row->updates, error);
}

static bool draw_object(void *context, void *parameter, AbExtraDrawInput input,
                        AbError *error) {
    AbClassicFlames *owner = context;
    FlameObject *row = parameter;
    const AbDataTable *physical =
        &owner->level.properties->objects[row->object].physical;
    AbDataValue name = field(physical, "name");
    if (name.kind != AB_DATA_STRING || (name.string.size && !name.string.data))
        return ab_fail(error, "Flame draw has no actual object name");
    AbSpan expected = owner->level.bodies->objects[row->object].name;
    if (name.string.size != expected.size ||
        (expected.size && memcmp(name.string.data, expected.data, expected.size)))
        return ab_fail(error, "Renamed flame draw requires its registration key owner");
    const AbDataValue *world = ab_data_get(&owner->level.properties->world, name);
    if (!world || !truthy(*world))
        return ab_extra_draws_remove(owner->config.extra_draws,
                                     (AbSpan){(const uint8_t *)row->key, row->key_size},
                                     error);
    AbDataValue effect = field(physical, "flameEffect");
    if (effect.kind != AB_DATA_TABLE || effect.table != &row->effect)
        return ab_fail(error, "Flame draw has a rebound effect table");
    if (!read_state(row, error))
        return false;
    AbVec2 position;
    if (!numeric(field(physical, "x"), &position.x, error) ||
        !numeric(field(physical, "y"), &position.y, error))
        return false;
    AbClassicSceneView objects =
        ab_classic_scene_view(ab_classic_render_scene_objects(owner->config.scene));
    if (row->object >= objects.count)
        return ab_fail(error, "Flame scale query has no native object");
    AbFlameDraw draw;
    if (!ab_flame_draw(&row->state, true, position,
                       objects.items[row->object].pose.scale.x, &draw, error))
        return false;
    if (!draw.sprite_present)
        return ab_fail(error,
                       "Flame nil sprite requires its native resource argument route");
    size_t frame = 0;
    while (frame < row->configuration.definition.count &&
           (draw.sprite.size != row->frames[frame].size ||
            (draw.sprite.size &&
             memcmp(draw.sprite.data, row->frames[frame].data, draw.sprite.size))))
        ++frame;
    if (frame == row->configuration.definition.count)
        return ab_fail(error,
                       "Flame sprite replacement requires its prepared resource");
    AbDrawCommand command;
    bool visible;
    return ab_game_sprite_command(row->bindings + frame,
                                  (AbGameSpritePose){draw.position,
                                                     {draw.scale, draw.scale},
                                                     draw.angle,
                                                     1,
                                                     owner->config.physics_scale},
                                  input.camera, input.width, input.height, &command,
                                  &visible, error) &&
           (!visible || ab_draw_list_push(input.list, &command, error));
}

static bool register_draw(void *context, AbFlameState *state, float z, AbError *error) {
    ApplyCall *call = context;
    FlameObject *row = call->row;
    (void)state;
    publish(row);
    row->draw = (AbExtraDrawFunction){.context = call->owner,
                                      .parameter = row,
                                      .draw = draw_object,
                                      .z = z,
                                      .z_present = true};
    row->applied = true;
    return ab_extra_draws_set(call->owner->config.extra_draws,
                              (AbSpan){(const uint8_t *)row->key, row->key_size},
                              &row->draw, error);
}

bool ab_classic_flames_apply(AbClassicFlames *owner, size_t object,
                             const AbDataTable *definition, AbError *error) {
    if (!owner || owner->failed || owner->bound)
        return ab_fail(error, "Invalid, stopped or already bound flame application");
    size_t index = 0;
    while (index < owner->count && owner->rows[index].object != object)
        ++index;
    if (index == owner->count || component(definition) != owner->rows[index].component)
        return ab_fail(error, "Flame application has no prepared source component");
    FlameObject *row = owner->rows + index;
    AbFlameStep step;
    float z;
    ApplyCall call = {owner, row};
    bool okay =
        numeric(field(definition, "z_order"), &z, error) &&
        body_step(owner, row, 0, &step, error) &&
        ab_flame_apply(&row->state,
                       &(AbFlameCreation){&row->configuration.definition, &step, &z,
                                          row->configuration.timer_speed,
                                          row->configuration.scale},
                       owner->config.random,
                       (AbFlameHost){&call, register_update, register_draw}, error);
    owner->failed = !okay;
    return okay;
}

bool ab_classic_flames_bind(AbClassicFlames *owner, AbClassicController *controller,
                            AbGameTailHooks **output, AbError *error) {
    if (!owner || owner->failed || owner->bound || !controller || !output)
        return ab_fail(error, "Invalid flame controller binding");
    if (!ab_classic_object_hooks_bind(owner->registered, controller, output, error))
        return false;
    owner->bound = true;
    return true;
}

AbClassicObjectHooks *ab_classic_flames_updates(AbClassicFlames *owner) {
    return owner ? owner->registered : NULL;
}

bool ab_classic_flames_view(const AbClassicFlames *owner, size_t object,
                            AbClassicFlameView *output) {
    if (!owner || !output || owner->failed)
        return false;
    for (size_t index = 0; index < owner->count; ++index) {
        const FlameObject *row = owner->rows + index;
        if (row->object == object) {
            *output = (AbClassicFlameView){&row->state,
                                           &row->configuration.definition,
                                           {(const uint8_t *)row->key, row->key_size},
                                           row->draw.z,
                                           row->applied};
            return true;
        }
    }
    return false;
}
