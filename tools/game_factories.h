#ifndef AB_GAME_FACTORIES_H
#define AB_GAME_FACTORIES_H

/* MuseumMode2711 counts authored pigs and resets GiftBoxManager4755 before
 * ObjectCreation. The pinned default_conf/game_conf enable this local phase. */
static bool game_factory_prepare(PreviewGame *game, const AbLuaData *level,
                                 const AbLuaData *catalog, AbError *error) {
    PreviewFactory *factory = &game->factory;
    AbDataValue world = menu_route_field(level->globals, "world");
    AbDataValue blocks = menu_route_field(catalog->globals, "blocks");
    if (world.kind != AB_DATA_TABLE || blocks.kind != AB_DATA_TABLE)
        return ab_fail(error, "Missing actual object-factory preparation tables");
    for (size_t index = 0; index < world.table->count; ++index) {
        AbDataValue object = world.table->entries[index].value;
        if (object.kind != AB_DATA_TABLE)
            continue;
        const AbDataValue *definition =
            ab_data_get(blocks.table, menu_route_field(object.table, "definition"));
        if (!definition || definition->kind != AB_DATA_TABLE)
            continue;
        if (menu_route_truthy(menu_route_field(definition->table, "levelGoal")))
            factory->initial_goals += 1;
        if (menu_route_truthy(menu_route_field(definition->table, "isGiftBoxable")))
            factory->giftable += 1;
    }
    factory->gift_selected = 1;
    if (factory->giftable > 1 &&
        ab_script_random(&game->menu->random, 2, 1, (int32_t)factory->giftable,
                         &factory->gift_selected) != AB_SCRIPT_RANDOM_OK)
        return ab_fail(error, "Invalid actual gift-pig randomizer state");
    /* All ordinary constructors increment before the missing-ad-system gate.
     * This is their resulting accumulator, before later dynamic factories. */
    factory->gift_accumulator = factory->initial_goals > 1 ? factory->giftable : 0;
    return true;
}

static bool game_factory_count(PreviewFactory *factory, AbDataValue definition,
                               float *output, AbError *error) {
    const AbDataValue *prior = ab_data_get(&factory->counts, definition);
    float count = 0;
    if (prior && prior->kind != AB_DATA_NIL) {
        if (prior->kind != AB_DATA_NUMBER || !isfinite(menu_real(prior->bits)))
            return ab_fail(error, "Invalid current objects.counts value");
        count = menu_real(prior->bits);
    }
    count += 1;
    if (!isfinite(count))
        return ab_fail(error, "Object creation counter overflow");
    AbObjectPropertyAccess access = {
        .physics_object = {.kind = AB_DATA_TABLE, .table = &factory->counts}};
    if (!ab_object_property_set(&access, definition.string, menu_route_number(count),
                                error))
        return false;
    *output = count;
    return true;
}

static bool game_factory_open(PreviewGame *game, AbError *error) {
    PreviewFactory *factory = &game->factory;
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    AbDataValue counts = menu_route_field(level.data->globals, "counts");
    AbDataValue blocks = menu_route_field(level.catalog, "blocks");
    if (blocks.kind != AB_DATA_TABLE ||
        (counts.kind != AB_DATA_NIL && counts.kind != AB_DATA_TABLE))
        return ab_fail(error, "Missing actual object-factory counters");
    size_t initial = counts.kind == AB_DATA_TABLE ? counts.table->count : 0;
    size_t capacity = blocks.table->count;
    if (initial > SIZE_MAX - capacity ||
        initial + capacity > SIZE_MAX / sizeof(*factory->entries))
        return ab_fail(error, "Object-factory counter capacity overflow");
    factory->capacity = ab_classic_level_runtime_view(game->level).object_capacity -
                        level.bodies->count;
    factory->entries = calloc(initial + capacity, sizeof(*factory->entries));
    if (factory->capacity) {
        factory->names = calloc(factory->capacity, sizeof(*factory->names));
        factory->events = calloc(factory->capacity, sizeof(*factory->events));
        factory->event_fields =
            calloc(factory->capacity, sizeof(*factory->event_fields));
    }
    if (!factory->entries ||
        (factory->capacity &&
         (!factory->names || !factory->events || !factory->event_fields)))
        return ab_fail(error, "Cannot allocate stable object-factory counters/names");
    factory->counts =
        (AbDataTable){factory->entries, initial, initial + capacity, NULL};
    if (initial)
        memcpy(factory->entries, counts.table->entries,
               initial * sizeof(*factory->entries));
    if (counts.kind == AB_DATA_NIL) {
        for (size_t object = 0; object < level.bodies->count; ++object) {
            AbSpan name = level.bodies->objects[object].definition_name;
            AbSpan object_name = level.bodies->objects[object].name;
            if (!name.size ||
                (object_name.size == 6 && !memcmp(object_name.data, "ground", 6)))
                continue;
            float unused;
            if (!game_factory_count(
                    factory, (AbDataValue){.kind = AB_DATA_STRING, .string = name},
                    &unused, error))
                return false;
        }
    }
    return true;
}

