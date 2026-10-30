#ifndef AB_GAME_VIAL_BLUE_H
#define AB_GAME_VIAL_BLUE_H

typedef struct {
    PreviewGame *game;
    AbError *error;
    bool occupied, okay;
} PreviewVialIntersection;

/* GameLua32bac0 queries the fat-proxy AABB and QueryCallback33f42c includes
 * static bodies without a shape-point test. Vial9918 uses equal +.01 offsets;
 * its predicate accepts names absent from objects.dynamic. */
static bool game_vial_intersection(void *context, int32_t proxy, uintptr_t payload) {
    (void)proxy;
    PreviewVialIntersection *query = context;
    AbClassicLevelView level = ab_classic_level_view(query->game->playfield.level);
    if (!payload || !ab_world_has_fixture(level.world, payload - 1)) {
        query->okay = ab_fail(query->error, "Invalid vial intersection fixture");
        return false;
    }
    const AbWorldFixture *fixture = level.world->storage.fixtures + payload - 1;
    for (size_t object = 0; object < level.bodies->count; ++object) {
        if (fixture->user_data != (uintptr_t)(level.bodies->objects + object))
            continue;
        const AbDataValue *dynamic =
            ab_data_get(&level.properties->dynamic,
                        (AbDataValue){.kind = AB_DATA_STRING,
                                      .string = level.bodies->objects[object].name});
        query->occupied = !dynamic || dynamic->kind == AB_DATA_NIL;
        return !query->occupied;
    }
    query->okay = ab_fail(query->error, "Vial intersection lost its native body name");
    return false;
}

static bool game_vial_blue_occupied(PreviewVialObject *row, bool *output,
                                    AbError *error) {
    PreviewGame *game = row->owner->game;
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    AbVec2 point;
    if (!game_shock_read(game_vial_physical(row), "x", &point.x, error) ||
        !game_shock_read(game_vial_physical(row), "y", &point.y, error))
        return false;
    point.x += .01f;
    point.y += .01f;
    PreviewVialIntersection query = {.game = game, .error = error, .okay = true};
    if (!ab_tree_query(&level.world->broadphase.tree, (AbAabb){point, point},
                       game_vial_intersection, &query, error) ||
        !query.okay)
        return false;
    *output = query.occupied;
    return true;
}

static bool game_vial_body(PreviewVialObject *row, AbBodyMotion **output,
                           AbError *error) {
    AbClassicLevelView level = ab_classic_level_view(row->owner->game->playfield.level);
    size_t body = level.frame->objects[row->object].body;
    if (!ab_world_has_body(level.world, body) || level.removed_objects[row->object])
        return ab_fail(error, "Vial effect lost its live native body");
    *output = &level.world->storage.bodies[body].motion;
    return true;
}

static bool game_vial_blue_spin(void *context, void *receiver, float seconds,
                                AbError *error) {
    PreviewVialObject *row = context;
    PreviewGame *game = row->owner->game;
    AbDataTable *physical = game_vial_physical(row);
    float acceleration, speed, mass, x, y;
    AbBodyMotion *body = NULL;
    if (receiver != physical || !game_vial_body(row, &body, error) ||
        !game_shock_read(physical, "blueVialSpinAcceleration", &acceleration, error) ||
        !game_shock_optional(physical, "blueVialSpinSpeed", 0, &speed, error) ||
        !game_shock_read(physical, "mass", &mass, error) ||
        !game_shock_read(physical, "x", &x, error) ||
        !game_shock_read(physical, "y", &y, error))
        return false;
    seconds *= game->speed;
    acceleration += seconds * 8;
    speed += seconds * acceleration;
    float denominator = fmaxf(fabsf(body->mass.linear_velocity.y), 1);
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    AbClassicSimulation *simulation =
        ab_classic_dynamics_view(ab_classic_world_runtime_view(game->world).dynamics)
            .simulation;
    return game_vial_write(row, "blueVialSpinAcceleration",
                           menu_route_number(acceleration), error) &&
           game_vial_write(row, "blueVialSpinSpeed", menu_route_number(speed), error) &&
           ab_body_angular_velocity_set(body, speed) &&
           ab_force_queue_append(
               ab_classic_simulation_view(simulation).queue,
               (AbQueuedForce){level.bodies->objects[row->object].name,
                               {0, (-60 * mass) / denominator},
                               {x, y}},
               error);
}

