#ifndef AB_GAME_VIAL_COMMON_H
#define AB_GAME_VIAL_COMMON_H

static AbDataTable *game_vial_physical(PreviewVialObject *row) {
    return &ab_classic_level_view(row->owner->game->playfield.level)
                .properties->objects[row->object]
                .physical;
}

static bool game_vial_write(PreviewVialObject *row, const char *key, AbDataValue value,
                            AbError *error) {
    return ab_classic_characters_set_goal_property(
        ab_classic_game_runtime_view(row->owner->game->game).characters, row->object,
        menu_span(key), value, error);
}

static bool game_vial_live(PreviewVialObject *row) {
    AbClassicLevelView level = ab_classic_level_view(row->owner->game->playfield.level);
    AbDataValue key = {.kind = AB_DATA_STRING,
                       .string = level.bodies->objects[row->object].name};
    const AbDataValue *value = ab_data_get(&level.properties->world, key);
    return value && menu_route_truthy(*value);
}

static bool game_vial_definition(PreviewGame *game, size_t object, AbDataValue *output,
                                 AbError *error) {
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    AbDataValue blocks = menu_route_field(level.catalog, "blocks");
    AbDataValue key =
        menu_route_field(&level.properties->objects[object].physical, "definition");
    const AbDataValue *found =
        blocks.kind == AB_DATA_TABLE ? ab_data_get(blocks.table, key) : NULL;
    if (!found || found->kind != AB_DATA_TABLE)
        return ab_fail(error, "Vial effect requires its raw block definition");
    *output = *found;
    return true;
}

static bool game_vial_find(PreviewVials *owner, AbDataValue physical,
                           PreviewVialObject **output, AbError *error) {
    if (!owner || !output)
        return ab_fail(error, "Missing vial target lookup owner/output");
    *output = NULL;
    AbClassicLevelView level = ab_classic_level_view(owner->game->playfield.level);
    for (size_t index = 0; index < level.bodies->count; ++index)
        if (physical.kind == AB_DATA_TABLE &&
            physical.table == &level.properties->objects[index].physical) {
            *output = owner->objects + index;
            return true;
        }
    return ab_fail(error, "Vial effect lost its canonical target table");
}

static AbClassicObjectHooks *game_vial_hooks(PreviewVials *owner) {
    AbClassicWorldRuntimeView world = ab_classic_world_runtime_view(owner->game->world);
    return ab_classic_flames_updates(
        ab_classic_block_scripts_view(world.scripts).flames);
}

static bool game_vial_remove_function(PreviewVialObject *row,
                                      const AbObjectUpdateFunction *function,
                                      AbError *error) {
    AbObjectUpdates *updates = NULL;
    return ab_classic_object_hooks_updates(game_vial_hooks(row->owner), row->object,
                                           &updates, error) &&
           (!updates || ab_object_update_remove(updates, function, error));
}

static bool game_vial_add_function(PreviewVialObject *row,
                                   const AbObjectUpdateFunction *function,
                                   AbError *error) {
    AbClassicObjectHooks *hooks = game_vial_hooks(row->owner);
    AbObjectUpdates *updates = NULL;
    if (!ab_classic_object_hooks_updates(hooks, row->object, &updates, error))
        return false;
    if (!updates)
        updates = &row->owned_updates;
    AbObjectUpdateList *promotion = NULL;
    if (updates->object.kind == AB_OBJECT_UPDATE_FUNCTION) {
        if (row->promotion_count == 4)
            return ab_fail(error, "Vial update promotion storage exhausted");
        size_t index = row->promotion_count++;
        row->promotions[index] = (AbObjectUpdateList){row->functions[index], 0, 8};
        promotion = row->promotions + index;
    }
    return ab_object_update_add(updates, function, promotion, error) &&
           ab_classic_object_hooks_register(hooks, row->object, updates, error);
}

static bool game_vial_remove_update(PreviewVialObject *row, AbError *error) {
    return game_vial_remove_function(row, &row->update, error);
}