static bool game_factory_created(PreviewGame *game, size_t object, AbError *error) {
    PreviewFactory *factory = &game->factory;
    if (factory->events_used == factory->capacity)
        return ab_fail(error, "Object-created notification capacity exhausted");
    size_t row = factory->events_used++;
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    factory->event_fields[row][0] =
        (AbDataEntry){game_text("id"), menu_event(game->menu, "EID_OBJECT_CREATED")};
    factory->event_fields[row][1] = (AbDataEntry){
        game_text("object"),
        {.kind = AB_DATA_TABLE, .table = &level.properties->objects[object].physical}};
    factory->events[row] = (AbDataTable){factory->event_fields[row], 2, 2, NULL};
    return ab_event_manager_notify(game->menu->events, factory->events + row, error);
}

static bool game_factory_goal_circle(PreviewGame *game, AbDataValue definition_name,
                                     AbVec2 position, size_t *output, AbError *error) {
    PreviewFactory *factory = &game->factory;
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    if (definition_name.kind != AB_DATA_STRING || factory->used == factory->capacity)
        return ab_fail(error, "Invalid or exhausted dynamic goal factory");
    AbObjectDefinition definition;
    if (!ab_object_definition_open(level.catalog, definition_name.string, NULL,
                                   &definition, error))
        return false;
    if (menu_route_truthy(
            ab_object_property(&definition, menu_span("isGiftBoxable"))) &&
        factory->initial_goals > 1) {
        factory->gift_accumulator += 1;
        /* GiftBoxManager8794 returns false before settings/probability/RNG when
         * showEditor is false and adSystem is absent. This local application
         * supplies no ad service; no readiness/claim response is fabricated. */
    }
    float count;
    if (!game_factory_count(factory, definition_name, &count, error))
        return false;
    uint8_t counter[32];
    AbSpan suffix;
    if (!ab_ui_value_text(menu_number(count), counter, sizeof(counter), &suffix) ||
        definition_name.string.size >= GAME_FACTORY_NAME_BYTES ||
        suffix.size > GAME_FACTORY_NAME_BYTES - definition_name.string.size - 1)
        return ab_fail(error, "Dynamic goal name exceeds stable storage");
    uint8_t *bytes = factory->names[factory->used];
    memcpy(bytes, definition_name.string.data, definition_name.string.size);
    bytes[definition_name.string.size] = '_';
    memcpy(bytes + definition_name.string.size + 1, suffix.data, suffix.size);
    AbSpan name = {bytes, definition_name.string.size + 1 + suffix.size};
    AbDataValue density = ab_object_property(&definition, menu_span("density"));
    size_t object;
    if (density.kind != AB_DATA_NUMBER ||
        !ab_classic_level_create_defined_goal_circle(
            game->playfield.level, name, definition_name.string, position,
            menu_real(density.bits), &object, error))
        return error && error->message[0]
                   ? false
                   : ab_fail(error, "Dynamic goal has no resolved density");
    ++factory->used;
    AbDataValue sprite = ab_object_property(&definition, menu_span("sprite"));
    if (sprite.kind != AB_DATA_STRING || definition.z_order.kind != AB_DATA_NUMBER ||
        !ab_classic_render_scene_create_object(
            game->playfield.scene, game->playfield.visuals, object, sprite.string,
            menu_real(definition.z_order.bits), error) ||
        !ab_classic_behaviors_create_object(
            game->playfield.behaviors, object,
            ab_object_property(&definition, menu_span("behaviors")), level.catalog_data,
            error) ||
        !game_created_register(game, object, false, error))
        return false;
    *output = object;
    return true;
}

#endif
