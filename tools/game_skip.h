#ifndef AB_GAME_SKIP_H
#define AB_GAME_SKIP_H

/* The recovered GameHud fast-forward component. Its ability/pause graph has
 * separate ownership; this binds the real SkipButton and its actual event. */
static bool game_skip_query(void *context, AbUiPointerValue *output, AbError *error) {
    (void)error;
    PreviewGame *game = context;
    *output = (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN,
                                 .boolean = game->finish.complete.completing};
    return true;
}

static bool game_skip_goals(void *context, AbDataTable *event, AbError *error) {
    (void)event;
    (void)error;
    PreviewGame *game = context;
    if (!game->finish.complete.eagle_launched &&
        !menu_route_truthy(game->input.using_gestures)) {
        game->skip.widget.frame->fields.visible =
            (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN, .boolean = true};
        game->skip.state = AB_UI_SKIP_APPEAR;
    }
    return true;
}

static bool game_skip_layout(PreviewGame *game, AbError *error) {
    AbUiWidget *widget = &game->skip.widget;
    float scale;
    if (!ab_ui_image_state_refresh(&widget->state.image, &game->menu->context.images,
                                   error) ||
        !ab_ui_scaling_get(&game->menu->scaling, &scale, error))
        return false;
    float width = menu_real(widget->frame->fields.w.number_bits);
    float height = menu_real(widget->frame->fields.h.number_bits);
    widget->frame->fields.x = menu_number(game->screen.x - width * 0.55f * scale);
    widget->frame->fields.y = menu_number(game->screen.y - height * 0.55f * scale);
    return ab_ui_scaling_set_to(&game->menu->scaling, &widget->frame->fields, error);
}

static bool game_skip_open(PreviewGame *game, AbError *error) {
    PreviewMenu *menu = game->menu;
    const AbDataTable *groups =
        ab_dynamic_asset_requirements_get(&menu->requirements, menu_span("gameHud"));
    if (!groups)
        return ab_fail(error, "Missing actual GameHud resource requirements");
    /* HUD_INGAME_1 belongs to HUD, outside the gameHud dynamic list. The
     * original createAssets54058 and cutscene return1223 load it separately. */
    if (!game_group(menu, "HUD", error) ||
        !ab_android_assets_manage(menu->planning, groups, true, ab_asset_resources_host,
                                  menu->resources, error) ||
        !ab_asset_textures_sync(menu->textures,
                                ab_asset_resources_owners(menu->resources), error))
        return false;
    AbUiWidget *widget = &game->skip.widget;
    if (!ab_ui_hud_skip_button_init(&game->skip, &game->menu->context,
                                    (AbUiWidgetChildren){0}, game, game_skip_query,
                                    error))
        return false;
    widget->frame->fields.name = menu_text("skip_button");
    widget->frame->fields.visible = (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN};
    widget->state.activate_on_release =
        (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN, .boolean = true};
    widget->state.return_value = menu_text("SKIP_TO_LEVEL_END");
    game->goals_listener = (AbEventListener){.name = menu_span("ApplicationHudGoals"),
                                             .context = game,
                                             .event_triggered = game_skip_goals};
    if (!ab_ui_image_button_state_set_image(
            &widget->state, menu_text("BUTTON_NEXTLEVEL_SKIP"), (AbUiPointerValue){0},
            &game->menu->context.images, error) ||
        !game_skip_layout(game, error) ||
        !ab_ui_widget_lifecycle(widget, AB_UI_FRAME_ENTRY, error) ||
        !ab_event_manager_add(game->menu->events, game_text("EID_LEVEL_GOALS_CLEARED"),
                              &game->goals_listener, error))
        return false;
    game->goals_listening = true;
    return true;
}

static void game_skip_free(PreviewGame *game) {
    AbError ignored = {0};
    if (game->goals_listening)
        (void)ab_event_manager_remove(game->menu->events,
                                      game_text("EID_LEVEL_GOALS_CLEARED"),
                                      &game->goals_listener, &ignored);
    if (game->skip.widget.initialized && !game->menu->context.failed)
        (void)ab_ui_widget_lifecycle(&game->skip.widget, AB_UI_FRAME_EXIT, &ignored);
}

static bool game_skip_pointer(PreviewGame *game, AbSpan action, AbVec2 point,
                              bool *consumed, AbError *error) {
    *consumed = false;
    AbUiPointerReply reply;
    if (!ab_ui_widget_pointer(&game->skip.widget, action, point, &reply, error))
        return false;
    if (menu_equal(reply.values[0], "SKIP_TO_LEVEL_END")) {
        AbClassicGameRuntimeView view = ab_classic_game_runtime_view(game->game);
        if (!ab_classic_level_ending_skip(view.ending, error))
            return false;
        game->skip.state = AB_UI_SKIP_CLICKED;
        *consumed = true;
    } else
        *consumed = ab_ui_pointer_value_truthy(&game->skip.widget.state.click_started);
    return true;
}

static bool game_skip_draw(PreviewGame *game, float seconds, AbError *error) {
    if (!ab_ui_widget_update(&game->skip.widget, seconds,
                             menu_real(game->globals.time.bits), error))
        return false;
    AbUiDrawInput input = {.transform.scale = {1, 1}, .present = 12};
    return ab_ui_widget_draw(&game->skip.widget, &input, error);
}

#endif
