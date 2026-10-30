#ifndef AB_UI_VIAL_EFFECTS_APK_H
#define AB_UI_VIAL_EFFECTS_APK_H

static bool vial_effect_pair(PreviewGame *game, const char *callback, bool nearby,
                             size_t *vial, size_t *pig, AbError *error) {
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    float nearest = INFINITY;
    *vial = *pig = SIZE_MAX;
    for (size_t object = 0; object < level.bodies->count; ++object) {
        if (!level.bodies->objects[object].definition_name.size)
            continue;
        AbDataValue definition;
        if (!game_vial_definition(game, object, &definition, error))
            return false;
        if (!menu_route_equal(menu_route_field(definition.table, "onRemoved"),
                              game_text(callback)))
            continue;
        AbDataTable *physical = &level.properties->objects[object].physical;
        AbDataValue effect = menu_route_field(definition.table, "vialEffect");
        float x, y, radius;
        if (!game_shock_read(physical, "x", &x, error) ||
            !game_shock_read(physical, "y", &y, error) ||
            !game_shock_read(effect.table, "radius", &radius, error))
            return false;
        for (size_t rank = 0; rank < level.goal_count; ++rank) {
            size_t goal = level.goal_order[rank];
            AbDataTable *target = &level.properties->objects[goal].physical;
            float target_x, target_y;
            if (!game_shock_read(target, "x", &target_x, error) ||
                !game_shock_read(target, "y", &target_y, error))
                return false;
            target_x -= x;
            target_y -= y;
            float distance = target_x * target_x + target_y * target_y;
            if ((!nearby || distance < radius * radius) && distance < nearest) {
                nearest = distance;
                *vial = object;
                *pig = goal;
            }
        }
    }
    return *pig != SIZE_MAX || ab_fail(error, "Vial fixture lacks an authored pair");
}

static bool vial_effect_widgets(PreviewMenu *menu, AbMetal *renderer, AbError *error) {
    const char *const levels[] = {"BlackBird_31", "BlackBird_43"};
    for (size_t scene = 0; scene < 2; ++scene) {
        if (!ability_test_episode_level(menu, 10, levels[scene], error))
            return false;
        PreviewGame *game = menu->game;
        AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
        AbClassicWorldRuntimeView world = ab_classic_world_runtime_view(game->world);
        size_t vial, pig;
        if (!game->vials || !game->puffers || !game->shockwaves ||
            !vial_effect_pair(game, "blackBirdOrangeVialRemoved", true, &vial, &pig,
                              error))
            return ab_fail(error, "Actual vial effect owners/pair were not bound");
        AbDataTable *physical = &level.properties->objects[pig].physical;
        size_t body = level.bodies->objects[pig].physics.body;
        AbSpan name = level.bodies->objects[pig].name;
        if (!ab_string_map_set(world.dead_blocks, level.bodies->objects[vial].name,
                               (uintptr_t)(world.physical_values + vial), true, error))
            return false;
        bool queued = false, transitioned = false, inflated = false, shrinking = false;
        float largest = 0;
        size_t notices = 0;
        AbEventListener listener = {menu_span("OrangeVialFixture"), &notices,
                                    shockwave_event};
        if (!ab_event_manager_add(menu->events,
                                  menu_event(menu, "EID_BIRD_SPECIALTY_TRIGGERED"),
                                  &listener, error))
            return false;
        for (size_t frame = 0; frame < 720 && !level.removed_objects[pig]; ++frame) {
            if (!game_eagle_test_frame(menu, error))
                return false;
            queued |=
                menu_route_field(physical, "vialEffectQueue").kind == AB_DATA_TABLE;
            transitioned |=
                menu_route_truthy(menu_route_field(physical, "dontUpdateDamageSprite"));
            if (!menu_route_equal(menu_route_field(physical, "spriteAffectedByVial"),
                                  game_text("orange")))
                continue;
            float radius = level.bodies->objects[pig].physics.dimensions.radius;
            if (!inflated) {
                inflated = true;
                largest = radius;
                if (!physics_replay_capture(menu, renderer, error))
                    return false;
                AbClassicCharacterView characters = ab_classic_characters_view(
                    ab_classic_game_runtime_view(game->game).characters);
                if (!(radius > 1) || level.bodies->objects[pig].physics.body != body ||
                    !characters.bindings[pig].goal.disable_smiling ||
                    characters.animations->objects[pig].goal.blink_timer < 9998 ||
                    menu_route_field(physical, "blinkSprite").kind != AB_DATA_NIL ||
                    menu_route_field(physical, "vialEffectQueue").kind != AB_DATA_NIL ||
                    (scene == 0 && !game_eagle_test_capture(menu, renderer,
                                                            "AB_VIAL_CAPTURE", error)))
                    return ab_fail(
                        error, "Orange transformation lost live animation/body state");
            }
            shrinking |= menu_route_field(physical, "deathTimer").kind != AB_DATA_NIL &&
                         radius < largest;
        }
        uintptr_t value;
        bool present;
        if (!queued || !transitioned || !inflated || !shrinking || notices ||
            !level.removed_objects[pig] || ab_world_has_body(level.world, body) ||
            !menu_route_truthy(menu_route_field(physical, "deflated")) ||
            !ab_string_map_get(
                ab_classic_goals_map(ab_classic_dynamics_view(world.dynamics).goals),
                name, &value, &present, error) ||
            present)
            return ab_fail(
                error,
                "Orange vial scene %zu failed queue/transition/inflate/retirement",
                scene);
        if (!ab_event_manager_remove(menu->events,
                                     menu_event(menu, "EID_BIRD_SPECIALTY_TRIGGERED"),
                                     &listener, error))
            return false;
        game_session_free(menu);
        menu->level_request_ready = false;
    }
    puts("Actual Orange vials on both crash levels: canonical destruction queue, "
         "delayed particles/transition, live blink/smile state, inflated pig fixtures, "
         "deflation, real goal/body removal and Metal drawing.");
    return true;
}

