#ifndef AB_UI_EPISODE_NAVIGATION_APK_TEST_H
#define AB_UI_EPISODE_NAVIGATION_APK_TEST_H

typedef struct {
    PreviewMenu *menu;
    AbClassicLoader *loader;
    AbClassicLoaderState state;
    AbClassicLoaderRequest request;
    AbDataValue time, skip, do_loading, show_loading, hide_loading;
    AbEventListener listener;
    AbSpan theme;
    size_t loaded, completed;
} EpisodeNavigationFixture;

static bool route_theme(void *context, AbSpan theme, AbError *error) {
    (void)error;
    EpisodeNavigationFixture *fixture = context;
    fixture->theme = theme;
    ++fixture->loaded;
    return true;
}

static bool route_event(void *context, AbDataTable *event, AbError *error) {
    EpisodeNavigationFixture *fixture = context;
    const AbDataValue *target = ab_data_get(event, menu_data("target"));
    if (target) {
        if (target->kind == AB_DATA_STRING && target->string.size == 17 &&
            !memcmp(target->string.data, "LEVEL_SELECTION_1", 17))
            ++fixture->completed;
        return true;
    }
    AbUiEpisodeSelection *owner = fixture->menu->episodes;
    const AbDataValue *items = ab_data_get(event, menu_data("items"));
    const AbDataValue *completion = ab_data_get(event, menu_data("completion_event"));
    if (!items || items->kind != AB_DATA_TABLE ||
        items->table != &owner->loading_items || !completion ||
        completion->kind != AB_DATA_TABLE || completion->table != &owner->completion ||
        owner->item.value.kind != AB_DATA_OBJECT ||
        owner->item.value.object != &owner->load)
        return ab_fail(error,
                       "Episode pointer emitted malformed actual Loader request");
    fixture->request = (AbClassicLoaderRequest){.id = fixture->do_loading,
                                                .completion_event = *completion,
                                                .items = &owner->loading_calls};
    return ab_classic_loader_event(fixture->loader, &fixture->request, error);
}

static bool route_click(PreviewMenu *menu, AbUiWidget *widget, const char *expected,
                        AbError *error) {
    AbVec2 point, scale, size, pivot;
    AbUiPointerValue image = widget->state.image.image;
    if (image.kind != AB_UI_POINTER_TEXT ||
        !ab_ui_widget_screen_position(widget, &point, error) ||
        !ab_ui_widget_cumulative_scale(widget, &scale, error) ||
        !menu->context.images.bounds(menu->context.images.context, image.text, &size,
                                     error) ||
        !menu->context.images.pivot(menu->context.images.context, image.text, &pivot,
                                    error))
        return false;
    point.x += (size.x * 0.5f - pivot.x) * scale.x;
    point.y += (size.y * 0.5f - pivot.y) * scale.y;
    return action(menu, widget, point, expected, error);
}

static bool route_enter(PreviewMenu *menu, AbError *error) {
    menu->navigation_enabled = true;
    AbUiPointerReply reply;
    menu->context.left_button_held = true;
    menu->context.touch_count = 1;
    AbVec2 point = {menu_real(menu->play.frame->fields.x.number_bits),
                    menu_real(menu->play.frame->fields.y.number_bits)};
    if (!menu_preview_pointer(menu, menu_span("LPRESS"), point, &reply, error))
        return false;
    menu->context.touch_count = 0;
    menu->context.left_button_held = false;
    return menu_preview_pointer(menu, menu_span("LRELEASE"), point, &reply, error) &&
           menu_equal(reply.values[0], "PLAY") && menu->episode_active;
}