static bool game_vial_blue_transition(void *context, AbDataValue parameters,
                                      AbError *error) {
    (void)parameters;
    PreviewVialRequest *request = context;
    return !game_vial_live(request->target) ||
           menu_route_truthy(
               menu_route_field(game_vial_physical(request->target), "isPuffing")) ||
           game_vial_transition(request, error);
}

static bool game_vial_blue_restore(PreviewVialObject *row, AbError *error) {
    AbDataTable *physical = game_vial_physical(row);
    if (!menu_route_truthy(menu_route_field(physical, "dontUpdateDamageSprite")))
        return true;
    AbDataValue sprite = menu_route_field(physical, "damageSprite");
    return sprite.kind == AB_DATA_STRING &&
           game_vial_write(row, "dontUpdateDamageSprite", row->owner->game->disabled,
                           error) &&
           ab_classic_visuals_set_sprite(row->owner->game->playfield.visuals,
                                         row->object, sprite.string, error);
}

static bool game_vial_blue_finish(PreviewVialRequest *request, PreviewVialObject *row,
                                  float cosine, float sine, AbError *error) {
    PreviewGame *game = row->owner->game;
    PreviewVialObject *original = request->target;
    AbDataTable *physical = game_vial_physical(row);
    AbBodyMotion *body = NULL;
    if (!game_vial_body(row, &body, error) ||
        !ab_body_velocity_set(body,
                              (AbVec2){(cosine * 10) * .6f, fminf(0, (-sine) * 10)}))
        return ab_fail(error, "Cannot set the actual Blue-vial clone velocity");
    if (menu_route_truthy(
            menu_route_field(game_vial_physical(original), "levelGoal")) &&
        !menu_route_truthy(menu_route_field(physical, "isPuffing"))) {
        float blink, oink;
        AbDamageSpriteNames faces;
        AbClassicGoals *goals = ab_classic_dynamics_view(
                                    ab_classic_world_runtime_view(game->world).dynamics)
                                    .goals;
        if (!game_vial_blue_restore(row, error) ||
            !game_vial_write(
                row, "levelGoal",
                menu_route_field(game_vial_physical(original), "levelGoal"), error) ||
            !game_shock_random(game->shockwaves, 2, 5, 30, &blink, error) ||
            !game_vial_write(row, "blinkTimer", menu_route_number(blink / 10), error) ||
            !game_shock_random(game->shockwaves, 2, 5, 30, &oink, error) ||
            !game_vial_write(row, "oinkTimer", menu_route_number(oink / 10), error) ||
            !ab_classic_visuals_update_damage(game->playfield.visuals, row->object,
                                              &game->workspace, &faces, error) ||
            !game_vial_write(row, "blinkSprite", faces.blink, error) ||
            !game_vial_write(row, "smileSprite", faces.smile, error) ||
            !ab_classic_goals_set(goals, row->object, true, error))
            return false;
    }
    bool occupied = false;
    if (!menu_route_truthy(menu_route_field(game_vial_physical(original),
                                            "shouldDieAfterVialEffect")) &&
        !game_vial_blue_occupied(row, &occupied, error))
        return false;
    if (occupied || menu_route_truthy(menu_route_field(game_vial_physical(original),
                                                       "shouldDieAfterVialEffect")))
        return game_shock_dead(game->shockwaves->objects + row->object, error);
    if (row == original)
        return true;
    AbDataValue speed =
        menu_route_field(game_vial_physical(original), "blueVialSpinSpeed");
    if (menu_route_truthy(speed) &&
        (speed.kind != AB_DATA_NUMBER ||
         !ab_body_angular_velocity_set(body, menu_real(speed.bits))))
        return ab_fail(error, "Cannot copy Blue-vial angular velocity");
    row->queue.count = row->owner->queue_snapshot.count;
    memcpy(row->queue.entries, row->owner->queue_snapshot.entries,
           row->queue.count * sizeof(*row->queue.entries));
    if (!game_vial_write(row, "vialEffectQueue",
                         (AbDataValue){.kind = AB_DATA_TABLE, .table = &row->queue},
                         error) ||
        !game_vial_add_update(row, error) ||
        !game_shock_transfer(game->shockwaves->objects + original->object,
                             game->shockwaves->objects + row->object, error))
        return false;
    AbDataValue effect = menu_route_field(request->vial_definition.table, "vialEffect");
    float duration;
    if (!game_shock_optional(effect.table, "invulnerabilityDuration", 2, &duration,
                             error) ||
        (row->queue.count &&
         !game_shock_optional(effect.table, "invulnerabilityDurationIfEffectsLeft",
                              duration, &duration, error)))
        return false;
    return game_shock_make_invulnerable(game->shockwaves->objects + row->object,
                                        duration, error);
}