static bool vial_blue_widgets(PreviewMenu *menu, AbMetal *renderer, AbError *error) {
    const char *const scenes[] = {"BlackBird_9", "BlackBird_16", "BlackBird_43"};
    for (size_t scene = 0; scene < 3; ++scene) {
        if (!ability_test_episode_level(menu, 10, scenes[scene], error))
            return false;
        PreviewGame *game = menu->game;
        AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
        AbClassicWorldRuntimeView world = ab_classic_world_runtime_view(game->world);
        size_t vial, pig;
        if (!vial_effect_pair(game, "blackBirdBlueVialRemoved", true, &vial, &pig,
                              error))
            return false;
        size_t initial = level.bodies->count,
               body = level.bodies->objects[pig].physics.body;
        if (!ab_string_map_set(world.dead_blocks, level.bodies->objects[vial].name,
                               (uintptr_t)(world.physical_values + vial), true, error))
            return false;
        bool spinning = false, transition = false, split = false;
        for (size_t frame = 0; frame < 360; ++frame) {
            if (!game_eagle_test_frame(menu, error))
                return false;
            const AbDataTable *physical = &level.properties->objects[pig].physical;
            spinning |=
                menu_route_field(physical, "blueVialSpinSpeed").kind == AB_DATA_NUMBER;
            transition |=
                menu_route_truthy(menu_route_field(physical, "dontUpdateDamageSprite"));
            if (!split && game->factory.used >= 2) {
                split = true;
                AbClassicSceneView draw = ab_classic_scene_view(
                    ab_classic_render_scene_objects(game->playfield.scene));
                if (!level.removed_objects[pig] &&
                    level.bodies->objects[pig].physics.body != body)
                    return ab_fail(error,
                                   "Blue cloning replaced the original native body");
                for (size_t object = initial; object < level.bodies->count; ++object) {
                    AbDataValue definition;
                    float radius;
                    if (!game_vial_definition(game, object, &definition, error) ||
                        !game_shock_read(definition.table, "radius", &radius, error) ||
                        level.bodies->objects[object].physics.dimensions.radius !=
                            radius * .8f ||
                        draw.items[object].pose.scale.x != .8f ||
                        menu_route_field(&level.properties->objects[object].physical,
                                         "gameObjectScript")
                                .kind != AB_DATA_TABLE)
                        return ab_fail(
                            error, "Blue clone lost actual scale/fixture/script state");
                }
                if (!physics_replay_focus(menu, pig, error) ||
                    !physics_replay_capture(menu, renderer, error) ||
                    (!scene && !game_eagle_test_capture(menu, renderer,
                                                        "AB_BLUE_VIAL_CAPTURE", error)))
                    return false;
            }
        }
        if (!spinning || !transition || !split || level.world->failed)
            return ab_fail(error,
                           "Actual Blue-vial scene did not spin, transition and clone");
        game_session_free(menu);
        menu->level_request_ready = false;
    }
    puts("Three actual Blue-vial scenes: canonical destruction, spin/force updates, "
         "delayed transition and cloning, original-body retention, real circle "
         "fixtures/scripts, Metal and continued physics.");
    return true;
}