static bool episode_navigation_widgets(PreviewMenu *menu, AbMetal *renderer,
                                       const float clear[4], const char *path,
                                       AbError *error) {
    EpisodeNavigationFixture fixture = {.menu = menu, .time = {.kind = AB_DATA_NUMBER}};
    AbImage captured = {0};
    bool okay = false, subscribed_load = false, subscribed_change = false;
    const char *stage = "enter from Play";
    if (!ab_audio_mixer_start(menu->mixer, false, error) ||
        !ab_ui_widget_clear_pointer_flags(&menu->root, error) ||
        !route_enter(menu, error) || !menu_preview_draw(menu, 0, error))
        goto release;
    AbUiEpisodeSelection *owner = menu->episodes;
    stage = "initial controls and Metal capture";
    if (owner->widget.frame->child_count != 6 || owner->cards.count != 14 ||
        owner->current_selection != 1 ||
        ab_ui_pointer_value_truthy(&owner->left.frame->fields.visible) ||
        !ab_ui_pointer_value_truthy(&owner->right.frame->fields.visible) ||
        !ab_metal_capture(renderer, menu->context.draw_list, 480, 320, clear,
                          1024 * 1024, &captured, error) ||
        (path && !save(path, &captured, error)))
        goto release;
    ab_image_free(&captured);
    stage = "right arrow and motion";
    if (!route_click(menu, &owner->right, "SCROLL_RIGHT", error))
        goto release;
    for (size_t frame = 0; frame < 128 && owner->cards.scroll.current_anchor != 2;
         ++frame)
        if (!menu_preview_draw(menu, 0.1f, error))
            goto release;
    AbDataValue saved;
    stage = "arrow saved selection";
    if (owner->current_selection != 2 ||
        !ab_episode_preferences_selected(&menu->preferences, &saved, error) ||
        saved.kind != AB_DATA_NUMBER || saved.bits != menu_number(2).number_bits ||
        !ab_ui_pointer_value_truthy(&owner->left.frame->fields.visible))
        goto release;
    stage = "back and reentry";
    if (!route_click(menu, &owner->back, "GOTO_MAIN_MENU", error) ||
        menu->episode_active || !route_enter(menu, error) ||
        owner->current_selection != 2)
        goto release;
    AbDataEntry request_fields[] = {
        {menu_data("id"), menu_event(menu, "EID_SCROLL_TO_EPISODE")},
        {menu_data("targetEpisode"),
         {.kind = AB_DATA_NUMBER, .bits = menu_number(1).number_bits}}};
    AbDataTable scroll_request = {request_fields, 2, 2, NULL};
    stage = "deferred scroll request";
    if (!ab_event_manager_notify(menu->events, &scroll_request, error) ||
        !owner->cards.autoscroll_on_entry || owner->cards.scroll.current_anchor != 2 ||
        !menu_preview_draw(menu, 0, error) || owner->cards.autoscroll_on_entry ||
        owner->cards.scroll.target_anchor != 1)
        goto release;
    for (size_t frame = 0; frame < 128 && owner->cards.scroll.current_anchor != 1;
         ++frame)
        if (!menu_preview_draw(menu, 0.1f, error))
            goto release;
    fixture.do_loading = menu_event(menu, "EID_DO_LOADING");
    stage = "Loader setup";
    fixture.show_loading = menu_event(menu, "EID_SHOW_LOADING_PAGE");
    fixture.hide_loading = menu_event(menu, "EID_HIDE_LOADING_PAGE");
    fixture.state.frame = (AbDataValue){.kind = AB_DATA_NUMBER};
    if (!ab_classic_loader_open(
            (AbClassicLoaderConfig){.state = &fixture.state,
                                    .script_time = &fixture.time,
                                    .do_loading = &fixture.do_loading,
                                    .show_loading = &fixture.show_loading,
                                    .hide_loading = &fixture.hide_loading,
                                    .skip_input = &fixture.skip,
                                    .events = menu->events,
                                    .notifications = 8,
                                    .storage_bytes = 4096},
            &fixture.loader, error))
        goto release;
    fixture.listener =
        (AbEventListener){.context = &fixture, .event_triggered = route_event};
    if (!ab_event_manager_add(menu->events, fixture.do_loading, &fixture.listener,
                              error))
        goto release;
    subscribed_load = true;
    if (!ab_event_manager_add(menu->events, menu_event(menu, "EID_CHANGE_SCENE"),
                              &fixture.listener, error))
        goto release;
    subscribed_change = true;
    owner->host.context = &fixture;
    owner->host.load_theme = route_theme;
    stage = "first card and Loader completion";
    if (!ab_ui_widget_clear_pointer_flags(&owner->widget, error) ||
        !route_click(menu, &owner->cards.buttons[0].widget, "GOTO_LEVEL_SELECTION",
                     error))
        goto release;
    for (size_t frame = 0; frame < 8 && !fixture.completed; ++frame)
        if (!ab_classic_loader_update(fixture.loader, error))
            goto release;
    if (fixture.loaded != 1 || fixture.completed != 1 || fixture.theme.size != 6 ||
        memcmp(fixture.theme.data, "theme1", 6))
        goto release;
    puts(
        "Metal menu route: Play, all 14 cards, back, arrow motion, retained selection, "
        "deferred scroll request and actual C Loader completion to LEVEL_SELECTION_1.");
    okay = true;
release:
    if (subscribed_load)
        (void)ab_event_manager_remove(menu->events, fixture.do_loading,
                                      &fixture.listener, NULL);
    if (subscribed_change)
        (void)ab_event_manager_remove(menu->events,
                                      menu_event(menu, "EID_CHANGE_SCENE"),
                                      &fixture.listener, NULL);
    if (menu->episodes) {
        menu->episodes->host.context = NULL;
        menu->episodes->host.load_theme = NULL;
    }
    ab_classic_loader_close(fixture.loader);
    ab_image_free(&captured);
    if (!okay && !error->message[0])
        ab_fail(error, "Contiguous episode menu integration differs: %s", stage);
    return okay;
}

typedef struct {
    AbVec2 points[4][4];
    size_t count;
} MenuBorderFixture;

static bool border_image(void *context, const AbSpriteBinding *sprite,
                         const AbVec2 vertices[4], const float tint[4],
                         AbError *error) {
    MenuBorderFixture *fixture = context;
    (void)tint;
    if (!sprite->region || fixture->count == 4)
        return ab_fail(error, "Unexpected menu screen-border sprite draw");
    memcpy(fixture->points[fixture->count++], vertices, sizeof(fixture->points[0]));
    return true;
}

