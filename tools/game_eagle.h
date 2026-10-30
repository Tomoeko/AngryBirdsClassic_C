#ifndef AB_GAME_EAGLE_H
#define AB_GAME_EAGLE_H

static bool game_eagle_clock(void *context, AbCalendarTime *output, AbError *error) {
    (void)context;
    return ab_calendar_time_now(output, error);
}

static bool game_eagle_font(void *context, AbUiPointerValue *output, AbError *error) {
    (void)context;
    (void)error;
    *output = menu_text("FONT_BASIC");
    return true;
}

static bool game_eagle_transitioning(PreviewGame *game) {
    return game->active.items && game->active.capacity && game->active.items[0];
}

static void game_eagle_refresh(PreviewEagle *eagle) {
    PreviewGame *game = eagle->game;
    const char *const names[] = {"levelName",
                                 "currentEpisode",
                                 "hasIaps",
                                 "g_freeEagleUnlockStatus",
                                 "highscores",
                                 "eagleBaitLaunched",
                                 "g_level_completing",
                                 "levelCompleted",
                                 "eagleUsedInCurrentLevel",
                                 "eagleLockedTime"};
    const AbDataValue values[] = {
        game->level_name,
        game->menu->level_request.episode_id,
        game->enabled,
        menu_route_field(game->menu->progress.globals, "g_freeEagleUnlockStatus"),
        {.kind = AB_DATA_TABLE,
         .table = (AbDataTable *)ab_settings_statistics_root(game->menu->highscores)},
        game->eagle_bait_launched,
        {.kind = AB_DATA_BOOLEAN, .bits = game->finish.complete.completing},
        {.kind = AB_DATA_BOOLEAN, .bits = game->finish.complete.completed},
        game->eagle_bait_launched,
        menu_route_number(3600)};
    for (size_t index = 0; index < sizeof(names) / sizeof(*names); ++index)
        eagle->fields[index] = (AbDataEntry){game_text(names[index]), values[index]};
    eagle->globals =
        (AbDataTable){eagle->fields, sizeof(values) / sizeof(*values), 16, NULL};
}

static bool game_eagle_query(void *context, AbUiEagleQuery query,
                             AbUiPointerValue *output, AbError *error) {
    PreviewEagle *eagle = context;
    game_eagle_refresh(eagle);
    bool infinite = eagle->game->menu->infinite_powerups;
    if (query == AB_UI_EAGLE_AVAILABLE && game_eagle_transitioning(eagle->game)) {
        *output = result_boolean(false);
        return true;
    }
    if (infinite &&
        (query == AB_UI_EAGLE_ENABLED || query == AB_UI_EAGLE_SUBSYSTEM_ENABLED ||
         query == AB_UI_EAGLE_SETTINGS_ENABLED || query == AB_UI_EAGLE_AVAILABLE)) {
        /* Local command-line policy only. Never forge a purchase or overwrite
         * inventory/cooldown settings; source level/visibility gates still run. */
        *output = result_boolean(true);
        return true;
    }
    return ab_ui_eagle_services_query(&eagle->services, query, output, error);
}

static bool game_eagle_power(void *context, const AbDataTable **output,
                             AbError *error) {
    PreviewEagle *eagle = context;
    game_eagle_refresh(eagle);
    return ab_ui_eagle_services_power(&eagle->services, output, error);
}

static bool game_eagle_used(void *context, AbUiPointerValue free_enabled,
                            AbDataValue *output, AbError *error) {
    PreviewEagle *eagle = context;
    if (eagle->game->menu->infinite_powerups) {
        *output = (AbDataValue){0};
        return true;
    }
    return ab_ui_eagle_services_used(&eagle->services, free_enabled, output, error);
}