static bool game_vial_blue_clone(void *context, AbDataValue parameters,
                                 AbError *error) {
    (void)parameters;
    PreviewVialRequest *request = context;
    PreviewVialObject *original = request->target;
    if (!game_vial_live(original))
        return true;
    PreviewVials *owner = original->owner;
    PreviewGame *game = owner->game;
    AbDataTable *physical = game_vial_physical(original);
    AbDataValue queue = menu_route_field(physical, "vialEffectQueue");
    if (queue.kind != AB_DATA_TABLE || queue.table != &original->queue ||
        original->queue.count > owner->queue_snapshot.capacity)
        return ab_fail(error, "Blue-vial cloning lost its captured effect queue");
    owner->queue_snapshot.count = original->queue.count;
    memcpy(owner->queue_snapshot.entries, original->queue.entries,
           original->queue.count * sizeof(*original->queue.entries));
    float x, y, radius, scale;
    AbDataValue effect = menu_route_field(request->vial_definition.table, "vialEffect");
    if (!game_vial_remove_function(original, &original->spin, error) ||
        !game_shock_read(physical, "x", &x, error) ||
        !game_shock_read(physical, "y", &y, error) ||
        !game_shock_read(request->definition.table, "radius", &radius, error) ||
        !game_shock_read(effect.table, "cloneScale", &scale, error) ||
        !game_shock_audio(game->shockwaves, game_text("pigs_potion_split_01"), error))
        return false;
    for (size_t index = 1; index <= 3; ++index) {
        float angle = (float)(120.0 * 0x1.1df46a2529d39p-6);
        angle = angle * ((float)index - 2);
        angle -= 0x1.921fb6p+1f / 2;
        float cosine, sine;
        if (!ab_script_cosine(angle, &cosine) || !ab_script_sine(angle, &sine))
            return ab_fail(error, "Invalid Blue-vial clone direction");
        PreviewVialObject *row = original;
        if (index != 2) {
            AbVec2 position;
            size_t object = SIZE_MAX;
            if (!game_shock_read(physical, "x", &position.x, error) ||
                !game_shock_read(physical, "y", &position.y, error) ||
                !game_factory_goal_circle(game,
                                          menu_route_field(physical, "definition"),
                                          position, &object, error))
                return false;
            row = owner->objects + object;
            PreviewVialVisual *source = original->visuals + GAME_VIAL_BLUE;
            if (!game_vial_visual(row, GAME_VIAL_BLUE, source->duration, source->speed,
                                  false, error))
                return false;
            row->visuals[GAME_VIAL_BLUE].time = source->time;
            row->visuals[GAME_VIAL_BLUE].started = source->started;
        }
        float distance = request->clone_width * .5f;
        if (!ab_classic_level_set_position(
                game->playfield.level, row->object,
                (AbVec2){x + cosine * distance, y - sine * distance}, error))
            return false;
        row->spin_fields[0] = (AbDataEntry){game_text("x"), menu_route_number(x)};
        row->spin_fields[1] = (AbDataEntry){game_text("y"), menu_route_number(y)};
        row->spin_center = (AbDataTable){row->spin_fields, 2, 2, NULL};
        AbClassicSimulation *simulation =
            ab_classic_dynamics_view(
                ab_classic_world_runtime_view(game->world).dynamics)
                .simulation;
        if (!game_vial_write(
                row, "blueVialSpinCenter",
                (AbDataValue){.kind = AB_DATA_TABLE, .table = &row->spin_center},
                error) ||
            !game_vial_write(row, "blueVialSpinAngle", menu_route_number(angle),
                             error) ||
            !game_vial_write(row, "blueVialSpinDistance", menu_route_number(distance),
                             error) ||
            !game_vial_write(row, "blueVialSpinSpeed",
                             menu_route_field(physical, "blueVialSpinSpeed"), error) ||
            !game_vial_write(row, "blueVialSpinAcceleration",
                             menu_route_field(physical, "blueVialSpinAcceleration"),
                             error) ||
            !ab_classic_scene_set_scale(
                ab_classic_render_scene_objects(game->playfield.scene), row->object,
                (AbVec2){scale, scale}, error) ||
            !ab_classic_level_resize_circle(game->playfield.level, row->object,
                                            radius * scale, error) ||
            !ab_classic_simulation_refresh_circle_radius(simulation, row->object,
                                                         error) ||
            !game_vial_blue_restore(row, error) ||
            !game_vial_blue_finish(request, row, cosine, sine, error))
            return false;
    }
    return game_vial_write(original, "vialEffectInProgress", game->disabled, error);
}

