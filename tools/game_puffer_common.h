#ifndef AB_GAME_PUFFER_COMMON_H
#define AB_GAME_PUFFER_COMMON_H

static AbDataTable *game_puffer_physical(PreviewPufferObject *row) {
    AbClassicLevelView level = ab_classic_level_view(row->owner->game->playfield.level);
    return &level.properties->objects[row->object].physical;
}

static bool game_puffer_read(const AbDataTable *table, const char *key, float *value,
                             AbError *error) {
    AbDataValue field = menu_route_field(table, key);
    if (field.kind != AB_DATA_NUMBER || !isfinite(menu_real(field.bits)))
        return ab_fail(error, "Missing numeric puffer field: %s", key);
    *value = menu_real(field.bits);
    return true;
}

static bool game_puffer_write(PreviewPufferObject *row, const char *key,
                              AbDataValue value, AbError *error) {
    AbObjectPropertyAccess access = {
        .physics_object = {.kind = AB_DATA_TABLE, .table = game_puffer_physical(row)}};
    return ab_object_property_set(&access, menu_span(key), value, error);
}

static bool game_puffer_number(PreviewPufferObject *row, const char *key, float value,
                               AbError *error) {
    return (isfinite(value) ||
            ab_fail(error, "Puffer arithmetic exceeds finite range")) &&
           game_puffer_write(row, key, menu_route_number(value), error);
}

static bool game_puffer_definition(PreviewPufferObject *row, AbDataValue *output,
                                   bool override, AbError *error) {
    AbDataValue selected =
        menu_route_field(game_puffer_physical(row), "pufferDefinition");
    if (override && menu_route_truthy(selected)) {
        if (selected.kind != AB_DATA_TABLE)
            return ab_fail(error, "Invalid actual puffer override definition");
        *output = selected;
        return true;
    }
    PreviewGame *game = row->owner->game;
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    return ab_object_definition_lookup(&level.properties->lookup,
                                       level.bodies->objects[row->object].name, output,
                                       error) &&
           (output->kind == AB_DATA_TABLE ||
            ab_fail(error, "Missing actual puffer definition"));
}

static AbClassicObjectHooks *game_puffer_hooks(PreviewPuffers *owner) {
    AbClassicWorldRuntimeView world = ab_classic_world_runtime_view(owner->game->world);
    return ab_classic_flames_updates(
        ab_classic_block_scripts_view(world.scripts).flames);
}

static bool game_puffer_has(PreviewPufferObject *row, PreviewPufferPhase phase,
                            bool *present, AbError *error) {
    AbObjectUpdates *updates = NULL;
    if (!ab_classic_object_hooks_updates(game_puffer_hooks(row->owner), row->object,
                                         &updates, error))
        return false;
    if (!updates) {
        *present = false;
        return true;
    }
    return ab_object_update_has(updates, row->callbacks + phase, present, error);
}

static bool game_puffer_add(PreviewPufferObject *row, PreviewPufferPhase phase,
                            AbError *error) {
    AbClassicObjectHooks *hooks = game_puffer_hooks(row->owner);
    AbObjectUpdates *updates = NULL;
    if (!ab_classic_object_hooks_updates(hooks, row->object, &updates, error))
        return false;
    if (!updates)
        updates = &row->owned_updates;
    AbObjectUpdateList *promotion = NULL;
    if (updates->object.kind == AB_OBJECT_UPDATE_FUNCTION) {
        if (row->promotion_count == 4)
            return ab_fail(error, "Puffer callback promotion storage exhausted");
        size_t index = row->promotion_count++;
        row->promotions[index] = (AbObjectUpdateList){row->functions[index], 0, 8};
        promotion = row->promotions + index;
    }
    return ab_object_update_add(updates, row->callbacks + phase, promotion, error) &&
           ab_classic_object_hooks_register(hooks, row->object, updates, error);
}

static bool game_puffer_remove(PreviewPufferObject *row, PreviewPufferPhase phase,
                               AbError *error) {
    AbObjectUpdates *updates = NULL;
    return ab_classic_object_hooks_updates(game_puffer_hooks(row->owner), row->object,
                                           &updates, error) &&
           (updates ? ab_object_update_remove(updates, row->callbacks + phase, error)
                    : ab_fail(error, "Puffer callback lost its physical property"));
}

static bool game_puffer_audio(PreviewPufferObject *row, AbDataValue name,
                              AbError *error) {
    PreviewGame *game = row->owner->game;
    AbDataValue selected;
    uint32_t voice;
    return ab_audio_bank_name_value(game->menu->audio, name, &selected, error) &&
           ab_audio_bank_play(game->menu->audio,
                              (AbAudioPlay){.name = selected,
                                            .volume = menu_route_number(1),
                                            .loop = game->disabled},
                              &voice, error);
}

static bool game_puffer_resize(PreviewPufferObject *row, float radius, AbError *error) {
    PreviewGame *game = row->owner->game;
    AbClassicWorldRuntimeView world = ab_classic_world_runtime_view(game->world);
    return ab_classic_level_resize_circle(game->playfield.level, row->object, radius,
                                          error) &&
           ab_classic_simulation_refresh_circle_radius(
               ab_classic_dynamics_view(world.dynamics).simulation, row->object, error);
}

static bool game_puffer_strength(PreviewPufferObject *row, AbError *error) {
    AbClassicLevelView level = ab_classic_level_view(row->owner->game->playfield.level);
    if (!game_puffer_number(row, "strength", 100000, error))
        return false;
    level.character_objects[row->object].goal.strength = 100000;
    return true;
}

#endif
