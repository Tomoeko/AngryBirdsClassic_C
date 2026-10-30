#ifndef AB_GAME_ROCKETS_H
#define AB_GAME_ROCKETS_H

typedef struct {
    PreviewRocketCanisters *owner;
    size_t object;
    AbDataTable info;
    AbDataEntry fields[7];
} PreviewRocketCanister;

struct PreviewRocketCanisters {
    PreviewGame *game;
    PreviewRocketCanister *objects;
    AbStringMap *active;
    AbClassicGameUpdateFunction update;
    size_t capacity;
};

static AbDataTable *game_rocket_physical(PreviewRocketCanister *row) {
    return &ab_classic_level_view(row->owner->game->playfield.level)
                .properties->objects[row->object]
                .physical;
}

static bool game_rocket_definition(PreviewGame *game, size_t object,
                                   AbDataValue *output, AbError *error) {
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    AbDataValue blocks = menu_route_field(level.catalog, "blocks");
    AbDataValue name =
        menu_route_field(&level.properties->objects[object].physical, "definition");
    const AbDataValue *definition =
        blocks.kind == AB_DATA_TABLE ? ab_data_get(blocks.table, name) : NULL;
    if (!definition || definition->kind != AB_DATA_TABLE)
        return ab_fail(error, "Missing actual rocket-canister definition");
    *output = *definition;
    return true;
}

static bool game_rocket_particles(PreviewRocketCanister *row, AbDataTable *info,
                                  float seconds, float angle, AbError *error) {
    float buffer = 0, interval = 0;
    if (!game_shock_read(info, "particleTimeBuffer", &buffer, error) ||
        !game_shock_read(info, "particleSpawnInterval", &interval, error) ||
        interval <= 0)
        return ab_fail(error, "Invalid actual rocket particle interval");
    buffer += seconds;
    AbObjectPropertyAccess access = {
        .physics_object = {.kind = AB_DATA_TABLE, .table = info}};
    if (!ab_object_property_set(&access, menu_span("particleTimeBuffer"),
                                menu_route_number(buffer), error))
        return false;
    AbDataValue offset = menu_route_field(info, "particleSpawnOffset");
    AbVec2 point;
    if (offset.kind != AB_DATA_TABLE ||
        !game_shock_read(offset.table, "x", &point.x, error) ||
        !game_shock_read(offset.table, "y", &point.y, error))
        return ab_fail(error, "Missing actual rocket particle offset");
    /* utils.rotatePointAroundOrigin11901 uses rounded Lua cos/sin, then
     * separate binary32 products/subtraction and products/addition. */
    float cosine = (float)cos((double)angle), sine = (float)sin((double)angle);
    AbVec2 rotated = {point.x * cosine - point.y * sine,
                      point.x * sine + point.y * cosine};
    if (!(buffer > interval))
        return true;
    float amount = (float)floor((double)(buffer / interval));
    buffer -= amount * interval;
    if (!ab_object_property_set(&access, menu_span("particleTimeBuffer"),
                                menu_route_number(buffer), error) ||
        !game_shock_read(game_rocket_physical(row), "x", &point.x, error) ||
        !game_shock_read(game_rocket_physical(row), "y", &point.y, error))
        return false;
    point.x = (point.x + rotated.x) * 20;
    point.y = (point.y + rotated.y) * 20;
    AbDataValue particles = menu_route_field(info, "particles");
    if (particles.kind != AB_DATA_STRING)
        return ab_fail(error, "Invalid actual rocket particle name");
    return ab_particles_emit(ab_classic_particles_engine(row->owner->game->particles),
                             &(AbParticleEmission){.definition = particles.string,
                                                   .amount = amount,
                                                   .position = point,
                                                   .size = {1, 1}},
                             error);
}

