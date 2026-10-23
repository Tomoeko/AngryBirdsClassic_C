#ifndef AB_UI_FLAMES_APK_TEST_H
#define AB_UI_FLAMES_APK_TEST_H

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
    for (size_t index = 0; index < game->tail.hooks->count; ++index) {
        const AbGameTailHook *hook = game->tail.hooks->items + index;
        if (hook->object->identity == object &&
            (hook->updates->registered.kind != AB_OBJECT_UPDATE_NONE ||
             hook->updates->object.kind == AB_OBJECT_UPDATE_NONE))
            return ab_fail(error,
                           "Flame hook pruning lost its retained object property");
    }
    game_session_free(menu);
    menu->level_request_ready = false;
    puts("Actual flame: source zero update, shared callback/RNG, animated cached "
         "artwork, layer order and Metal capture, pause, world pruning and retained "
         "hook property.");
    return true;
}

#endif
