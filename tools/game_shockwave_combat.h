#ifndef AB_GAME_SHOCKWAVE_COMBAT_H
#define AB_GAME_SHOCKWAVE_COMBAT_H

static bool game_shock_damage(PreviewShockObject *row, float damage, AbError *error) {
    PreviewGame *game = row->owner->game;
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    float strength;
    if (!game_shock_read(game_shock_physical(row), "strength", &strength, error))
        return false;
    strength -= damage;
    if (!game_shock_number(row, "strength", strength, error))
        return false;
    level.character_objects[row->object].goal.strength = strength;
    AbDamageSpriteNames names;
    if (!ab_classic_visuals_update_damage(game->playfield.visuals, row->object,
                                          &game->workspace, &names, error) ||
        !game_shock_write(row, "damageSprite", names.sprite, error) ||
        !game_shock_write(row, "blinkSprite", names.blink, error) ||
        !game_shock_write(row, "smileSprite", names.smile, error) ||
        (strength <= 0 && !game_shock_dead(row, error)))
        return false;
    game->globals.score = menu_route_number(strength <= 0 ? strength : damage);
    return true;
}

static bool game_shock_score(PreviewShockObject *row, const AbDataTable *definition,
                             float score, AbError *error) {
    PreviewShockwaves *owner = row->owner;
    PreviewGame *game = owner->game;
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    AbDataTable *physical = game_shock_physical(row);
    AbSpan name = level.bodies->objects[row->object].name;
    AbObjectDefinition resolved;
    AbDataValue key = menu_route_field(physical, "definition");
    if (key.kind != AB_DATA_STRING ||
        !ab_object_definition_open(level.catalog, key.string, NULL, &resolved, error))
        return false;
    AbDataValue particles = ab_object_property(&resolved, menu_span("particles"));
    if (menu_route_truthy(particles) &&
        !menu_route_truthy(menu_route_field(physical, "levelGoal"))) {
        float amount;
        if (!game_shock_random(owner, 2, 12, 20, &amount, error))
            return false;
        AbDataValue limits = menu_route_field(definition, "particlesAmountLimits");
        if (limits.kind != AB_DATA_NIL) {
            if (limits.kind != AB_DATA_TABLE)
                return ab_fail(error, "Invalid electrical damage particle limits");
            AbDataValue low = game_script_index(limits.table, 1);
            AbDataValue high = game_script_index(limits.table, 2);
            if (low.kind != AB_DATA_NUMBER || high.kind != AB_DATA_NUMBER ||
                !isfinite(menu_real(low.bits)) || !isfinite(menu_real(high.bits)) ||
                (double)menu_real(low.bits) < INT32_MIN ||
                (double)menu_real(low.bits) > INT32_MAX ||
                (double)menu_real(high.bits) < INT32_MIN ||
                (double)menu_real(high.bits) > INT32_MAX ||
                !game_shock_random(owner, 2, (int32_t)menu_real(low.bits),
                                   (int32_t)menu_real(high.bits), &amount, error))
                return ab_fail(error, "Unsupported electrical particle interval");
        }
        if (!ab_classic_particles_legacy(game->particles, name, particles,
                                         menu_route_number(amount), false, false,
                                         error))
            return false;
    }
    AbClassicWorldRuntimeView world = ab_classic_world_runtime_view(game->world);
    AbBlockCollisionScores *scores = world.block_scores;
    scores->total += score;
    if (scores->count == scores->capacity)
        return ab_fail(error, "Electrical damage score list exhausted");
    scores->entries[scores->count++] = score;
    AbDataValue raw_score = menu_route_number(score);
    uint8_t text[32];
    AbBirdCollisionFloatingScore floating = {.score = score,
                                             .lifetime = .6f,
                                             .max_scale = game->floating_scale *
                                                          (.25f + score / 3000),
                                             .text_present = true};
    return game_shock_read(physical, "x", &floating.position.x, error) &&
           game_shock_read(physical, "y", &floating.position.y, error) &&
           ab_ui_value_text((AbUiPointerValue){.kind = AB_UI_POINTER_NUMBER,
                                               .number_bits = raw_score.bits},
                            text, sizeof(text), &floating.text) &&
           ab_floating_scores_add(world.floating, &floating, error);
}

static bool game_shock_impact(PreviewShockObject *wave, PreviewShockObject *row,
                              AbError *error) {
    PreviewGame *game = row->owner->game;
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    AbDataTable *physical = game_shock_physical(row);
    AbDataValue definition;
    if (!game_shock_definition(row, &definition, error))
        return false;
    AbDataValue types = menu_route_field(level.catalog, "collider_types");
    if (types.kind != AB_DATA_TABLE)
        return ab_fail(error, "Electrical contact has no original collider types");
    bool bird = menu_route_equal(menu_route_field(definition.table, "collider"),
                                 menu_route_field(types.table, "bird"));
    AbBirdShockwaveObject input = {
        .bird = bird,
        .shot = menu_route_truthy(menu_route_field(physical, "shot")),
        .same_wave =
            menu_route_equal(menu_route_field(physical, "damagedByShockwave"),
                             menu_route_field(game_shock_physical(wave), "name")),
        .live = !level.removed_objects[row->object]};
    if (bird && !input.shot)
        return true;
    AbVec2 point;
    if (!game_shock_read(physical, "x", &point.x, error) ||
        !game_shock_read(physical, "y", &point.y, error))
        return false;
    input.relative = (AbVec2){point.x - wave->origin.x, point.y - wave->origin.y};
    if (!bird && !input.same_wave && input.live) {
        AbDataValue strength = menu_route_field(definition.table, "strength");
        input.definition_strength_present = menu_route_truthy(strength);
        if ((input.definition_strength_present &&
             !game_shock_read(definition.table, "strength", &input.definition_strength,
                              error)) ||
            !game_shock_read(physical, "strength", &input.strength, error) ||
            !game_shock_optional(definition.table, "defence", 0,
                                 &input.definition_defence, error))
            return false;
    }
    AbBirdShockwaveImpact impact;
    if (!ab_bird_shockwave_impact(&wave->wave, &input, &impact, error))
        return false;
    if (!impact.inside)
        return true;
    AbSpan name = level.bodies->objects[row->object].name;
    if (!ab_classic_joints_break(level.joints, name, 1000,
                                 ab_classic_joint_updates_defer, game->joint_updates,
                                 error))
        return false;
    if (impact.mark_damaged &&
        !game_shock_write(row, "damagedByShockwave",
                          menu_route_field(game_shock_physical(wave), "name"), error))
        return false;
    if (impact.inflict &&
        (!game_shock_damage(row, impact.damage, error) ||
         !game_shock_score(row, definition.table, impact.score, error)))
        return false;
    size_t body = level.frame->objects[row->object].body;
    if (impact.impulse &&
        !ab_body_impulse_apply(&level.world->storage.bodies[body].motion, impact.vector,
                               &point))
        return ab_fail(error, "Cannot apply actual electrical impulse");
    if (impact.force) {
        /* Global applyForce160869 captures the five script arguments unchanged;
         * applyForcesAtPhysicsStep161227 replays them at each native step. */
        AbClassicWorldRuntimeView world = ab_classic_world_runtime_view(game->world);
        AbClassicSimulationView simulation = ab_classic_simulation_view(
            ab_classic_dynamics_view(world.dynamics).simulation);
        if (!ab_force_queue_append(simulation.queue,
                                   (AbQueuedForce){name, impact.vector, point}, error))
            return false;
    }
    return true;
}

#endif
