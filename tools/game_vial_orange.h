#ifndef AB_GAME_VIAL_ORANGE_H
#define AB_GAME_VIAL_ORANGE_H

static bool game_vial_transition_later(void *context, AbDataValue parameters,
                                       AbError *error) {
    (void)parameters;
    PreviewVialRequest *request = context;
    if (!game_vial_live(request->target))
        return true;
    AbDataValue effect = menu_route_field(request->vial_definition.table, "vialEffect");
    AbDataValue sprite;
    float scale;
    return game_vial_transition(request, error) &&
           game_vial_sprite(request->target,
                            menu_route_field(effect.table, "pigTransitionSprite"),
                            &sprite, &scale, error) &&
           game_vial_write(request->target, "damageSprite", sprite, error);
}

static bool game_vial_inflate_later(void *context, AbDataValue parameters,
                                    AbError *error) {
    (void)parameters;
    PreviewVialRequest *request = context;
    PreviewVialObject *row = request->target;
    PreviewGame *game = row->owner->game;
    if (game_vial_live(row)) {
        AbDataValue effect =
            menu_route_field(request->vial_definition.table, "vialEffect");
        AbDataValue sprite, definition;
        float unused_scale, radius;
        if (!game_vial_sprite(row, menu_route_field(effect.table, "pigPuffedSprite"),
                              &sprite, &unused_scale, error) ||
            !game_vial_definition(game, row->object, &definition, error) ||
            !game_shock_read(definition.table, "radius", &radius, error) ||
            !ab_classic_scene_set_scale(
                ab_classic_render_scene_objects(game->playfield.scene), row->object,
                (AbVec2){1, 1}, error))
            return false;
        const char *const puffed_names[] = {"radius", "friction", "restitution",
                                            "density", "damageFactors"};
        const AbDataValue puffed_values[] = {
            menu_route_number(7.5f), menu_route_number(.5f), menu_route_number(0),
            menu_route_number(.1f), game_text("GlobeBirdBigDamageFactors")};
        for (size_t index = 0; index < 5; ++index)
            row->puffed_fields[index] =
                (AbDataEntry){game_text(puffed_names[index]), puffed_values[index]};
        row->puffed = (AbDataTable){row->puffed_fields, 5, 5, NULL};
        const char *const names[] = {"puffedValues",
                                     "spriteSpecial",
                                     "puffTime",
                                     "deathTime",
                                     "automaticPuffDelay",
                                     "directionChangeTime",
                                     "directionChangeMagnitude",
                                     "specialSound",
                                     "removeSound",
                                     "radius"};
        const AbDataValue values[] = {{.kind = AB_DATA_TABLE, .table = &row->puffed},
                                      sprite,
                                      menu_route_number(1.5f),
                                      menu_route_number(1.5f),
                                      menu_route_number(1.5f),
                                      menu_route_number(.05f),
                                      menu_route_number(50),
                                      game_text("pigs_potion_inflate_01"),
                                      game_text("pigs_potion_deflate_01"),
                                      menu_route_number(radius)};
        for (size_t index = 0; index < 10; ++index)
            row->puffer_fields[index] =
                (AbDataEntry){game_text(names[index]), values[index]};
        row->puffer = (AbDataTable){row->puffer_fields, 10, 10, NULL};
        if (!game->puffers || row->object >= game->puffers->capacity ||
            !game_vial_write(
                row, "pufferDefinition",
                (AbDataValue){.kind = AB_DATA_TABLE, .table = &row->puffer}, error) ||
            !game_vial_write(row, "smileSprite", sprite, error) ||
            !game_vial_write(row, "frozenSprite", (AbDataValue){0}, error) ||
            !game_vial_write(row, "blinkSprite", (AbDataValue){0}, error) ||
            !game_vial_write(row, "disableSpecialExpressions", game->enabled, error) ||
            !game_vial_write(row, "specialExpression", (AbDataValue){0}, error) ||
            !game_vial_write(row, "specialExpressionTimer", (AbDataValue){0}, error) ||
            !game_puffer_trigger(game->puffers->objects + row->object, error) ||
            !game_vial_write(row, "spriteAffectedByVial", game_text("orange"), error) ||
            !game_vial_write(row, "vialEffectQueue", (AbDataValue){0}, error) ||
            !game_vial_remove_update(row, error))
            return false;
    }
    return game_vial_write(row, "vialEffectInProgress", game->disabled, error);
}

static bool game_vial_orange(PreviewVialRequest *request, bool *retain,
                             AbError *error) {
    PreviewVialObject *row = request->target;
    PreviewGame *game = row->owner->game;
    AbDataTable *physical = game_vial_physical(row);
    if (!game_vial_write(row, "vialEffectInProgress", game->enabled, error))
        return false;
    if (menu_route_truthy(menu_route_field(physical, "levelGoal"))) {
        if (menu_route_truthy(menu_route_field(physical, "isPuffing"))) {
            *retain = true;
            return true;
        }
        if (!game->shockwaves || row->object >= game->shockwaves->capacity ||
            !game_shock_make_invulnerable(game->shockwaves->objects + row->object, 1000,
                                          error) ||
            !ab_delayed_callbacks_add(game->delayed, .7f, &request->transition,
                                      game->absent, error))
            return false;
    }
    return game_vial_visual(row, GAME_VIAL_ORANGE, 3.5f, 4, false, error) &&
           ab_delayed_callbacks_add(game->delayed, 1.3f, &request->inflate,
                                    game->absent, error);
}

