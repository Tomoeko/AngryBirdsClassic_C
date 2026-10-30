#ifndef AB_GAME_VIAL_PINK_H
#define AB_GAME_VIAL_PINK_H

/* Bubbles1523/3925/4558 and VialEffects14966/15487, pinned Android 5.2.0.
 * Captured physical tables outlive body removal. Retain their insertion data
 * and native string-hash traversal separately; subsequent source writes only
 * delete keys, so surviving traversal slots never move. */
static bool game_vial_pink_mask(PreviewVialObject *row, uint16_t mask, AbError *error) {
    AbClassicLevelView level = ab_classic_level_view(row->owner->game->playfield.level);
    AbBodyMotion *body = NULL;
    if (!game_vial_body(row, &body, error))
        return false;
    (void)body;
    size_t native_body = level.frame->objects[row->object].body;
    /* GameObject::setFilterMask VA 0x344360 preserves category/group and walks
     * the native fixture list newest first, narrowing the supplied mask to u16. */
    for (size_t rank = level.world->fixture_count; rank > 0; --rank) {
        size_t fixture = level.world->fixture_order[rank - 1];
        AbWorldFixture *item = level.world->storage.fixtures + fixture;
        if (item->body != native_body)
            continue;
        AbCollisionFilter filter = item->filter;
        filter.mask = mask;
        if (!ab_world_set_fixture_filter(level.world, fixture, filter, error))
            return false;
    }
    return true;
}

static bool game_vial_pink_release(PreviewVialObject *row, bool restore_filter,
                                   AbError *error) {
    AbBodyMotion *body = NULL;
    if (!game_vial_body(row, &body, error))
        return false;
    AbVec2 velocity = body->mass.linear_velocity;
    for (size_t rank = 0; rank < row->captured.count; ++rank) {
        size_t index = row->captured_indices[rank];
        if (row->captured_entries[index].value.kind == AB_DATA_NIL)
            continue;
        size_t object = row->captured_indices[row->captured.count + index];
        PreviewVialObject *target = row->owner->objects + object;
        if (restore_filter &&
            (!game_vial_write(target, "inBubble", (AbDataValue){0}, error) ||
             !game_vial_write(target, "dontDrawBubble", (AbDataValue){0}, error)))
            return false;
        if (!game_vial_live(target))
            continue;
        AbBodyMotion *target_body = NULL;
        if ((restore_filter && !game_vial_pink_mask(target, UINT16_MAX, error)) ||
            !game_vial_body(target, &target_body, error) ||
            !ab_body_velocity_set(target_body, velocity))
            return ab_fail(error, "Cannot release actual floating-pig contents");
    }
    return true;
}

static bool game_vial_pink_update(void *context, void *receiver, float seconds,
                                  AbError *error) {
    PreviewVialObject *row = context;
    PreviewGame *game = row->owner->game;
    AbDataTable *physical = game_vial_physical(row);
    float timer, next;
    bool cleanup;
    if (receiver != physical ||
        !game_shock_read(physical, "bubbleAntiGravityTimer", &timer, error) ||
        !ab_bird_bubble_timer(timer, seconds, game->speed, &next, &cleanup, error))
        return ab_fail(error, "Invalid floating-pig callback receiver/timer");
    if (cleanup) {
        AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
        AbClassicWorldRuntimeView world = ab_classic_world_runtime_view(game->world);
        return game_vial_write(row, "bubbleAntiGravityTimer", game->absent, error) &&
               game_vial_write(row, "bubbleAntiGravityFloat", game->absent, error) &&
               game_vial_write(row, "inBubble", game->absent, error) &&
               game_vial_remove_function(row, &row->floating, error) &&
               ab_string_map_set(
                   world.dead_blocks, level.bodies->objects[row->object].name,
                   (uintptr_t)(world.physical_values + row->object), true, error) &&
               game_vial_pink_release(row, false, error);
    }
    AbVec2 origin;
    if (!game_vial_write(row, "bubbleAntiGravityTimer", menu_route_number(next),
                         error) ||
        !game_bubble_apply_float(game, row->object, error) ||
        !game_shock_read(physical, "x", &origin.x, error) ||
        !game_shock_read(physical, "y", &origin.y, error))
        return false;
    for (size_t rank = 0; rank < row->captured.count; ++rank) {
        size_t index = row->captured_indices[rank];
        AbDataEntry *entry = row->captured_entries + index;
        if (entry->value.kind == AB_DATA_NIL)
            continue;
        size_t object = row->captured_indices[row->captured.count + index];
        PreviewVialObject *target = row->owner->objects + object;
        if (!game_vial_live(target)) {
            entry->value = game->absent;
            continue;
        }
        AbVec2 offset;
        if (!game_shock_read(entry->value.table, "dx", &offset.x, error) ||
            !game_shock_read(entry->value.table, "dy", &offset.y, error) ||
            !ab_classic_level_set_position(
                game->playfield.level, object,
                (AbVec2){origin.x + offset.x, origin.y + offset.y}, error))
            return false;
    }
    return true;
}

