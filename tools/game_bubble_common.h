#ifndef AB_GAME_BUBBLE_COMMON_H
#define AB_GAME_BUBBLE_COMMON_H

static AbDataTable *game_bubble_physical(PreviewBubbleObject *row) {
    AbClassicLevelView level = ab_classic_level_view(row->owner->game->playfield.level);
    return &level.properties->objects[row->object].physical;
}

static bool game_bubble_read(const AbDataTable *table, const char *key, float *output,
                             AbError *error) {
    AbDataValue value = menu_route_field(table, key);
    if (value.kind != AB_DATA_NUMBER || !isfinite(menu_real(value.bits)))
        return ab_fail(error, "Missing numeric bubble field: %s", key);
    *output = menu_real(value.bits);
    return true;
}

static bool game_bubble_write(PreviewBubbleObject *row, const char *key,
                              AbDataValue value, AbError *error) {
    AbObjectPropertyAccess access = {
        .physics_object = {.kind = AB_DATA_TABLE, .table = game_bubble_physical(row)}};
    return ab_object_property_set(&access, menu_span(key), value, error);
}

static AbClassicObjectHooks *game_bubble_hooks(PreviewBubbles *owner) {
    AbClassicWorldRuntimeView world = ab_classic_world_runtime_view(owner->game->world);
    return ab_classic_flames_updates(
        ab_classic_block_scripts_view(world.scripts).flames);
}

static bool game_bubble_add_update(PreviewBubbleObject *row, AbError *error) {
    AbClassicObjectHooks *hooks = game_bubble_hooks(row->owner);
    AbObjectUpdates *updates = NULL;
    if (!ab_classic_object_hooks_updates(hooks, row->object, &updates, error))
        return false;
    if (!updates)
        updates = &row->owned_updates;
    AbObjectUpdateList *promotion = NULL;
    if (updates->object.kind == AB_OBJECT_UPDATE_FUNCTION) {
        if (row->promotions_used == 4)
            return ab_fail(error, "Bubble callback promotion storage exhausted");
        size_t index = row->promotions_used++;
        row->promotions[index] = (AbObjectUpdateList){row->functions[index], 0, 8};
        promotion = row->promotions + index;
    }
    return ab_object_update_add(updates, &row->update, promotion, error) &&
           ab_classic_object_hooks_register(hooks, row->object, updates, error);
}

static bool game_bubble_remove_update(PreviewBubbleObject *row, AbError *error) {
    AbObjectUpdates *updates = NULL;
    return ab_classic_object_hooks_updates(game_bubble_hooks(row->owner), row->object,
                                           &updates, error) &&
           (updates ? ab_object_update_remove(updates, &row->update, error)
                    : ab_fail(error, "Bubble callback lost its property"));
}

static bool game_bubble_particles(PreviewBubbleObject *row, const char *name,
                                  AbError *error) {
    PreviewGame *game = row->owner->game;
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    /* The source's extra sixth argument is ignored by addParticles147263,
     * whose actual parameter count is five. */
    return ab_classic_particles_legacy(
        game->particles, level.bodies->objects[row->object].name, game_text(name),
        menu_route_number(1), false, false, error);
}

static bool game_bubble_queue(PreviewBubbleObject *row, AbVec2 force, AbError *error) {
    PreviewGame *game = row->owner->game;
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    AbClassicWorldRuntimeView world = ab_classic_world_runtime_view(game->world);
    AbClassicSimulationView simulation =
        ab_classic_simulation_view(ab_classic_dynamics_view(world.dynamics).simulation);
    AbVec2 point;
    return game_bubble_read(game_bubble_physical(row), "x", &point.x, error) &&
           game_bubble_read(game_bubble_physical(row), "y", &point.y, error) &&
           ab_force_queue_append(
               simulation.queue,
               (AbQueuedForce){level.bodies->objects[row->object].name, force, point},
               error);
}

static bool game_bubble_velocity(PreviewBubbleObject *row, bool reset_y,
                                 AbVec2 *velocity, AbError *error) {
    PreviewGame *game = row->owner->game;
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    size_t body = level.frame->objects[row->object].body;
    if (body >= level.world->body_slot_count || !level.world->body_alive[body])
        return ab_fail(error, "Bubble object lost its live physics body");
    *velocity = level.world->storage.bodies[body].motion.mass.linear_velocity;
    if (reset_y) {
        velocity->y = 0;
        return ab_body_velocity_set(&level.world->storage.bodies[body].motion,
                                    *velocity) ||
               ab_fail(error, "Cannot reset bubble object velocity");
    }
    return true;
}

/* Bubbles3017 is shared by Stella's ordinary bubbles and floating vial pigs.
 * Neither path needs the other's visual/charge owner to apply this force. */
static bool game_bubble_apply_float(PreviewGame *game, size_t object, AbError *error) {
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    size_t body = level.frame->objects[object].body;
    if (!ab_world_has_body(level.world, body))
        return ab_fail(error, "Floating object lost its live physics body");
    AbVec2 velocity = level.world->storage.bodies[body].motion.mass.linear_velocity;
    AbVec2 force, point;
    bool apply;
    bool held_a = menu_route_truthy(menu_route_field(&game->held, "A"));
    if (!ab_bird_bubble_force(velocity, held_a, 0, 0, 0, &force, &apply, error))
        return false;
    if (!apply)
        return true;
    AbDataTable *physical = &level.properties->objects[object].physical;
    float lateral = 0, mass;
    if (menu_route_truthy(menu_route_field(physical, "bubbleAntiGravityFloat")) &&
        !game_bubble_read(physical, "bubbleAntiGravityFloat", &lateral, error))
        return false;
    AbClassicWorldRuntimeView world = ab_classic_world_runtime_view(game->world);
    AbClassicSimulationView simulation =
        ab_classic_simulation_view(ab_classic_dynamics_view(world.dynamics).simulation);
    return game_bubble_read(physical, "mass", &mass, error) &&
           ab_bird_bubble_force(velocity, held_a, lateral, mass,
                                level.world->definition.gravity.y, &force, &apply,
                                error) &&
           game_bubble_read(physical, "x", &point.x, error) &&
           game_bubble_read(physical, "y", &point.y, error) &&
           ab_force_queue_append(
               simulation.queue,
               (AbQueuedForce){level.bodies->objects[object].name, force, point},
               error);
}

#endif