static bool episode_background_widgets(PreviewMenu *menu, AbMetal *renderer,
                                       const char *path, AbError *error) {
    AbImage captured = {0};
    if (!menu_background_open(menu, error))
        return false;
    AbVec2 missing = {7, 8};
    if (!menu->context.images.pivot(menu->context.images.context,
                                    menu_span("fixture_missing_sprite"), &missing,
                                    error) ||
        missing.x != 0 || missing.y != 0)
        return ab_fail(error, "Native missing-sprite pivot did not return zero");
    size_t count = ab_theme_layers_count(menu->theme_layers);
    if (count != 5 || menu->theme_name.size != 6 ||
        memcmp(menu->theme_name.data, "theme1", 6))
        return ab_fail(error, "Wrong Android menu theme selection");
    for (size_t index = 1; index <= count; ++index) {
        const AbThemeLayerBinding *binding;
        if (!ab_theme_layers_binding(menu->theme_layers, index, &binding) ||
            binding->kind == AB_THEME_SPRITE_MISSING)
            return ab_fail(error, "Android menu theme has missing layer %zu", index);
    }
    /* Controlled border metadata tests the separate supported subtype. The
     * Android theme1 preview keeps its own unmodified five-layer graph. */
    const AbDataValue *theme =
        ab_data_get(menu->theme_data.data.globals, menu_data("theme1"));
    AbDataEntry border_fields[] = {
        {menu_data("type"), menu_data("border")},
        {menu_data("sprite"), menu_data("SCREEN_GRADIENT")},
        {menu_data("borderSize"),
         {.kind = AB_DATA_NUMBER, .bits = menu_number(0.2f).number_bits}}};
    AbDataTable border = {border_fields, 3, 3, NULL};
    AbDataEntry item = {
        .key = {.kind = AB_DATA_NUMBER, .bits = menu_number(1).number_bits},
        .value = {.kind = AB_DATA_TABLE, .table = &border}};
    AbDataTable foreground = {&item, 1, 1, NULL}, empty = {0};
    AbDataEntry root_fields[] = {
        {menu_data("color"), *ab_data_get(theme->table, menu_data("color"))},
        {menu_data("groundColor"),
         *ab_data_get(theme->table, menu_data("groundColor"))},
        {menu_data("bgLayers"), {.kind = AB_DATA_TABLE, .table = &empty}},
        {menu_data("fgLayers"), {.kind = AB_DATA_TABLE, .table = &foreground}}};
    AbDataTable root = {root_fields, 4, 4, NULL};
    AbThemeLayers *controlled = NULL;
    AbThemeLayerRenderState state = {.alpha = -7};
    MenuBorderFixture fixture = {0};
    if (!ab_theme_layers_open_provider(&root, &menu->sprites.provider, 4096,
                                       &controlled, error))
        return false;
    bool drew = ab_theme_layers_draw_border(controlled, 1, 480, 320, 1, &state,
                                            border_image, &fixture, error) &&
                state.alpha == -7 && !fixture.count &&
                ab_theme_layers_draw_border(controlled, 1, 480, 320, 0, &state,
                                            border_image, &fixture, error) &&
                state.alpha == 1 && !fixture.count;
    ab_theme_layers_close(controlled);
    controlled = NULL;
    if (!drew)
        return ab_fail(error, "Screen-border missing-sprite/filter reset differs");
    border_fields[1].value = menu_data("BUTTON_EMPTY");
    if (!ab_theme_layers_open_provider(&root, &menu->sprites.provider, 4096,
                                       &controlled, error))
        return false;
    drew = ab_theme_layers_draw_border(controlled, 1, 480, 320, 0, &state, border_image,
                                       &fixture, error);
    ab_theme_layers_close(controlled);
    if (!drew)
        return false;
    const AbVec2 expected[4][4] = {{{416, 0}, {480, 0}, {416, 320}, {480, 320}},
                                   {{64, 320}, {0, 320}, {64, 0}, {0, 0}},
                                   {{480, 256}, {480, 320}, {0, 256}, {0, 320}},
                                   {{0, 64}, {0, 0}, {480, 64}, {480, 0}}};
    if (fixture.count != 4 || memcmp(fixture.points, expected, sizeof(expected)) ||
        state.alpha != 1 || state.geometry.scale_x != 1 ||
        state.geometry.scale_y != 1 || !menu_preview_draw(menu, 0.02f, error)) {
        if (!error->message[0])
            ab_fail(error, "Menu border draw differs: count %zu, alpha %g, scale %g,%g",
                    fixture.count, (double)state.alpha, (double)state.geometry.scale_x,
                    (double)state.geometry.scale_y);
        return false;
    }
    float clear[4];
    menu_background_clear(menu, clear);
    bool okay = ab_metal_capture(renderer, menu->context.draw_list, 480, 320, clear,
                                 1024 * 1024, &captured, error) &&
                (!path || save(path, &captured, error));
    ab_image_free(&captured);
    if (okay)
        puts("Metal episode menu: Android theme1, live resource recipe, "
             "all five layers bound, scrolling camera and separate border checks. "
             "Effects and complete root behavior remain open.");
    return okay;
}

#endif