static bool game_vial_pink_functions(void *context, size_t object,
                                     AbUiPointerValue *output, AbError *error) {
    PreviewGame *game = context;
    if (!game->vials || object >= game->vials->capacity)
        return ab_fail(error, "Unbound object removal-function list");
    PreviewVialObject *row = game->vials->objects + object;
    AbDataValue value = menu_route_field(game_vial_physical(row), "removeFunctions");
    if (value.kind != AB_DATA_TABLE || value.table != &row->remove_functions ||
        row->remove_functions.count != 1 ||
        row->remove_entry.value.kind != AB_DATA_OBJECT ||
        row->remove_entry.value.object != &row->floating)
        return ab_fail(error, "Unsupported current object removal-function list");
    *output = (AbUiPointerValue){.kind = AB_UI_POINTER_OBJECT,
                                 .object = (uintptr_t)&row->removal_list};
    return true;
}

static bool game_vial_pink_removal(void *context, AbUiPointerValue function,
                                   size_t object, AbError *error) {
    PreviewGame *game = context;
    if (!game->vials || object >= game->vials->capacity ||
        function.kind != AB_UI_POINTER_FUNCTION ||
        function.function != (uintptr_t)&game->vials->objects[object].floating)
        return ab_fail(error, "Unbound object removal function");
    return game_vial_pink_release(game->vials->objects + object, true, error);
}

static bool game_vial_pink_candidate(PreviewVialObject *row, size_t object,
                                     AbVec2 origin, float radius_squared,
                                     AbVec2 *offset, bool *inside, AbError *error) {
    PreviewGame *game = row->owner->game;
    AbDataValue definition;
    AbDataTable *physical = game_vial_physical(row->owner->objects + object);
    if (!game_vial_definition(game, object, &definition, error) ||
        !game_shock_read(physical, "x", &offset->x, error) ||
        !game_shock_read(physical, "y", &offset->y, error))
        return false;
    offset->x -= origin.x;
    offset->y -= origin.y;
    AbDataValue material = menu_route_field(definition.table, "materialName");
    *inside = !menu_route_truthy(menu_route_field(definition.table, "controllable")) &&
              !menu_route_equal(material, game_text("immovable")) &&
              !menu_route_equal(material, game_text("bubble")) &&
              offset->x * offset->x + offset->y * offset->y < radius_squared &&
              object != row->object;
    return true;
}

