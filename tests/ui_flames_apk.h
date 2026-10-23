#ifndef AB_UI_FLAMES_APK_TEST_H
#define AB_UI_FLAMES_APK_TEST_H

typedef struct {
    AbClassicObjectHooks *registry;
    AbObjectUpdates updates[2];
    AbObjectUpdateFunction functions[2];
    size_t objects[2], calls[2];
    bool added;
} FlameSharedHooks;

static bool flame_secondary_hook(void *context, void *receiver, float seconds,
                                 AbError *error) {
    (void)receiver;
    (void)seconds;
    (void)error;
    ++((FlameSharedHooks *)context)->calls[1];
    return true;
}

static bool flame_primary_hook(void *context, void *receiver, float seconds,
                               AbError *error) {
    (void)receiver;
    (void)seconds;
    FlameSharedHooks *hooks = context;
    ++hooks->calls[0];
    if (!hooks->added) {
        hooks->added = true;
        return ab_classic_object_hooks_register(hooks->registry, hooks->objects[1],
                                                hooks->updates + 1, error);
    }
    return true;
}

static bool flame_shared_hooks(PreviewGame *game, AbClassicFlames *flames, size_t flame,
                               FlameSharedHooks *hooks, AbError *error) {
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    hooks->registry = ab_classic_flames_updates(flames);
    size_t count = 0;
    for (size_t object = 0; object < level.bodies->count && count < 2; ++object)
        if (object != flame && !level.removed_objects[object])
            hooks->objects[count++] = object;
    if (count != 2)
        return ab_fail(error, "Shared hook fixture lacks two live source objects");
    hooks->functions[0] = (AbObjectUpdateFunction){hooks, flame_primary_hook};
    hooks->functions[1] = (AbObjectUpdateFunction){hooks, flame_secondary_hook};
    for (size_t index = 0; index < 2; ++index) {
        AbObjectUpdateValue value = {AB_OBJECT_UPDATE_FUNCTION,
                                     {.function = hooks->functions + index}};
        hooks->updates[index] = (AbObjectUpdates){value, value};
    }
    if (!ab_classic_object_hooks_register(hooks->registry, hooks->objects[0],
                                          hooks->updates, error) ||
        !powerup_test_tick(game->menu, 2, error))
        return false;
    size_t first = hooks->calls[0], second = hooks->calls[1];
    return first && second && powerup_test_tick(game->menu, 1, error) &&
           hooks->calls[0] == first + 1 && hooks->calls[1] == second + 1;
}

static size_t flame_visible_command(const AbDrawList *list,
                                    const AbLuaAssetSpriteBinding *binding) {
    for (size_t index = 0; index < list->count; ++index) {
        const AbDrawCommand *command = list->commands + index;
        if (command->mask.texture || command->texture != binding->texture ||
            command->coordinates != AB_DRAW_CLIP)
            continue;
        bool match = true;
        float x = 0, y = 0;
        for (size_t vertex = 0; vertex < 4; ++vertex) {
            const AbDrawVertex *point = command->quad.vertices + vertex;
            match = match && point->u == binding->uv[vertex * 2] &&
                    point->v == binding->uv[vertex * 2 + 1];
            x += point->x;
            y += point->y;
        }
        if (match && x > -4 && x < 4 && y > -4 && y < 4)
            return index;
    }
    return SIZE_MAX;
}