static bool game_eagle_layout(PreviewGame *game, AbError *error) {
    PreviewEagle *eagle = game->eagle;
    if (!eagle || !eagle->entered || !game->pause)
        return true;
    float scale;
    if (!ab_ui_scaling_get(&game->menu->scaling, &scale, error))
        return false;
    AbUiWidget *pause = &game->pause->button.widget;
    AbUiWidget *button = &eagle->button.widget;
    button->frame->fields.x = menu_number(
        menu_real(pause->state.image.resource_pivot_x.number_bits) * scale * 3.2f +
        game->pause->margin.x);
    button->frame->fields.y = pause->frame->fields.y;
    return ab_ui_scaling_set_to(&game->menu->scaling, &button->frame->fields, error) &&
           ab_ui_widget_lifecycle(button, AB_UI_FRAME_LAYOUT, error);
}

static void game_eagle_free(PreviewGame *game) {
    PreviewEagle *eagle = game->eagle;
    if (!eagle)
        return;
    AbError ignored = {0};
    if (eagle->entered && !game->menu->context.failed)
        (void)ab_ui_widget_lifecycle(&eagle->button.widget, AB_UI_FRAME_EXIT, &ignored);
    if (eagle->button.initialized)
        (void)ab_ui_mighty_eagle_button_detach(&eagle->button, &ignored);
    if (game->menu->fonts.getters == &eagle->font) {
        game->menu->fonts.getters = eagle->previous_getters;
        game->menu->fonts.getter_count = eagle->previous_getter_count;
    }
    free(eagle);
    game->eagle = NULL;
}

static bool game_eagle_open(PreviewGame *game, AbError *error) {
    PreviewEagle *eagle = calloc(1, sizeof(*eagle));
    if (!eagle)
        return ab_fail(error, "Cannot allocate Mighty Eagle controls");
    game->eagle = eagle;
    eagle->game = game;
    eagle->bait = eagle->bird = SIZE_MAX;
    game_eagle_refresh(eagle);
    eagle->services = (AbUiEagleServices){
        .progress = game->menu->progress,
        .settings = ab_settings_statistics_root(game->menu->settings),
        .level = &game->menu->level_request,
        .clock = {game, game_eagle_clock}};
    eagle->services.progress.globals = &eagle->globals;
    eagle->font = (AbUiFontGetter){game, game_eagle_font};
    eagle->previous_getters = game->menu->fonts.getters;
    eagle->previous_getter_count = game->menu->fonts.getter_count;
    if (eagle->previous_getter_count)
        return ab_fail(error, "Mighty Eagle font getter requires shared registration");
    game->menu->fonts.getters = &eagle->font;
    game->menu->fonts.getter_count = 1;
    EagleTextClip *clip = &eagle->timer_clip;
    AbUiMightyEagleHost host = {
        .scaling = &game->menu->scaling,
        .fonts = &game->menu->fonts,
        .events = game->menu->events,
        .event_ids = game->menu->event_data.data.globals,
        .globals = &eagle->globals,
        .basic_font_getter = {.kind = AB_UI_POINTER_FUNCTION,
                              .function = (uintptr_t)&eagle->font},
        .clock = {game, game_eagle_clock},
        .timer_storage = {clip->lines, clip->names, 32, clip->wrapped, clip->text, 8,
                          clip->codes, 256, clip->bytes, 2048},
        .context = eagle,
        .query = game_eagle_query,
        .episode_power = game_eagle_power,
        .used_time = game_eagle_used};
    AbUiImageButtonArguments arguments = {.image.frame.name = menu_text("eagleButton"),
                                          .return_value =
                                              menu_text("ME_BUTTON_CLICKED"),
                                          .activate_on_release = result_boolean(true),
                                          .click_sound = menu_text("menu_confirm")};
    if (!ab_ui_mighty_eagle_button_init(&eagle->button, &game->menu->context,
                                        &arguments, menu_text("INGAME"), host, error))
        return false;
    eagle->button.widget.padding[2] = menu_number(50);
    if (!ab_ui_widget_lifecycle(&eagle->button.widget, AB_UI_FRAME_ENTRY, error))
        return false;
    eagle->entered = true;
    return game_eagle_layout(game, error);
}