static bool game_vial_pink_capture(PreviewVialObject *row, float radius,
                                   AbError *error) {
    PreviewVials *owner = row->owner;
    PreviewGame *game = owner->game;
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    AbVec2 origin;
    if (!game_shock_read(game_vial_physical(row), "x", &origin.x, error) ||
        !game_shock_read(game_vial_physical(row), "y", &origin.y, error) ||
        !ab_string_map_reset(owner->capture_workspace, 0, error))
        return false;
    /* Reserve exact retained table storage once at transformation. The source
     * callbacks below only queue joints, so candidate topology stays stable. */
    size_t count = 0;
    for (unsigned pass = 0; pass < 2; ++pass) {
        AbClassicWorldCursor cursor = {0};
        size_t used = 0;
        for (;;) {
            size_t object;
            bool present, inside;
            AbVec2 offset;
            if (!ab_classic_level_world_next(game->playfield.level, &cursor, &object,
                                             &present, error))
                return false;
            if (!present)
                break;
            if (!game_vial_pink_candidate(row, object, origin, radius * radius, &offset,
                                          &inside, error))
                return false;
            if (!inside)
                continue;
            if (!pass) {
                ++count;
                continue;
            }
            if (used == count)
                return ab_fail(error, "Floating capture topology changed");
            PreviewVialObject *target = owner->objects + object;
            AbSpan name = level.bodies->objects[object].name;
            row->captured_entries[used] = (AbDataEntry){
                {.kind = AB_DATA_STRING, .string = name},
                {.kind = AB_DATA_TABLE, .table = game_vial_physical(target)}};
            row->captured_indices[count + used] = object;
            if (!game_vial_write(target, "inBubble", game->enabled, error) ||
                !game_vial_write(target, "dontDrawBubble", game->enabled, error) ||
                !game_vial_write(target, "dx", menu_route_number(offset.x), error) ||
                !game_vial_write(target, "dy", menu_route_number(offset.y), error) ||
                !game_vial_pink_mask(target, 0, error) ||
                !ab_string_map_set(owner->capture_workspace, name, used + 1, true,
                                   error))
                return false;
            AbBodyMotion *body = NULL;
            float random;
            if (!game_vial_body(target, &body, error) ||
                !ab_body_velocity_set(body, (AbVec2){0}) ||
                ab_script_random(&game->menu->random, 0, 0, 0, &random) !=
                    AB_SCRIPT_RANDOM_OK ||
                !ab_body_angular_velocity_set(body, (random * 0x1.921fb6p+1f) / 2) ||
                !game_joint_defer_object(game, name, error))
                return ab_fail(error, "Cannot capture actual floating-pig contents");
            ++used;
        }
        if (pass && used != count)
            return ab_fail(error, "Floating capture count changed");
        if (!pass && count) {
            size_t item_size = sizeof(AbDataEntry) + 2 * sizeof(size_t);
            const size_t budget = 16 * 1024 * 1024;
            if (count > (budget - owner->captured_bytes) / item_size)
                return ab_fail(error, "Floating capture storage exceeds byte limit");
            row->captured_entries = calloc(count, sizeof(AbDataEntry));
            row->captured_indices = calloc(count * 2, sizeof(size_t));
            if (!row->captured_entries || !row->captured_indices)
                return ab_fail(error, "Cannot allocate actual floating capture table");
            owner->captured_bytes += count * item_size;
        }
        if (!pass) {
            row->captured = (AbDataTable){row->captured_entries, count, count, NULL};
            if (!game_vial_write(
                    row, "blocksInBubble",
                    (AbDataValue){.kind = AB_DATA_TABLE, .table = &row->captured},
                    error))
                return false;
        }
    }
    size_t cursor = 0, rank = 0;
    AbStringMapEntry entry;
    while (ab_string_map_next(owner->capture_workspace, &cursor, &entry, error)) {
        if (!entry.present)
            return rank == count || ab_fail(error, "Incomplete floating table order");
        if (rank == count || !entry.value || entry.value > count)
            return ab_fail(error, "Invalid floating table order");
        row->captured_indices[rank++] = entry.value - 1;
    }
    return false;
}

