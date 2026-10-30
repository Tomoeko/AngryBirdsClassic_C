#ifndef AB_GAME_SPAWNS_H
#define AB_GAME_SPAWNS_H

static bool game_spawn_descendants(const AbDataTable *catalog, AbSpan name,
                                   AbSpan *path, size_t depth, size_t path_capacity,
                                   size_t limit, size_t *output, AbError *error);

static bool game_spawn_branch(const AbDataTable *catalog, AbDataValue spawn, bool death,
                              AbSpan *path, size_t depth, size_t path_capacity,
                              size_t limit, size_t *output, AbError *error) {
    *output = 0;
    if (spawn.kind != AB_DATA_TABLE)
        return ab_fail(error, "Destruction spawn requires its actual table");
    AbDataValue value = menu_route_field(spawn.table, "amount");
    float amount = 0;
    if (value.kind != AB_DATA_NUMBER)
        return ab_fail(error, "Destruction spawn requires its numeric amount");
    memcpy(&amount, &value.bits, sizeof(amount));
    if (!isfinite(amount) || amount > 16777215)
        return ab_fail(error, "Invalid destruction spawn amount");
    if (amount < 1)
        return true;
    size_t count = (size_t)amount;
    if (count > limit)
        return ab_fail(error, "Destruction spawns exceed reserved level capacity");
    AbDataValue types = menu_route_field(spawn.table, "types");
    if (death && (types.kind != AB_DATA_TABLE || !types.table->count))
        return ab_fail(error, "Death spawn requires its actual type choices");
    size_t maximum = 0;
    size_t choices = death ? types.table->count : 1;
    for (size_t index = 0; index < choices; ++index) {
        AbDataValue target = death ? types.table->entries[index].value
                                   : menu_route_field(spawn.table, "definition");
        if (death && types.table->entries[index].key.kind != AB_DATA_NUMBER)
            continue;
        size_t children = 0;
        if (target.kind != AB_DATA_STRING ||
            !game_spawn_descendants(catalog, target.string, path, depth, path_capacity,
                                    limit / count, &children, error))
            return error && error->message[0]
                       ? false
                       : ab_fail(error, "Destruction spawn type is absent");
        if (children >= limit / count)
            return ab_fail(error, "Nested spawns exceed reserved level capacity");
        if (maximum < children + 1)
            maximum = children + 1;
    }
    *output = maximum * count;
    return true;
}

static bool game_spawn_descendants(const AbDataTable *catalog, AbSpan name,
                                   AbSpan *path, size_t depth, size_t path_capacity,
                                   size_t limit, size_t *output, AbError *error) {
    if (depth >= path_capacity)
        return ab_fail(error, "Destruction spawn definition graph exceeds its catalog");
    for (size_t index = 0; index < depth; ++index)
        if (path[index].size == name.size &&
            !memcmp(path[index].data, name.data, name.size))
            return ab_fail(error, "Cyclic destruction spawn definition graph");
    path[depth] = name;
    AbObjectDefinition definition;
    if (!ab_object_definition_open(catalog, name, NULL, &definition, error))
        return false;
    AbDataValue grid = ab_object_property(&definition, menu_span("spawn"));
    AbDataValue death = ab_object_property(&definition, menu_span("deathSpawn"));
    size_t grid_count = 0, death_count = 0;
    if ((grid.kind != AB_DATA_NIL &&
         !game_spawn_branch(catalog, grid, false, path, depth + 1, path_capacity, limit,
                            &grid_count, error)) ||
        (menu_route_truthy(death) &&
         !game_spawn_branch(catalog, death, true, path, depth + 1, path_capacity,
                            limit - grid_count, &death_count, error)))
        return false;
    *output = grid_count + death_count;
    return true;
}

static bool game_spawn_reserve(void *context, AbClassicLevelView level,
                               size_t *additional, AbError *error) {
    PreviewGame *game = context;
    AbDataValue blocks = menu_route_field(level.catalog, "blocks");
    if (blocks.kind != AB_DATA_TABLE || !blocks.table->count ||
        blocks.table->count > SIZE_MAX / sizeof(AbSpan))
        return ab_fail(error, "Missing bounded spawn definition catalog");
    AbSpan *path = calloc(blocks.table->count, sizeof(*path));
    if (!path)
        return ab_fail(error, "Cannot allocate spawn reservation workspace");
    size_t count = 0, blue = 0, pink = 0;
    size_t available = level.bodies->capacity - level.bodies->count;
    bool okay = true;
    for (size_t object = 0; okay && object < level.bodies->count; ++object) {
        AbSpan definition = level.bodies->objects[object].definition_name;
        if (!definition.size)
            continue;
        AbObjectDefinition descriptor;
        if (!ab_object_definition_open(level.catalog, definition, NULL, &descriptor,
                                       error)) {
            okay = false;
            break;
        }
        AbDataValue removed = ab_object_property(&descriptor, menu_span("onRemoved"));
        blue += menu_route_equal(removed, game_text("blackBirdBlueVialRemoved"));
        pink += menu_route_equal(removed, game_text("blackBirdPinkVialRemoved"));
        size_t descendants = 0;
        okay = game_spawn_descendants(level.catalog, definition, path, 0,
                                      blocks.table->count, available - count,
                                      &descendants, error);
        if (okay)
            count += descendants;
    }
    free(path);
    if (okay) {
        game->spawns.capacity = count;
        size_t bird_slots = (game_birthday(game) ? 4u : 3u) * level.bird_count + 2;
        if (bird_slots > available || count > available - bird_slots)
            return ab_fail(error,
                           "Destruction/bird reservations exceed level capacity");
        size_t budget = available - bird_slots - count;
        size_t added = 0, live = level.goal_count;
        for (size_t vial = 0; vial < blue; ++vial) {
            if (live > (budget - added) / 2) {
                added = budget;
                break;
            }
            added += live * 2;
            live *= 3;
        }
        /* Pink's affectGoals traverses every nearby goal, despite its unused
         * affectOnlyOne metadata. Each can create one floating-pig body. */
        if (pink && live > (budget - added) / pink)
            added = budget;
        else
            added += pink * live;
        /* Tripling can exceed the explicit native-body limit. Reserve that
         * remaining limit; actual creation still rejects on exhaustion. */
        *additional = count + added;
    }
    return okay;
}