static bool game_vial_blue(PreviewVialRequest *request, AbError *error) {
    PreviewVialObject *row = request->target;
    PreviewGame *game = row->owner->game;
    AbDataTable *physical = game_vial_physical(row);
    AbBodyMotion *body = NULL;
    AbDataValue sprite = menu_route_field(physical, "sprite");
    AbSpriteAtlasBinding binding;
    if (!game_vial_write(row, "vialEffectInProgress", game->enabled, error) ||
        !game_vial_body(row, &body, error) || sprite.kind != AB_DATA_STRING ||
        !game->menu->sprites.provider.sprite(game->menu->sprites.provider.context,
                                             sprite.string, &binding) ||
        !binding.sprite.region)
        return ab_fail(error, "Blue vial requires its actual body and sprite bounds");
    request->clone_width = (float)binding.sprite.region->width * .05f;
    if (menu_route_truthy(menu_route_field(physical, "levelGoal")) &&
        !menu_route_truthy(menu_route_field(physical, "isPuffing"))) {
        float acceleration =
            menu_route_truthy(menu_route_field(physical, "blueVialSpinAcceleration"))
                ? 2.0f
                : 8.0f;
        if (!game_vial_remove_gadgets(row, error) ||
            !game_shock_make_invulnerable(game->shockwaves->objects + row->object, 1.8f,
                                          error) ||
            !game_vial_write(row, "blueVialSpinSpeed",
                             menu_route_number(body->mass.angular_velocity * .25f),
                             error) ||
            !game_vial_write(row, "blueVialSpinAcceleration",
                             menu_route_number(acceleration), error) ||
            !game_vial_remove_function(row, &row->spin, error) ||
            !game_vial_add_function(row, &row->spin, error))
            return false;
    }
    request->transition = (AbDelayedFunction){request, game_vial_blue_transition};
    request->inflate = (AbDelayedFunction){request, game_vial_blue_clone};
    return game_vial_visual(row, GAME_VIAL_BLUE, 3, 6, true, error) &&
           ab_delayed_callbacks_add(game->delayed, 1, &request->transition,
                                    game->absent, error) &&
           ab_delayed_callbacks_add(game->delayed, 1.8f, &request->inflate,
                                    game->absent, error);
}

#endif