static bool vial_pink_contents(PreviewVialObject *row, bool released, AbError *error) {
    AbClassicLevelView level = ab_classic_level_view(row->owner->game->playfield.level);
    for (size_t rank = 0; rank < row->captured.count; ++rank) {
        size_t index = row->captured_indices[rank];
        AbDataEntry *entry = row->captured_entries + index;
        if (entry->value.kind == AB_DATA_NIL)
            continue;
        size_t object = row->captured_indices[row->captured.count + index];
        AbDataTable *physical = &level.properties->objects[object].physical;
        if (entry->value.kind != AB_DATA_TABLE || entry->value.table != physical ||
            menu_route_truthy(menu_route_field(physical, "inBubble")) == released ||
            menu_route_truthy(menu_route_field(physical, "dontDrawBubble")) == released)
            return ab_fail(error, "Floating contents lost retained physical identity");
        if (level.removed_objects[object])
            continue;
        size_t body = level.frame->objects[object].body;
        for (size_t fixture = 0; fixture < level.world->fixture_slot_count; ++fixture)
            if (level.world->fixture_alive[fixture] &&
                level.world->storage.fixtures[fixture].body == body &&
                level.world->storage.fixtures[fixture].filter.mask !=
                    (released ? UINT16_MAX : 0))
                return ab_fail(error, "Floating contents have an incorrect mask");
    }
    return true;
}

static bool vial_pink_widgets(PreviewMenu *menu, AbMetal *renderer, AbError *error) {
    const char *const scenes[] = {"BlackBird_30", "BlackBird_40", "BlackBird_44",
                                  "FlockFavorites23", "FlockFavorites24"};
    for (size_t scene = 0; scene < 5; ++scene) {
        if (!ability_test_episode_level(menu, scene < 3 ? 10 : 11, scenes[scene],
                                        error))
            return false;
        PreviewGame *game = menu->game;
        AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
        AbClassicWorldRuntimeView world = ab_classic_world_runtime_view(game->world);
        size_t vial, pig;
        if (!game->vials || !vial_effect_pair(game, "blackBirdPinkVialRemoved", false,
                                              &vial, &pig, error))
            return false;
        /* Move the authored vial to its nearest authored pig, then exercise
         * ordinary destruction. Other bodies retain their authored layout. */
        AbVec2 position;
        if (!game_shock_read(&level.properties->objects[pig].physical, "x", &position.x,
                             error) ||
            !game_shock_read(&level.properties->objects[pig].physical, "y", &position.y,
                             error) ||
            !ab_classic_level_set_position(game->playfield.level, vial, position,
                                           error))
            return false;
        size_t initial = level.bodies->count, floating = SIZE_MAX;
        bool transitioned = false, released = false;
        if (!ab_string_map_set(world.dead_blocks, level.bodies->objects[vial].name,
                               (uintptr_t)(world.physical_values + vial), true, error))
            return false;
        for (size_t frame = 0; frame < 480; ++frame) {
            if (!game_eagle_test_frame(menu, error))
                return false;
            transitioned |= menu_route_truthy(menu_route_field(
                &level.properties->objects[pig].physical, "dontUpdateDamageSprite"));
            if (floating == SIZE_MAX) {
                for (size_t object = initial; object < level.bodies->count; ++object)
                    if (menu_route_equal(
                            menu_route_field(
                                &level.properties->objects[object].physical,
                                "definition"),
                            game_text("BLACKBIRD_FLOATING_PIG"))) {
                        floating = object;
                        break;
                    }
                if (floating == SIZE_MAX)
                    continue;
                PreviewVialObject *row = game->vials->objects + floating;
                if (!level.removed_objects[pig] || !row->captured.count ||
                    !vial_pink_contents(row, false, error) ||
                    !physics_replay_focus(menu, floating, error) ||
                    !physics_replay_capture(menu, renderer, error) ||
                    (!scene && !game_eagle_test_capture(menu, renderer,
                                                        "AB_PINK_VIAL_CAPTURE", error)))
                    return error->message[0]
                               ? false
                               : ab_fail(error, "Pink fixture did not capture objects");
                /* Alternate expiry with early ordinary destruction. Both must
                 * execute removeFloatingPig before losing its live body. */
                if (scene % 2 &&
                    !ab_string_map_set(
                        world.dead_blocks, level.bodies->objects[floating].name,
                        (uintptr_t)(world.physical_values + floating), true, error))
                    return false;
            }
            if (!released && level.removed_objects[floating]) {
                released = true;
                if (!vial_pink_contents(game->vials->objects + floating, true, error))
                    return false;
            }
        }
        if (!transitioned || floating == SIZE_MAX || !released || level.world->failed)
            return ab_fail(error, "Pink vial failed transition/capture/release");
        game_session_free(menu);
        menu->level_request_ready = false;
    }
    puts("Five actual Pink-vial scenes: real scored pig replacement, floating goal "
         "factory, captured table identities, disabled filters, timed and early "
         "destruction cleanup, Metal and continued physics.");
    return true;
}