static bool game_spawn_random(void *context, unsigned count, float lower, float upper,
                              AbDataValue *output, AbError *error) {
    PreviewGame *game = context;
    float value;
    if (!isfinite(lower) || !isfinite(upper) || (double)lower < INT32_MIN ||
        (double)lower > INT32_MAX || (double)upper < INT32_MIN ||
        (double)upper > INT32_MAX || lower != truncf(lower) || upper != truncf(upper))
        return ab_fail(error, "Spawn random bounds exceed the integer domain");
    if (ab_script_random(&game->menu->random, count, (int32_t)lower, (int32_t)upper,
                         &value) != AB_SCRIPT_RANDOM_OK)
        return ab_fail(error, "Invalid actual spawn random call");
    *output = menu_route_number(value);
    return true;
}

static bool game_spawn_create(void *context, AbDataValue block_table,
                              AbDataValue definition_name, AbSpan name, float x,
                              float y, AbDataValue *output, AbError *error) {
    PreviewGame *game = context;
    PreviewSpawns *spawns = &game->spawns;
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    if (spawns->used >= spawns->capacity || name.size > GAME_SPAWN_NAME_BYTES ||
        block_table.kind != AB_DATA_TABLE || block_table.table != level.catalog ||
        definition_name.kind != AB_DATA_STRING)
        return ab_fail(error, "Invalid or exhausted destruction spawn factory");
    uint8_t *retained = spawns->names[spawns->used];
    memcpy(retained, name.data, name.size);
    name.data = retained;
    AbObjectDefinition definition;
    if (!ab_object_definition_open(level.catalog, definition_name.string, NULL,
                                   &definition, error))
        return false;
    AbDataValue type = ab_object_property(&definition, menu_span("type"));
    AbDataValue density = ab_object_property(&definition, menu_span("density"));
    bool box = menu_route_equal(type, menu_data("box"));
    if ((!box && !menu_route_equal(type, menu_data("circle"))) ||
        density.kind != AB_DATA_NUMBER)
        return ab_fail(error, "Spawn requires its recovered shape/density factory");
    size_t object;
    bool created = box ? ab_classic_level_create_defined_box(
                             game->playfield.level, name, definition_name.string,
                             (AbVec2){x, y}, menu_real(density.bits), &object, error)
                       : ab_classic_level_create_defined_circle(
                             game->playfield.level, name, definition_name.string,
                             (AbVec2){x, y}, menu_real(density.bits), &object, error);
    if (!created)
        return false;
    ++spawns->used;
    AbDataValue sprite = level.initial_sprites[object].sprite;
    if (sprite.kind != AB_DATA_STRING || definition.z_order.kind != AB_DATA_NUMBER ||
        !ab_classic_render_scene_create_object(
            game->playfield.scene, game->playfield.visuals, object, sprite.string,
            menu_real(definition.z_order.bits), error) ||
        !ab_classic_behaviors_create_object(
            game->playfield.behaviors, object,
            ab_object_property(&definition, menu_span("behaviors")), level.catalog_data,
            error) ||
        !game_created_register(game, object, box, error))
        return false;
    *output = (AbDataValue){.kind = AB_DATA_TABLE,
                            .table = &level.properties->objects[object].physical};
    return true;
}

static bool game_spawn_rotate(void *context, AbDataValue object, float angle,
                              AbError *error) {
    PreviewGame *game = context;
    if (object.kind != AB_DATA_TABLE)
        return ab_fail(error, "Spawn rotation lost its actual physical table");
    AbDataValue name = menu_route_field(object.table, "name");
    size_t index;
    return name.kind == AB_DATA_STRING &&
           game_hat_index(game, name.string, &index, error) &&
           ab_classic_level_set_rotation(game->playfield.level, index, angle, error);
}

static bool game_spawn_open(PreviewGame *game, AbError *error) {
    PreviewSpawns *spawns = &game->spawns;
    if (spawns->capacity > SIZE_MAX / sizeof(*spawns->names))
        return ab_fail(error, "Destruction spawn name allocation overflow");
    if (spawns->capacity) {
        spawns->names = calloc(spawns->capacity, sizeof(*spawns->names));
        if (!spawns->names)
            return ab_fail(error, "Cannot allocate retained destruction spawn names");
    }
    spawns->host = (AbBlockRemovalSpawnHost){
        .context = game,
        .block_table =
            {.kind = AB_DATA_TABLE,
             .table =
                 (AbDataTable *)ab_classic_level_view(game->playfield.level).catalog},
        .scale_factor = menu_route_number(
            ab_classic_level_runtime_view(game->level).placement_scale),
        .math_pi = 0x1.921fb6p+1f,
        .name_storage = spawns->scratch,
        .name_capacity = sizeof(spawns->scratch),
        .maximum_instances = spawns->capacity,
        .random = game_spawn_random,
        .create = game_spawn_create,
        .rotate = game_spawn_rotate};
    return true;
}

#endif