static bool game_rocket_update(void *context, float seconds, float time,
                               AbError *error) {
    (void)time;
    PreviewRocketCanisters *owner = context;
    PreviewGame *game = owner->game;
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    AbClassicWorldRuntimeView world = ab_classic_world_runtime_view(game->world);
    AbClassicSimulationView simulation =
        ab_classic_simulation_view(ab_classic_dynamics_view(world.dynamics).simulation);
    size_t cursor = 0;
    AbStringMapEntry entry;
    while (ab_string_map_next(owner->active, &cursor, &entry, error)) {
        if (!entry.present)
            return true;
        PreviewRocketCanister *row = (PreviewRocketCanister *)entry.value;
        AbDataTable *physical = game_rocket_physical(row);
        AbDataValue info = menu_route_field(physical, "rocketInfo");
        const AbDataValue *member =
            ab_data_get(&level.properties->world,
                        (AbDataValue){.kind = AB_DATA_STRING, .string = entry.key});
        float timer = 0, duration = 0;
        bool retire =
            !member || member->kind == AB_DATA_NIL || info.kind == AB_DATA_NIL;
        if (!retire) {
            if (info.kind != AB_DATA_TABLE ||
                !game_shock_read(info.table, "timer", &timer, error) ||
                !game_shock_read(info.table, "propelDuration", &duration, error))
                return ab_fail(error, "Invalid live rocket-canister timer");
            retire = timer > duration;
        }
        if (retire) {
            if (!ab_string_map_set(owner->active, entry.key, 0, false, error) ||
                !ab_string_map_set(world.dead_blocks, entry.key,
                                   (uintptr_t)(world.physical_values + row->object),
                                   true, error))
                return false;
            continue;
        }
        AbObjectPropertyAccess access = {.physics_object = info};
        float angle = 0, mass = 0, force = 0;
        AbVec2 point;
        if (!ab_object_property_set(&access, menu_span("timer"),
                                    menu_route_number(timer + seconds), error) ||
            !game_shock_read(physical, "angle", &angle, error))
            return false;
        float direction = angle + 0x1.921fb6p+1f * .5f;
        float cosine = (float)cos((double)direction),
              sine = (float)sin((double)direction);
        if (!game_rocket_particles(row, info.table, seconds, angle, error) ||
            !game_shock_read(info.table, "propelForce", &force, error) ||
            !game_shock_read(physical, "mass", &mass, error) ||
            !game_shock_read(physical, "x", &point.x, error) ||
            !game_shock_read(physical, "y", &point.y, error))
            return false;
        float magnitude = force * mass;
        if (!ab_force_queue_append(
                simulation.queue,
                (AbQueuedForce){
                    entry.key, {magnitude * cosine, magnitude * sine}, point},
                error))
            return false;
    }
    return false;
}

static bool game_rocket_start(PreviewRocketCanister *row, AbError *error) {
    PreviewGame *game = row->owner->game;
    AbDataTable *physical = game_rocket_physical(row);
    if (menu_route_field(physical, "rocketInfo").kind != AB_DATA_NIL)
        return true;
    AbDataValue definition;
    if (!game_rocket_definition(game, row->object, &definition, error))
        return false;
    AbDataValue effect = menu_route_field(definition.table, "rocketEffect");
    if (effect.kind != AB_DATA_TABLE)
        return ab_fail(error, "Missing actual rocket-canister effect");
    const char *const keys[] = {
        "timer",     "particleTimeBuffer",    "propelDuration",     "propelForce",
        "particles", "particleSpawnInterval", "particleSpawnOffset"};
    for (size_t index = 0; index < 7; ++index)
        row->fields[index] =
            (AbDataEntry){menu_data(keys[index]),
                          index < 2 ? menu_route_number(0)
                                    : menu_route_field(effect.table, keys[index])};
    row->info = (AbDataTable){row->fields, 7, 7, NULL};
    AbObjectPropertyAccess access = {
        .physics_object = {.kind = AB_DATA_TABLE, .table = physical}};
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    return ab_object_property_set(
               &access, menu_span("rocketInfo"),
               (AbDataValue){.kind = AB_DATA_TABLE, .table = &row->info}, error) &&
           ab_string_map_set(row->owner->active,
                             level.bodies->objects[row->object].name, (uintptr_t)row,
                             true, error) &&
           ab_classic_game_updates_add(game->updates,
                                       menu_span("updateRocketCanisters"),
                                       &row->owner->update, error);
}

static bool game_rocket_trigger(PreviewGame *game, size_t object, float force,
                                AbError *error) {
    if (!game->rockets || object >= game->rockets->capacity)
        return ab_fail(error, "Unbound rocket collision owner/identity");
    AbDataValue definition = menu_route_field(
        game_rocket_physical(game->rockets->objects + object), "definition");
    return !menu_route_equal(definition, menu_data("ROCKET_CANISTER")) ||
           !(force >= 10) || game_rocket_start(game->rockets->objects + object, error);
}

