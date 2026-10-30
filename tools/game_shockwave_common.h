#ifndef AB_GAME_SHOCKWAVE_COMMON_H
#define AB_GAME_SHOCKWAVE_COMMON_H

static AbDataTable *game_shock_physical(PreviewShockObject *row) {
    AbClassicLevelView level = ab_classic_level_view(row->owner->game->playfield.level);
    return &level.properties->objects[row->object].physical;
}

static bool game_shock_write(PreviewShockObject *row, const char *key,
                             AbDataValue value, AbError *error) {
    return ab_classic_characters_set_goal_property(
        ab_classic_game_runtime_view(row->owner->game->game).characters, row->object,
        menu_span(key), value, error);
}

static bool game_shock_number(PreviewShockObject *row, const char *key, float value,
                              AbError *error) {
    return isfinite(value) &&
           game_shock_write(row, key, menu_route_number(value), error);
}

static bool game_shock_read(const AbDataTable *table, const char *key, float *output,
                            AbError *error) {
    AbDataValue value = menu_route_field(table, key);
    if (value.kind != AB_DATA_NUMBER || !isfinite(menu_real(value.bits))) {
        ab_fail(error, "Missing numeric electrical effect field: %s", key);
        return false;
    }
    *output = menu_real(value.bits);
    return true;
}

static bool game_shock_optional(const AbDataTable *table, const char *key,
                                float fallback, float *output, AbError *error) {
    if (!menu_route_truthy(menu_route_field(table, key))) {
        *output = fallback;
        return true;
    }
    return game_shock_read(table, key, output, error);
}

static bool game_shock_random(PreviewShockwaves *owner, unsigned arguments, int32_t low,
                              int32_t high, float *output, AbError *error) {
    return ab_script_random(&owner->game->menu->random, arguments, low, high, output) ==
               AB_SCRIPT_RANDOM_OK ||
           ab_fail(error, "Invalid shared electrical effect random state");
}

static bool game_shock_definition(PreviewShockObject *row, AbDataValue *output,
                                  AbError *error) {
    PreviewGame *game = row->owner->game;
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    return ab_object_definition_lookup(&level.properties->lookup,
                                       level.bodies->objects[row->object].name, output,
                                       error) &&
           (output->kind == AB_DATA_TABLE ||
            ab_fail(error, "Missing actual electrical effect definition"));
}

static AbClassicObjectHooks *game_shock_hooks(PreviewShockwaves *owner) {
    AbClassicWorldRuntimeView world = ab_classic_world_runtime_view(owner->game->world);
    return ab_classic_flames_updates(
        ab_classic_block_scripts_view(world.scripts).flames);
}

static bool game_shock_add_update(PreviewShockObject *row,
                                  const AbObjectUpdateFunction *function,
                                  AbError *error) {
    AbClassicObjectHooks *hooks = game_shock_hooks(row->owner);
    AbObjectUpdates *updates = NULL;
    if (!ab_classic_object_hooks_updates(hooks, row->object, &updates, error))
        return false;
    if (!updates)
        updates = &row->owned_updates;
    AbObjectUpdateList *promotion = NULL;
    if (updates->object.kind == AB_OBJECT_UPDATE_FUNCTION) {
        if (row->promotion_count == 4)
            return ab_fail(error, "Electrical callback promotion storage exhausted");
        size_t index = row->promotion_count++;
        row->promotions[index] = (AbObjectUpdateList){row->functions[index], 0, 8};
        promotion = row->promotions + index;
    }
    return ab_object_update_add(updates, function, promotion, error) &&
           ab_classic_object_hooks_register(hooks, row->object, updates, error);
}

static bool game_shock_remove_update(PreviewShockObject *row,
                                     const AbObjectUpdateFunction *function,
                                     AbError *error) {
    AbObjectUpdates *updates = NULL;
    return ab_classic_object_hooks_updates(game_shock_hooks(row->owner), row->object,
                                           &updates, error) &&
           (updates ? ab_object_update_remove(updates, function, error)
                    : ab_fail(error, "Electrical callback lost its property"));
}

static bool game_shock_dead(PreviewShockObject *row, AbError *error) {
    PreviewGame *game = row->owner->game;
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    AbClassicWorldRuntimeView world = ab_classic_world_runtime_view(game->world);
    return ab_string_map_set(world.dead_blocks, level.bodies->objects[row->object].name,
                             (uintptr_t)(world.physical_values + row->object), true,
                             error);
}

static bool game_shock_sprite(PreviewShockwaves *owner, AbElectricSprite sprite,
                              AbGameSpritePose pose, AbExtraDrawInput input,
                              AbError *error) {
    AbDrawCommand command;
    bool visible;
    return ab_game_sprite_command(owner->sprites + sprite, pose, input.camera,
                                  input.width, input.height, &command, &visible,
                                  error) &&
           (!visible || ab_draw_list_push(input.list, &command, error));
}

static bool game_shock_audio(PreviewShockwaves *owner, AbDataValue name,
                             AbError *error) {
    PreviewGame *game = owner->game;
    AbDataValue resolved;
    uint32_t voice;
    return ab_audio_bank_name_value(game->menu->audio, name, &resolved, error) &&
           ab_audio_bank_play(game->menu->audio,
                              (AbAudioPlay){.name = resolved,
                                            .volume = menu_route_number(1),
                                            .loop = game->disabled},
                              &voice, error);
}

#endif