static bool game_vial_queue_update(void *context, void *receiver, float seconds,
                                   AbError *error) {
    (void)seconds;
    PreviewVialObject *row = context;
    AbDataTable *physical = game_vial_physical(row);
    if (receiver != physical)
        return ab_fail(error, "Vial callback receiver changed");
    if (menu_route_truthy(menu_route_field(physical, "vialEffectInProgress")))
        return true;
    AbDataValue queue = menu_route_field(physical, "vialEffectQueue");
    if (queue.kind != AB_DATA_TABLE || queue.table != &row->queue)
        return ab_fail(error, "Vial callback lost its actual effect queue");
    if (!row->queue.count)
        return game_vial_write(row, "vialEffectQueue", (AbDataValue){0}, error) &&
               game_vial_remove_update(row, error);
    AbDataValue value = row->queue.entries[0].value;
    if (value.kind != AB_DATA_OBJECT || !value.object)
        return ab_fail(error, "Vial queue contains an unbound effect closure");
    PreviewVialRequest *request = (PreviewVialRequest *)value.object;
    if (request->target != row) {
        if (row->requests_used == row->owner->request_capacity)
            return ab_fail(error, "Cloned vial closure storage exhausted");
        PreviewVialRequest *invocation = row->requests + row->requests_used++;
        *invocation = *request;
        invocation->target = row;
        request = invocation;
    }
    bool retain = false;
    AbDataValue effect = menu_route_field(request->vial_definition.table, "onRemoved");
    if (menu_route_equal(effect, game_text("blackBirdBlueVialRemoved"))) {
        if (!game_vial_blue(request, error))
            return false;
    } else if (menu_route_equal(effect, game_text("blackBirdPinkVialRemoved"))) {
        if (!game_vial_pink(request, &retain, error))
            return false;
    } else if (menu_route_equal(effect, game_text("blackBirdOrangeVialRemoved"))) {
        request->transition = (AbDelayedFunction){request, game_vial_transition_later};
        request->inflate = (AbDelayedFunction){request, game_vial_inflate_later};
        if (!game_vial_orange(request, &retain, error))
            return false;
    } else {
        return ab_fail(error, "Vial queue requires its recovered effect branch");
    }
    if (!retain) {
        for (size_t index = 1; index < row->queue.count; ++index)
            row->queue.entries[index - 1].value = row->queue.entries[index].value;
        --row->queue.count;
    }
    return true;
}

static bool game_vial_affect_goals(PreviewVialObject *vial, AbError *error) {
    PreviewVials *owner = vial->owner;
    PreviewGame *game = owner->game;
    AbClassicDynamicsView dynamics =
        ab_classic_dynamics_view(ab_classic_world_runtime_view(game->world).dynamics);
    AbStringMap *goals = ab_classic_goals_map(dynamics.goals);
    if (!ab_string_map_reset(owner->goal_copy, 0, error))
        return false;
    size_t cursor = 0;
    AbStringMapEntry entry;
    while (true) {
        if (!ab_string_map_next(goals, &cursor, &entry, error))
            return false;
        if (!entry.present)
            break;
        if (!ab_string_map_set(owner->goal_copy, entry.key, entry.value, true, error))
            return false;
    }
    float x, y, radius;
    AbDataValue effect = menu_route_field(vial->definition.table, "vialEffect");
    if (!game_shock_read(game_vial_physical(vial), "x", &x, error) ||
        !game_shock_read(game_vial_physical(vial), "y", &y, error) ||
        !game_shock_read(effect.table, "radius", &radius, error))
        return false;
    cursor = 0;
    while (ab_string_map_next(owner->goal_copy, &cursor, &entry, error)) {
        if (!entry.present)
            return true;
        const AbDataValue *physical = (const AbDataValue *)entry.value;
        if (!physical || physical->kind != AB_DATA_TABLE)
            return ab_fail(error, "Vial goal snapshot lost its canonical identity");
        if (menu_route_truthy(menu_route_field(physical->table, "isMightyEagle")) ||
            menu_route_truthy(menu_route_field(physical->table, "isMightyBait")))
            continue;
        float target_x, target_y;
        if (!game_shock_read(physical->table, "x", &target_x, error) ||
            !game_shock_read(physical->table, "y", &target_y, error))
            return false;
        target_x -= x;
        target_y -= y;
        if (!(target_x * target_x + target_y * target_y < radius * radius))
            continue;
        PreviewVialObject *row = NULL;
        if (!game_vial_find(owner, *physical, &row, error))
            return false;
        if (row->requests_used == owner->request_capacity)
            return ab_fail(error, "Vial effect closure storage exhausted");
        PreviewVialRequest *request = row->requests + row->requests_used++;
        request->target = row;
        request->vial_definition = vial->definition;
        request->transition = (AbDelayedFunction){request, game_vial_transition_later};
        request->inflate = (AbDelayedFunction){request, game_vial_inflate_later};
        if (!game_vial_definition(game, row->object, &request->definition, error))
            return false;
        if (!menu_route_truthy(menu_route_field(physical->table, "vialEffectQueue"))) {
            row->queue.count = 0;
            if (!game_vial_write(
                    row, "vialEffectQueue",
                    (AbDataValue){.kind = AB_DATA_TABLE, .table = &row->queue}, error))
                return false;
        }
        if (row->queue.count == row->queue.capacity)
            return ab_fail(error, "Vial effect queue exhausted");
        size_t index = row->queue.count++;
        row->queue.entries[index] =
            (AbDataEntry){menu_route_number((float)(index + 1)),
                          {.kind = AB_DATA_OBJECT, .object = request}};
        AbObjectUpdates *updates = NULL;
        bool present = false;
        if (!ab_classic_object_hooks_updates(game_vial_hooks(owner), row->object,
                                             &updates, error) ||
            (updates &&
             !ab_object_update_has(updates, &row->update, &present, error)) ||
            (present && !game_vial_remove_update(row, error)) ||
            !game_vial_add_update(row, error))
            return false;
    }
    return false;
}

#endif