static bool vial_chain_destroy(PreviewGame *game, size_t pig, size_t initial,
                               unsigned mask, AbError *error) {
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    AbClassicWorldRuntimeView world = ab_classic_world_runtime_view(game->world);
    AbVec2 position;
    if (!game_shock_read(&level.properties->objects[pig].physical, "x", &position.x,
                         error) ||
        !game_shock_read(&level.properties->objects[pig].physical, "y", &position.y,
                         error))
        return false;
    unsigned found = 0;
    for (size_t object = 0; object < initial; ++object) {
        if (level.removed_objects[object])
            continue;
        AbDataValue definition;
        if (!game_vial_definition(game, object, &definition, error))
            return false;
        AbDataValue removed = menu_route_field(definition.table, "onRemoved");
        unsigned type =
            menu_route_equal(removed, game_text("blackBirdOrangeVialRemoved")) ? 1u
            : menu_route_equal(removed, game_text("blackBirdBlueVialRemoved")) ? 2u
                                                                               : 0u;
        if (!(mask & type))
            continue;
        found |= type;
        if (!ab_classic_level_set_position(game->playfield.level, object, position,
                                           error) ||
            !ab_string_map_set(world.dead_blocks, level.bodies->objects[object].name,
                               (uintptr_t)(world.physical_values + object), true,
                               error))
            return false;
    }
    if (found != mask)
        return ab_fail(error,
                       "Compound fixture lacks its requested authored vial types");
    /* Drain actual removals before stepping the deliberately overlapping vials;
     * their artificial initial pileup must not kill the intended pig first. */
    return ab_classic_removals_run(ab_classic_dynamics_view(world.dynamics).removals,
                                   error);
}

static bool vial_chain_widgets(PreviewMenu *menu, AbMetal *renderer, AbError *error) {
    for (size_t variant = 0; variant < 3; ++variant) {
        if (!ability_test_episode_level(menu, 10, "BlackBird_43", error))
            return false;
        PreviewGame *game = menu->game;
        AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
        if (!game->vials || !level.goal_count)
            return ab_fail(error, "Compound vial scene lacks its authored owners");
        size_t pig = level.goal_order[0], initial = level.bodies->count;
        unsigned first = variant == 1 ? 2u : variant == 2 ? 1u : 3u;
        if (!vial_chain_destroy(game, pig, initial, first, error))
            return false;
        bool transformed = false;
        for (size_t frame = 0; frame < 720; ++frame) {
            /* Blue first queues Orange before cloning; Orange first can discard
             * the waiting Blue effect. Both use actual retained vial definitions. */
            size_t delay = variant == 1 ? 90 : 30;
            if (variant && frame == delay &&
                !vial_chain_destroy(game, pig, initial, 3u ^ first, error))
                return false;
            if (!game_eagle_test_frame(menu, error) ||
                (frame % 120 == 0 && !physics_replay_capture(menu, renderer, error)))
                return false;
            transformed |= menu_route_field(&level.properties->objects[pig].physical,
                                            "spriteAffectedByVial")
                               .kind == AB_DATA_STRING;
        }
        /* Orange explicitly discards its remaining effect queue, so a mixed
         * sequence need not create clones/floating replacements. */
        if (level.world->failed || (!transformed && !game->factory.used) ||
            level.bodies->count > level.bodies->capacity)
            return ab_fail(error, "Compound vial factory/physics continuation failed");
        game_session_free(menu);
        menu->level_request_ready = false;
    }
    puts("Actual BlackBird_43 vials in three orders: simultaneous, Blue before Orange, "
         "Orange before Blue; real effect queues/factories/joints, Metal and 720 "
         "frames.");
    return true;
}

#endif