static bool game_vial_add_update(PreviewVialObject *row, AbError *error) {
    return game_vial_add_function(row, &row->update, error);
}

static bool game_vial_visual(PreviewVialObject *row, PreviewVialKind kind,
                             float duration, float speed, bool refresh,
                             AbError *error) {
    if (kind >= GAME_VIAL_EFFECTS)
        return ab_fail(error, "Invalid vial visual kind");
    PreviewVialVisual *visual = row->visuals + kind;
    AbClassicLevelView level = ab_classic_level_view(row->owner->game->playfield.level);
    AbSpan object_name = level.bodies->objects[row->object].name;
    const char *const prefixes[] = {"enlarged_", "cloned_", "floating_"};
    int size = snprintf((char *)visual->name, sizeof(visual->name), "%s%.*s",
                        prefixes[kind], (int)object_name.size, object_name.data);
    if (size < 0 || (size_t)size >= sizeof(visual->name))
        return ab_fail(error, "Vial visual key exceeds stable storage");
    visual->name_size = (size_t)size;
    AbSpan key = {visual->name, visual->name_size};
    uintptr_t value;
    bool present;
    if (!ab_string_map_get(row->owner->effects, key, &value, &present, error))
        return false;
    if (refresh && present) {
        visual->time = visual->duration * .9f;
        visual->started = true;
        return true;
    }
    visual->target = row;
    visual->duration = duration;
    visual->speed = speed;
    visual->kind = kind;
    visual->time = 0;
    visual->started = false;
    return ab_string_map_set(row->owner->effects, key, (uintptr_t)visual, true, error);
}

static bool game_vial_clear_table(AbDataTable *table, AbSpan name, AbError *error) {
    AbObjectPropertyAccess access = {
        .physics_object = {.kind = AB_DATA_TABLE, .table = table}};
    return ab_object_property_set(&access, name, (AbDataValue){0}, error);
}

static bool game_vial_remove_silently(PreviewVialObject *row, AbError *error) {
    if (!game_vial_live(row))
        return true;
    PreviewGame *game = row->owner->game;
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    AbClassicWorldRuntimeView world = ab_classic_world_runtime_view(game->world);
    AbClassicDynamicsView dynamics = ab_classic_dynamics_view(world.dynamics);
    AbSpan name = level.bodies->objects[row->object].name;
    if (!ab_classic_object_removal_call(dynamics.physical, name, error) ||
        !ab_string_map_set(ab_classic_goals_map(dynamics.goals), name, 0, false,
                           error) ||
        !game_vial_clear_table(&level.properties->world, name, error) ||
        !game_vial_clear_table(&level.properties->dynamic, name, error) ||
        !ab_string_map_set(world.dead_blocks, name, 0, false, error))
        return false;
    /* EggDefenderUtils3567 calls the selected level hook, without invoking the
     * definition onRemoved callback or ordinary destruction/scoring phases. */
    AbObjectRemovalLevel *hook = NULL;
    if (!game_removal_level(game, game->level_name, &hook, error))
        return false;
    AbObjectRemovalObject object = {.context = game_vial_physical(row)};
    return !hook || game_removal_call(game, AB_OBJECT_REMOVAL_LEVEL, hook->on_remove,
                                      &object, error);
}