static bool game_rocket_block_collision(void *context, size_t first, size_t second,
                                        const AbBlockCollisionNotice *notice,
                                        AbDataValue *result, AbError *error) {
    PreviewGame *game = context;
    *result = (AbDataValue){0};
    return game_rocket_trigger(game, first, notice->force, error) &&
           game_rocket_trigger(game, second, notice->force, error);
}

static bool game_rocket_bird_collision(void *context, size_t first, size_t second,
                                       const AbBlockCollisionNotice *notice,
                                       AbError *error) {
    PreviewGame *game = context;
    if (!game->rockets || first >= game->rockets->capacity)
        return ab_fail(error, "Unbound rocket bird-collision owner");
    bool shot = menu_route_truthy(
        menu_route_field(game_rocket_physical(game->rockets->objects + first), "shot"));
    return game_rocket_trigger(game, shot ? second : first, notice->force, error);
}

static bool game_rocket_removed(PreviewGame *game, AbDataTable *physical,
                                AbError *error) {
    AbDataValue name = menu_route_field(physical, "name");
    return game->rockets && name.kind == AB_DATA_STRING
               ? ab_string_map_set(game->rockets->active, name.string, 0, false, error)
               : ab_fail(error, "Unbound removed rocket-canister registry");
}

static bool game_rocket_particle_names(PreviewGame *game, AbClassicParticleNames *names,
                                       AbError *error) {
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    for (size_t object = 0; object < level.bodies->count; ++object) {
        AbDataValue definition;
        if (!game_rocket_definition(game, object, &definition, error))
            return false;
        AbDataValue effect = menu_route_field(definition.table, "rocketEffect");
        if (effect.kind == AB_DATA_NIL)
            continue;
        if (effect.kind != AB_DATA_TABLE)
            return ab_fail(error, "Invalid actual rocket effect descriptor");
        AbDataValue name = menu_route_field(effect.table, "particles");
        if (name.kind != AB_DATA_STRING)
            return ab_fail(error, "Missing actual rocket particle dependency");
        bool found = false;
        for (size_t index = 0; index < names->count; ++index)
            found |=
                menu_route_equal(name, (AbDataValue){.kind = AB_DATA_STRING,
                                                     .string = names->names[index]});
        if (!found) {
            if (names->count == names->capacity)
                return ab_fail(error, "Rocket particle dependency capacity exhausted");
            names->names[names->count++] = name.string;
        }
    }
    return true;
}

static void game_rocket_close(PreviewGame *game) {
    if (!game->rockets)
        return;
    ab_string_map_close(game->rockets->active);
    free(game->rockets->objects);
    free(game->rockets);
    game->rockets = NULL;
}

static bool game_rocket_open(PreviewGame *game, AbError *error) {
    size_t capacity = ab_classic_level_runtime_view(game->level).object_capacity;
    if (!capacity || capacity > (2 * 1024 * 1024) / sizeof(PreviewRocketCanister))
        return ab_fail(error, "Rocket owner exceeds retained byte limit");
    PreviewRocketCanisters *owner = calloc(1, sizeof(*owner));
    if (!owner)
        return ab_fail(error, "Cannot allocate rocket-canister owner");
    game->rockets = owner;
    owner->game = game;
    owner->capacity = capacity;
    owner->objects = calloc(capacity, sizeof(*owner->objects));
    size_t nodes = 1;
    while (nodes < capacity)
        nodes *= 2;
    if (!owner->objects ||
        !ab_string_map_open(0, nodes, 1024 * 1024, &owner->active, error))
        return ab_fail(error, "Cannot allocate bounded rocket registry");
    if (sizeof(*owner) + capacity * sizeof(*owner->objects) +
            ab_string_map_bytes(owner->active) >
        2 * 1024 * 1024)
        return ab_fail(error, "Combined rocket owner exceeds retained byte limit");
    for (size_t object = 0; object < capacity; ++object)
        owner->objects[object] =
            (PreviewRocketCanister){.owner = owner, .object = object};
    owner->update = (AbClassicGameUpdateFunction){owner, game_rocket_update};
    return true;
}

#endif