static bool game_eagle_launch(PreviewGame *game, AbError *error) {
    PreviewEagle *eagle = game->eagle;
    AbClassicGameRuntimeView runtime = ab_classic_game_runtime_view(game->game);
    AbClassicWorldRuntimeView world = ab_classic_world_runtime_view(game->world);
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    const AbDataTable *power = NULL;
    if (!eagle || eagle->bait != SIZE_MAX || game_eagle_transitioning(game) ||
        !game_eagle_power(eagle, &power, error))
        return ab_fail(error, "Invalid Mighty Eagle bait activation");
    AbDataValue name = menu_route_field(power, "definition");
    if (!menu_route_truthy(name))
        name = game_text("BaitSardine");
    if (!menu_route_equal(name, game_text("BaitSardine")))
        return ab_fail(error,
                       "Episode Mighty Power requires its recovered constructor");
    if (!ab_classic_sling_cancel_drag(game->playfield.sling, error))
        return false;
    if (game->flying.kind != AB_DATA_NIL &&
        menu_route_truthy(game->globals.specialty_available) &&
        !ab_classic_bird_specialty_trigger_flying(runtime.specialty, 0, error))
        return false;
    size_t cursor = 0;
    AbStringMapEntry hat;
    for (;;) {
        if (!ab_string_map_next(world.hats, &cursor, &hat, error))
            return false;
        if (!hat.present)
            break;
        if (!game_hat_release(game, hat.key, error))
            return false;
    }
    AbClassicSlingView sling = ab_classic_sling_view(game->playfield.sling);
    float index = sling.current_bird_index;
    bool removed_current = false;
    if (game->shot.current_bird_name) {
        size_t object = 0;
        while (object < level.bodies->count &&
               (strlen(game->shot.current_bird_name) !=
                    level.bodies->objects[object].name.size ||
                memcmp(game->shot.current_bird_name,
                       level.bodies->objects[object].name.data,
                       level.bodies->objects[object].name.size)))
            ++object;
        if (object < level.bodies->count &&
            !game_eagle_true(game_eagle_physical(game, object), "shot")) {
            if (!ab_classic_birds_remove(runtime.birds, object, (AbDataValue){0},
                                         error))
                return false;
            removed_current = true;
        }
    }
    if (!removed_current && sling.transfer_object != SIZE_MAX) {
        if (!ab_classic_birds_remove(runtime.birds, sling.transfer_object,
                                     (AbDataValue){0}, error))
            return false;
        index++;
    }
    for (;;) {
        size_t object;
        index++;
        if (!ab_classic_sling_next_bird(game->playfield.sling, index, &object, error))
            return false;
        if (object == SIZE_MAX)
            break;
        if (!ab_classic_birds_remove(runtime.birds, object, (AbDataValue){0}, error))
            return false;
    }
    if (!ab_classic_sling_clear_selection(game->playfield.sling, error))
        return false;
    eagle->launch_fields[0] = (AbDataEntry){
        game_text("id"), menu_event(game->menu, "EID_EAGLE_BAIT_LAUNCHED")};
    eagle->launch_fields[1] =
        (AbDataEntry){game_text("isDemoLevel"),
                      {.kind = AB_DATA_BOOLEAN,
                       .bits = menu_route_equal(game->menu->level_request.episode_id,
                                                game_text("EagleDemo"))}};
    eagle->launch_event = (AbDataTable){eagle->launch_fields, 2, 2, NULL};
    if (!ab_event_manager_notify(game->menu->events, &eagle->launch_event, error) ||
        !ab_camera_system_goto_snap(
            ab_classic_camera_system(game->playfield.camera), 0, true, true,
            &(AbCameraMove){.duration = 1, .duration_present = true}, error))
        return false;
    game->eagle_timer.sound_played = (AbDataValue){0};
    game->eagle_bait_launched = game->enabled;
    game->finish.complete.eagle_launched = true;
    game_eagle_cancel_ending(game);
    game->globals.specialty_available = game->disabled;
    AbObjectDefinition definition;
    if (!ab_object_definition_open(level.catalog, name.string, NULL, &definition,
                                   error))
        return false;
    AbDataValue sprite = ab_object_property(&definition, menu_span("sprite"));
    AbSpriteAtlasBinding binding;
    if (sprite.kind != AB_DATA_STRING ||
        !game->menu->sprites.provider.sprite(game->menu->sprites.provider.context,
                                             sprite.string, &binding))
        return ab_fail(error, "Missing actual sardine bait sprite");
    AbVec2 position = ab_classic_sling_start_pose(game->playfield.sling).base;
    position.x = (position.x * 20) * .05f;
    position.y = (position.y * 20) * .05f;
    if (!ab_classic_level_create_bait_box(
            game->playfield.level, menu_span("BaitSardine_1"), name.string, position,
            (AbVec2){binding.sprite.region->width, binding.sprite.region->height},
            &eagle->bait, error))
        return false;
    size_t object = eagle->bait;
    AbDataTable *physical = game_eagle_physical(game, object);
    game->shot.bird_count++;
    if (!ab_classic_render_scene_create_object(
            game->playfield.scene, game->playfield.visuals, object, sprite.string,
            menu_real(definition.z_order.bits), error) ||
        !ab_classic_behaviors_create_object(
            game->playfield.behaviors, object,
            ab_object_property(&definition, menu_span("behaviors")), level.catalog_data,
            error) ||
        !game_eagle_number(physical, "startNumber", game->shot.bird_count, error) ||
        !game_eagle_number(physical, "animTimer", 3, error) ||
        !game_eagle_number(physical, "jumpTimer", 3, error) ||
        !game_eagle_write(physical, "outsideBoundaries", game->disabled, error) ||
        !game_eagle_write(physical, "isMightyBait", game->enabled, error) ||
        !game_eagle_write(physical, "recordTrajectory", game->disabled, error) ||
        !game_eagle_register(game, object, true, error) ||
        !game_eagle_add_bird(game, object, error))
        return false;
    game->eagle_timer.timer = (AbDataValue){0};
    game->tail.mighty_timer_present = true;
    return ab_classic_sling_reload_after(game->playfield.sling,
                                         game->shot.bird_count - 1, .1f, error) &&
           ab_ui_mighty_eagle_button_visible(&eagle->button, result_boolean(false),
                                             error);
}