static bool game_vial_remove_gadgets(PreviewVialObject *row, AbError *error) {
    PreviewVials *owner = row->owner;
    PreviewGame *game = owner->game;
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    AbSpan name = level.bodies->objects[row->object].name;
    if (!ab_string_map_reset(owner->jointed, 0, error) ||
        !ab_classic_joint_updates_refresh(game->joint_updates, error))
        return false;
    AbGameTailJoints *joints = ab_classic_joint_updates_tail(game->joint_updates);
    for (size_t index = 0; index < joints->count; ++index) {
        const AbClassicJointRecord *record =
            ab_classic_joints_metadata(level.joints, joints->entries[index].name);
        if (!record)
            return ab_fail(error, "Vial joint lookup lost native metadata");
        AbSpan other;
        if (menu_route_equal((AbDataValue){.kind = AB_DATA_STRING,
                                           .string = record->definition.endpoints[0]},
                             (AbDataValue){.kind = AB_DATA_STRING, .string = name}))
            other = record->definition.endpoints[1];
        else if (menu_route_equal(
                     (AbDataValue){.kind = AB_DATA_STRING,
                                   .string = record->definition.endpoints[1]},
                     (AbDataValue){.kind = AB_DATA_STRING, .string = name}))
            other = record->definition.endpoints[0];
        else
            continue;
        const AbDataValue *physical =
            ab_data_get(&level.properties->world,
                        (AbDataValue){.kind = AB_DATA_STRING, .string = other});
        PreviewVialObject *gadget = NULL;
        if (!physical || physical->kind != AB_DATA_TABLE ||
            !game_vial_find(owner, *physical, &gadget, error) ||
            !ab_string_map_set(owner->jointed, other, (uintptr_t)gadget, true, error))
            return ab_fail(error, "Vial gadget lost its live joint endpoint");
    }
    size_t cursor = 0;
    AbStringMapEntry entry;
    while (ab_string_map_next(owner->jointed, &cursor, &entry, error)) {
        if (!entry.present)
            return true;
        /* Capture the table identity, not a mutable world[name] value slot. */
        PreviewVialObject *gadget = (PreviewVialObject *)entry.value;
        AbDataValue definition;
        if (!game_vial_definition(game, gadget->object, &definition, error))
            return false;
        if (menu_route_equal(menu_route_field(definition.table, "materialName"),
                             game_text("extras")) &&
            !game_vial_remove_silently(gadget, error))
            return false;
    }
    return false;
}

static bool game_vial_sprite(PreviewVialObject *row, AbDataValue mappings,
                             AbDataValue *sprite, float *scale, AbError *error) {
    if (mappings.kind != AB_DATA_TABLE)
        return ab_fail(error, "Missing actual vial sprite mapping");
    AbDataValue definition = menu_route_field(game_vial_physical(row), "definition");
    const AbDataValue *found = ab_data_get(mappings.table, definition);
    AbDataValue selected = found ? *found : (AbDataValue){0};
    if (!menu_route_truthy(selected))
        selected = menu_route_field(mappings.table, "default");
    if (selected.kind != AB_DATA_TABLE)
        return ab_fail(error, "Missing actual vial sprite mapping entry");
    *sprite = menu_route_field(selected.table, "sprite");
    return sprite->kind == AB_DATA_STRING &&
           game_shock_optional(selected.table, "scale", 1, scale, error);
}

static bool game_vial_transition(PreviewVialRequest *request, AbError *error) {
    PreviewVialObject *row = request->target;
    PreviewGame *game = row->owner->game;
    AbDataValue effect = menu_route_field(request->vial_definition.table, "vialEffect");
    AbDataValue sprite;
    float scale;
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    return game_vial_write(row, "ignoreAllDamage", game->enabled, error) &&
           game_vial_remove_gadgets(row, error) &&
           game_joint_defer_object(game, level.bodies->objects[row->object].name,
                                   error) &&
           game_vial_sprite(row, menu_route_field(effect.table, "pigTransitionSprite"),
                            &sprite, &scale, error) &&
           ab_classic_visuals_set_sprite(game->playfield.visuals, row->object,
                                         sprite.string, error) &&
           ab_classic_scene_set_scale(
               ab_classic_render_scene_objects(game->playfield.scene), row->object,
               (AbVec2){scale, scale}, error) &&
           game_vial_write(row, "blinkTimer", menu_route_number(10000), error) &&
           game_vial_write(row, "oinkTimer", menu_route_number(10000), error) &&
           game_vial_write(row, "dontUpdateDamageSprite", game->enabled, error) &&
           game_vial_write(row, "disableSmiling", game->enabled, error);
}

#endif