static bool game_vial_pink_transform(PreviewVialObject *old, PreviewVialObject **output,
                                     AbError *error) {
    PreviewGame *game = old->owner->game;
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    AbDataTable *physical = game_vial_physical(old);
    AbDataValue original = menu_route_field(physical, "definition");
    AbClassicDynamicsView dynamics =
        ab_classic_dynamics_view(ab_classic_world_runtime_view(game->world).dynamics);
    if (!ab_classic_removals_remove_pig(dynamics.removals,
                                        level.bodies->objects[old->object].name, error))
        return false;
    AbVec2 position;
    size_t object = SIZE_MAX;
    if (!game_shock_read(physical, "x", &position.x, error) ||
        !game_shock_read(physical, "y", &position.y, error) ||
        !game_factory_goal_circle(game, game_text("BLACKBIRD_FLOATING_PIG"), position,
                                  &object, error))
        return false;
    PreviewVialObject *row = old->owner->objects + object;
    if (!game->shockwaves ||
        !game_shock_transfer(game->shockwaves->objects + old->object,
                             game->shockwaves->objects + object, error) ||
        !game_vial_definition(game, object, &row->definition, error))
        return false;
    AbDataValue sprite = menu_route_field(row->definition.table, "sprite");
    float radius, margin, sprite_radius, life;
    AbLuaAssetSpriteBinding binding;
    if (sprite.kind != AB_DATA_STRING ||
        !game_vial_write(row, "preBubbleDefinition", original, error) ||
        !ab_classic_goals_set(dynamics.goals, object, true, error) ||
        !game_vial_write(row, "blinkTimer", menu_route_number(1e10f), error) ||
        !game_vial_write(row, "oinkTimer", menu_route_number(1e10f), error) ||
        !game_vial_write(row, "smileSprite", sprite, error) ||
        !game_vial_write(row, "spriteAffectedByVial", game_text("pink"), error) ||
        !game_vial_write(row, "vialEffectInProgress", game->enabled, error) ||
        !game_shock_read(row->definition.table, "radius", &radius, error) ||
        !game_shock_read(row->definition.table, "radiusMargin", &margin, error) ||
        !game_vial_pink_capture(row, radius - margin, error))
        return false;
    AbBodyMotion *body = NULL;
    if (!game_vial_body(row, &body, error) ||
        !ab_body_velocity_set(body, (AbVec2){0}) ||
        !ab_lua_asset_sprite_bind(&game->menu->sprites.provider, sprite.string,
                                  &binding, error) ||
        !binding.region || !(binding.region->width > 0) ||
        !game_shock_read(row->definition.table, "spriteRadius", &sprite_radius,
                         error) ||
        !game_shock_read(row->definition.table, "lifeSpan", &life, error))
        return ab_fail(error, "Invalid actual floating-pig sprite/body/lifetime");
    float scale = ((2 * sprite_radius) * 20) / binding.region->width;
    row->remove_entry = (AbDataEntry){
        menu_route_number(1), {.kind = AB_DATA_OBJECT, .object = &row->floating}};
    row->remove_functions = (AbDataTable){&row->remove_entry, 1, 1, NULL};
    row->removal_items[0] = (AbUiPointerValue){.kind = AB_UI_POINTER_FUNCTION,
                                               .function = (uintptr_t)&row->floating};
    row->removal_list = (AbBlockRemovalFunctions){row->removal_items, 2};
    if (!ab_classic_scene_set_scale(
            ab_classic_render_scene_objects(game->playfield.scene), object,
            (AbVec2){scale, scale}, error) ||
        !game_vial_add_function(row, &row->floating, error) ||
        !game_vial_write(
            row, "removeFunctions",
            (AbDataValue){.kind = AB_DATA_TABLE, .table = &row->remove_functions},
            error) ||
        !game_vial_write(row, "inBubble", game->enabled, error) ||
        !game_vial_write(row, "dontDrawBubble", game->enabled, error) ||
        !game_vial_write(row, "bubbleAntiGravityTimer", menu_route_number(life), error))
        return false;
    *output = row;
    return true;
}

static bool game_vial_pink_later(void *context, AbDataValue parameters,
                                 AbError *error) {
    (void)parameters;
    PreviewVialRequest *request = context;
    PreviewVialObject *old = request->target;
    PreviewGame *game = old->owner->game;
    if (!game->shockwaves ||
        !game_shock_audio(game->shockwaves, game_text("pigs_potion_bubbles_01"), error))
        return false;
    if (!game_vial_live(old))
        return true;
    AbDataValue effect = menu_route_field(request->vial_definition.table, "vialEffect");
    AbDataValue sprite;
    float unused_scale;
    PreviewVialObject *row = NULL;
    if (!game_vial_sprite(old, menu_route_field(effect.table, "pigFloatingSprite"),
                          &sprite, &unused_scale, error) ||
        !game_vial_pink_transform(old, &row, error))
        return false;
    return game_vial_write(row, "ignoreAllDamage", game->enabled, error) &&
           game_vial_write(row, "vialEffectInProgress", game->disabled, error) &&
           game_vial_write(row, "dontUpdateDamageSprite", game->enabled, error) &&
           game_vial_write(row, "disableSmiling", game->enabled, error) &&
           ab_classic_visuals_set_sprite(game->playfield.visuals, row->object,
                                         sprite.string, error) &&
           game_vial_write(row, "pinkVialSprite", sprite, error);
}

static bool game_vial_pink(PreviewVialRequest *request, bool *retain, AbError *error) {
    PreviewVialObject *row = request->target;
    AbDataTable *physical = game_vial_physical(row);
    if (!menu_route_truthy(menu_route_field(physical, "levelGoal")))
        return ab_fail(error, "Pink vial target is not a level goal");
    *retain = menu_route_truthy(menu_route_field(physical, "isPuffing")) ||
              menu_route_truthy(menu_route_field(physical, "inBubble"));
    if (*retain)
        return true;
    request->inflate = (AbDelayedFunction){request, game_vial_pink_later};
    return game_vial_write(row, "vialEffectInProgress", row->owner->game->enabled,
                           error) &&
           game_vial_transition(request, error) &&
           game_vial_visual(row, GAME_VIAL_PINK, 3, 4, false, error) &&
           ab_delayed_callbacks_add(row->owner->game->delayed, 1.3f, &request->inflate,
                                    (AbDataValue){0}, error);
}

#endif