static bool game_eagle_pointer(PreviewGame *game, AbSpan action, AbVec2 point,
                               bool *consumed, AbError *error) {
    *consumed = false;
    if (!game->eagle || game->paused || game->ended)
        return true;
    game_eagle_refresh(game->eagle);
    AbUiPointerReply reply;
    if (!ab_ui_widget_pointer(&game->eagle->button.widget, action, point, &reply,
                              error))
        return false;
    *consumed = reply.values[0].kind != AB_UI_POINTER_NONE;
    if (!menu_equal(reply.values[0], "ME_BUTTON_CLICKED"))
        return true;
    if (game_eagle_transitioning(game) || game->menu->reward)
        return true;
    game->input.skip_input = game->enabled;
    game->input.eagle_click_time = game->globals.time;
    AbUiPointerValue enabled, available;
    if (!game_eagle_query(game->eagle, AB_UI_EAGLE_ENABLED, &enabled, error) ||
        !game_eagle_query(game->eagle, AB_UI_EAGLE_AVAILABLE, &available, error))
        return false;
    if (!ab_ui_pointer_value_truthy(&enabled)) {
        /* The external store is unavailable in this offline application. Keep
         * the live level and source purchase flags intact. */
        fprintf(stderr, "Mighty Eagle purchase service unavailable; --infinite enables "
                        "local use\n");
        return true;
    }
    return !ab_ui_pointer_value_truthy(&available) || game_eagle_launch(game, error);
}

static bool game_eagle_draw(PreviewGame *game, float seconds, AbError *error) {
    if (!game->eagle || game->paused || game->ended)
        return true;
    PreviewEagle *eagle = game->eagle;
    game_eagle_refresh(eagle);
    if (!ab_ui_mighty_eagle_button_visible(&eagle->button, result_boolean(true),
                                           error) ||
        !ab_ui_widget_update(&eagle->button.widget, seconds,
                             menu_real(game->globals.time.bits), error))
        return false;
    return ab_ui_widget_draw(&eagle->button.widget,
                             &(AbUiDrawInput){.transform.scale = {1, 1}, .present = 12},
                             error);
}

#endif