static bool flame_widgets(PreviewMenu *menu, AbMetal *renderer, AbError *error) {
    if (!ability_test_episode_level(menu, 10, "BlackBird_1", error))
        return false;
    PreviewGame *game = menu->game;
    AbClassicBlockScriptView scripts = ab_classic_block_scripts_view(
        ab_classic_world_runtime_view(game->world).scripts);
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    AbClassicFlameView flame;
    size_t object = 0;
    while (object < level.bodies->count &&
           !ab_classic_flames_view(scripts.flames, object, &flame))
        ++object;
    if (object == level.bodies->count || !flame.applied || !game->tail.hooks ||
        !game->tail.hooks->count || !ambient_camera(menu, object, error))
        return ab_fail(error, "Actual BlackBird_1 has no applied live flame");
    AbSpan first = flame.state->sprite;
    bool advanced = false, captured = false;
    for (size_t frame = 0; frame < 90; ++frame) {
        if (!powerup_test_tick(menu, 1, error) ||
            !ab_classic_flames_view(scripts.flames, object, &flame))
            return false;
        advanced =
            advanced || first.size != flame.state->sprite.size ||
            (first.size && memcmp(first.data, flame.state->sprite.data, first.size));
        AbLuaAssetSpriteBinding binding;
        if (!ab_lua_asset_sprite_bind(&menu->sprites.provider, flame.state->sprite,
                                      &binding, error) ||
            !binding.region)
            return ab_fail(error, "Actual flame omitted its preloaded artwork");
        size_t command = flame_visible_command(menu->context.draw_list, &binding);
        if (!captured && command != SIZE_MAX) {
            const AbDrawList *list = menu->context.draw_list;
            AbClassicSceneView objects = ab_classic_scene_view(
                ab_classic_render_scene_objects(game->playfield.scene));
            AbDrawCommand base_command;
            AbDrawList base = {&base_command, 0, 1};
            const size_t index = 0;
            AbCameraSystemView camera =
                ab_classic_camera_view(game->playfield.camera).system;
            AbCoordinateCamera coordinates = camera.viewport->rendering;
            if (!ab_sprite_scene_draw(objects.items + object, 1, &index, coordinates,
                                      (uint32_t)menu->screen.x,
                                      (uint32_t)menu->screen.y, &base, error) ||
                base.count != 1)
                return ab_fail(error, "Actual flame base sprite is not visible");
            size_t before = 0;
            while (before < list->count &&
                   (list->commands[before].texture != base_command.texture ||
                    memcmp(&list->commands[before].quad, &base_command.quad,
                           sizeof(base_command.quad))))
                ++before;
            if (before >= command ||
                !ambient_capture(menu, renderer, "AB_FLAME_CAPTURE", error))
                return ab_fail(error,
                               "Actual flame did not follow its base scene layer");
            captured = true;
        }
    }
    if (!advanced || !captured)
        return ab_fail(error, "Actual flame did not animate visibly on Metal");
    if (!game_session_escape(menu, error) || !game->paused)
        return false;
    AbFlameState frozen = *flame.state;
    if (!powerup_test_tick(menu, 30, error) ||
        !ab_classic_flames_view(scripts.flames, object, &flame) ||
        memcmp(&frozen, flame.state, sizeof(frozen)) ||
        !game_session_escape(menu, error) || game->paused)
        return ab_fail(error, "Pause advanced the actual flame callback");
    FlameSharedHooks shared = {0};
    if (!flame_shared_hooks(game, scripts.flames, object, &shared, error))
        return ab_fail(error, "Live hook insertion failed beside actual FlameEffect");
    AbGameTailHook *retained = NULL;
    if (!game->tail.hooks->begin || !game->tail.hooks->next ||
        !game->tail.hooks->begin(game->tail.hooks->context, error))
        return ab_fail(error, "Flame omitted its shared live update registry");
    for (;;) {
        AbGameTailHook *hook;
        if (!game->tail.hooks->next(game->tail.hooks->context, &hook, error))
            return false;
        if (!hook)
            break;
        if (hook->object->identity == object)
            retained = hook;
    }
    if (!retained)
        return ab_fail(error, "Actual flame update was not registered");
    AbObjectPropertyAccess world = {
        .physics_object = {.kind = AB_DATA_TABLE, .table = &level.properties->world}};
    if (!ab_object_property_set(&world, level.bodies->objects[object].name,
                                (AbDataValue){0}, error) ||
        !powerup_test_tick(menu, 1, error))
        return false;
    const AbExtraDrawFunction *registered;
    if (!ab_extra_draws_find(game->extra_draws, flame.draw_key, &registered, error) ||
        registered)
        return ab_fail(error, "Absent flame retained its extra-draw registration");
    if (retained->updates->registered.kind != AB_OBJECT_UPDATE_NONE ||
        retained->updates->object.kind == AB_OBJECT_UPDATE_NONE)
        return ab_fail(error, "Flame pruning lost its retained object property");
    game_session_free(menu);
    menu->level_request_ready = false;
    puts("Actual flame: source zero update, shared callback/RNG, animated cached "
         "artwork, layer order and Metal capture, pause, world pruning and retained "
         "hook property.");
    return true;
}

#endif
