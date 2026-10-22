#ifndef AB_UI_LEVEL_SELECTION_APK_TEST_H
#define AB_UI_LEVEL_SELECTION_APK_TEST_H

#include "ab/ui_level_selection.h"

typedef struct {
    AbDataTable *last;
    size_t count;
} LevelSelectionEvents;

static bool selection_record(void *context, AbDataTable *event, AbError *error) {
    (void)error;
    LevelSelectionEvents *events = context;
    events->last = event;
    ++events->count;
    return true;
}

static bool selection_background_alias(void *context, AbSpan image,
                                       AbUiImageRenderBinding *output, AbError *error) {
    const AbUiImageRenderBoundaries *real = context;
    if (image.size != 13 || memcmp(image.data, "LS_BACKGROUND", 13))
        return ab_fail(error, "Unexpected level-background resource lookup");
    /* Explicit geometry fixture: the target's LS_BACKGROUND is absent. Borrow
     * a prepared sprite only to exercise the mirrored draw arithmetic. */
    return real->lookup(real->context, menu_span("BUTTON_TOONS_PLAY"), output, error);
}

static bool selection_background(PreviewMenu *menu, AbMetal *renderer, AbError *error) {
    AbUiImageRenderState previous = menu->render;
    AbUiImageRenderBoundaries real = menu->context.render;
    uint32_t width = menu->context.render.viewport_width;
    menu->context.render.viewport_width = 1025;
    menu->render.alpha = 0.375f;
    menu->render.geometry.translation_x = 7;
    ab_draw_list_clear(menu->context.draw_list);
    bool okay = false;
    AbImage captured = {0};
    if (!ab_ui_level_selection_background(NULL, (AbVec2){0}, true, error) ||
        !ab_ui_level_selection_background(&menu->context, (AbVec2){1025, 768}, false,
                                          error))
        goto release;
    if (menu->context.draw_list->count || menu->render.alpha != 0.375f ||
        menu->render.geometry.translation_x != 0 || menu->render.geometry.scale_x != 1)
        goto release;
    menu->context.render.context = &real;
    menu->context.render.lookup = selection_background_alias;
    if (!ab_ui_level_selection_background(&menu->context, (AbVec2){1025, 768}, false,
                                          error))
        goto release;
    if (menu->context.draw_list->count != 2 || menu->render.alpha != 0.375f ||
        menu->render.geometry.translation_x != 0 ||
        menu->render.geometry.scale_x != 1 || menu->render.geometry.scale_y != 1)
        goto release;
    const AbDrawQuad *mirror = &menu->context.draw_list->commands[0].quad;
    const AbDrawQuad *ordinary = &menu->context.draw_list->commands[1].quad;
    float mirror_right = (mirror->vertices[0].x + 1) * 0.5f * 1025;
    float mirror_left = (mirror->vertices[1].x + 1) * 0.5f * 1025;
    float ordinary_left = (ordinary->vertices[0].x + 1) * 0.5f * 1025;
    float ordinary_right = (ordinary->vertices[1].x + 1) * 0.5f * 1025;
    if (fabsf(mirror_right - 1025) > 0.001f || fabsf(mirror_left - 512) > 0.001f ||
        fabsf(ordinary_left) > 0.001f || fabsf(ordinary_right - 512) > 0.001f)
        goto release;
    const float clear[] = {0.1f, 0.3f, 0.7f, 1};
    if (!ab_metal_capture(renderer, menu->context.draw_list, 1025, 768, clear,
                          16 * 1024 * 1024, &captured, error))
        goto release;
    /* The odd center column belongs to the mirrored ceil half. Actual APK
     * alpha may leave clear pixels; verify that its GPU draw contributes. */
    size_t colored = 0;
    for (uint32_t y = 0; y < captured.height; ++y)
        for (uint32_t x = 0; x < captured.width; ++x) {
            const uint8_t *pixel = captured.rgba.data + ((size_t)y * 1025 + x) * 4;
            if (pixel[3] != 255)
                goto release;
            if (pixel[0] != 26 || pixel[1] != 76 || pixel[2] != 178)
                ++colored;
        }
    okay = colored != 0;
release:
    menu->context.render.viewport_width = width;
    menu->context.render = real;
    menu->render = previous;
    ab_draw_list_clear(menu->context.draw_list);
    ab_image_free(&captured);
    if (!okay && !error->message[0])
        ab_fail(error, "Mirrored level background lost coverage/state at odd width");
    return okay;
}

static bool selection_click(PreviewMenu *menu, AbUiLevelSelection *owner,
                            AbUiWidget *button, const char *expected, AbError *error) {
    AbVec2 point, size, pivot, scale;
    AbUiPointerValue image = button->state.image.image;
    if (image.kind != AB_UI_POINTER_TEXT ||
        !ab_ui_widget_screen_position(button, &point, error) ||
        !ab_ui_widget_cumulative_scale(button, &scale, error) ||
        !menu->context.images.bounds(menu->context.images.context, image.text, &size,
                                     error) ||
        !menu->context.images.pivot(menu->context.images.context, image.text, &pivot,
                                    error))
        return false;
    point.x += (size.x * 0.5f - pivot.x) * scale.x;
    point.y += (size.y * 0.5f - pivot.y) * scale.y;
    menu->context.cursor = point;
    menu->context.left_button_held = true;
    menu->context.touch_count = 1;
    AbUiPointerReply reply;
    if (!ab_ui_widget_pointer(&owner->widget, menu_span("LPRESS"), point, &reply,
                              error))
        return false;
    menu->context.left_button_held = false;
    menu->context.touch_count = 0;
    return ab_ui_widget_pointer(&owner->widget, menu_span("LRELEASE"), point, &reply,
                                error) &&
           (menu_equal(reply.values[0], expected) ||
            ab_fail(error, "LevelSelection reply differs: %s", expected));
}

static bool level_selection_widgets(PreviewMenu *menu, AbMetal *renderer,
                                    const char *path, AbError *error) {
    AbUiLevelSelection *owner = calloc(1, sizeof(*owner));
    if (!owner)
        return ab_fail(error, "Cannot allocate level-selection test storage");
    bool okay = false, entered = false;
    size_t subscribed = 0;
    const char *stage = "construction";
    AbImage captured = {0};
    LevelSelectionEvents events = {0};
    AbEventListener listener = {.context = &events,
                                .event_triggered = selection_record};
    const char *const observed[] = {"EID_CHANGE_SCENE", "EID_LOAD_INTRO_CUTSCENE",
                                    "EID_CHANGE_LEVEL", "EID_SHOW_REGISTRATION_DIALOG"};
    AbDataEntry score_fields[] = {
        {menu_data("score"), level_test_number(1)},
        {menu_data("completed"), {.kind = AB_DATA_BOOLEAN, .bits = 1}}};
    AbDataTable score = {score_fields, 2, 2, NULL};
    AbDataEntry saved = {menu_data("Level1"), {.kind = AB_DATA_TABLE, .table = &score}};
    AbDataTable highscores = {&saved, 0, 1, NULL};
    AbEpisodeStatistics statistics = menu->statistics;
    statistics.highscores = &highscores;
    AbUiLevelServices services = {
        .statistics = &statistics,
        .local_settings = ab_settings_statistics_root(menu->local_settings)};
    AbUiLevelButtonHost buttons = ab_ui_level_services_host(&services);
    buttons.fonts = &menu->fonts;
    buttons.scaling = &menu->scaling;
    AbLevelProgress progress = {.episodes = statistics.episodes,
                                .highscores = &highscores,
                                .release_build = true};
    float script_time = 0;
    AbUiLevelSelectionHost host = {
        .levels = {.buttons = buttons,
                   .progress = &progress,
                   .scroll = {.events = menu->events,
                              .pause_event = menu_event(menu, "EID_GAME_PAUSED"),
                              .input_seconds = &menu->input_seconds,
                              .wheel = &menu->wheel},
                   .event_ids = menu->event_data.data.globals,
                   .initial_time = 0,
                   .arrows = true},
        .preferences = &menu->preferences,
        .episode_progress = &menu->progress,
        .exit_link = menu_data("EXIT_LEVEL_SELECTION"),
        .script_time = &script_time,
        .interpolate_background = true};
    if (!selection_background(menu, renderer, error))
        goto release;
    for (; subscribed < sizeof(observed) / sizeof(observed[0]); ++subscribed)
        if (!ab_event_manager_add(menu->events, menu_event(menu, observed[subscribed]),
                                  &listener, error))
            goto release;
    if (!ab_episode_preferences_set_page(&menu->preferences, level_test_number(1),
                                         level_test_number(1), error) ||
        !ab_ui_level_selection_init(owner, &menu->context, level_test_number(1), host,
                                    error) ||
        !ab_ui_widget_lifecycle(&owner->widget, AB_UI_FRAME_ENTRY, error))
        goto release;
    entered = true;
    stage = "layout and source controls";
    if (!ab_ui_widget_lifecycle(&owner->widget, AB_UI_FRAME_LAYOUT, error) ||
        !ab_ui_widget_update(&owner->widget, 0, script_time, error) ||
        owner->widget.frame->child_count != 12 || owner->levels.count != 63 ||
        owner->current_selection != 1 ||
        !menu_equal(owner->dots[0].state.image.image, "LS_DOT_WHITE") ||
        ab_ui_pointer_value_truthy(&owner->left.frame->fields.visible) ||
        !ab_ui_pointer_value_truthy(&owner->right.frame->fields.visible) ||
        !ab_ui_pointer_value_truthy(
            &owner->page_numbers[0].widget.frame->fields.visible))
        goto release;
    stage = "complete component Metal capture";
    ab_draw_list_clear(menu->context.draw_list);
    AbUiDrawInput input = {0};
    float clear[4];
    for (size_t index = 0; index < 4; ++index)
        clear[index] = owner->background[index] / 255;
    if (!ab_ui_widget_draw(&owner->widget, &input, error) ||
        !ab_metal_capture(renderer, menu->context.draw_list, 1024, 768, clear,
                          16 * 1024 * 1024, &captured, error) ||
        (path && !save(path, &captured, error)))
        goto release;
    stage = "intro cutscene metadata";
    if (!selection_click(menu, owner, &owner->levels.buttons[0].widget, "GOTO_LEVEL",
                         error) ||
        events.count != 1 || events.last != &owner->launch_event ||
        level_test_child(events.last, menu_data("data")) !=
            &owner->levels.buttons[0].meta ||
        ab_data_get(events.last, menu_data("cutscene")) == NULL ||
        ab_data_get(events.last, menu_data("minLoadTime")) != NULL)
        goto release;
    stage = "score synchronization and direct launch";
    highscores.count = 1;
    AbDataEntry sync_field = {menu_data("id"), menu_event(menu, "EID_SYNC_NEW_DATA")};
    AbDataTable sync = {&sync_field, 1, 1, NULL};
    if (!ab_event_manager_notify(menu->events, &sync, error) ||
        !selection_click(menu, owner, &owner->levels.buttons[1].widget, "GOTO_LEVEL",
                         error) ||
        events.count != 2 || events.last != &owner->launch_event ||
        !ab_data_get(events.last, menu_data("from")) ||
        ab_data_get(events.last, menu_data("cutscene")) != NULL)
        goto release;
    stage = "page arrow and saved-page restoration";
    if (!selection_click(menu, owner, &owner->right, "SCROLL_RIGHT", error))
        goto release;
    for (size_t frame = 0; frame < 64; ++frame) {
        script_time += 0.1f;
        if (!ab_ui_widget_update(&owner->widget, 0.1f, script_time, error))
            goto release;
    }
    AbDataValue stored;
    bool visited = false;
    if (owner->current_selection != 2 ||
        !menu_equal(owner->dots[1].state.image.image, "LS_DOT_WHITE") ||
        !menu_equal(owner->dots[0].state.image.image, "LS_DOT_BLACK") ||
        !ab_episode_preferences_current_page(&menu->preferences, level_test_number(1),
                                             &stored, error) ||
        stored.bits != menu_number(2).number_bits ||
        !ab_episode_progress_page_visited(&menu->progress, level_test_number(1),
                                          level_test_number(2), &visited, error) ||
        !visited)
        goto release;
    stage = "trial registration retains exact success event";
    /* The actual first page is free_to_play; use the real second page and an
     * explicitly opened button to reach the independent trial launch gate. */
    owner->host.trial_version = true;
    if (!ab_ui_level_button_open(owner->levels.buttons + 21, error) ||
        !selection_click(menu, owner, &owner->levels.buttons[21].widget, "GOTO_LEVEL",
                         error) ||
        events.count != 3 || events.last != &owner->registration ||
        level_test_child(events.last, menu_data("success_event")) !=
            &owner->launch_event)
        goto release;
    owner->host.trial_version = false;
    stage = "Back and reentry";
    if (!selection_click(menu, owner, &owner->back, "EPISODE_SELECTION", error) ||
        events.count != 4 || events.last != &owner->back_event ||
        !ab_ui_widget_lifecycle(&owner->widget, AB_UI_FRAME_EXIT, error))
        goto release;
    entered = false;
    if (!ab_ui_scroll_frame_set_current(&owner->levels.scroll, 1, error) ||
        !ab_ui_widget_lifecycle(&owner->widget, AB_UI_FRAME_ENTRY, error))
        goto release;
    entered = true;
    if (!ab_ui_widget_lifecycle(&owner->widget, AB_UI_FRAME_LAYOUT, error) ||
        owner->levels.scroll.current_anchor != 2 || owner->current_selection != 2)
        goto release;
    stage = "strict deferred scroll deadline";
    script_time = 0;
    AbDataEntry deferred_fields[] = {
        {menu_data("id"), menu_event(menu, "EID_SCROLL_TO_NEXT_WORLD")},
        {menu_data("episode"), level_test_number(1)}};
    AbDataTable deferred = {deferred_fields, 2, 2, NULL};
    if (!ab_event_manager_notify(menu->events, &deferred, error))
        goto release;
    script_time = 0.1f;
    if (!ab_ui_widget_update(&owner->widget, 0, script_time, error) ||
        !owner->scroll_to_next_page || owner->levels.scroll.target_anchor != 2)
        goto release;
    script_time = 0.11f;
    if (!ab_ui_widget_update(&owner->widget, 0, script_time, error) ||
        owner->scroll_to_next_page || owner->levels.scroll.target_anchor != 3)
        goto release;
    puts("Actual LevelSelection: source controls, page dots, saved/visited pages, "
         "Back, intro/direct/trial launch events and strict deferred scroll deadline.");
    okay = true;
release:
    if (!okay && !error->message[0])
        ab_fail(error, "Actual LevelSelection integration differs at %s", stage);
    AbError cleanup = {0};
    if (entered && !menu->context.failed)
        (void)ab_ui_widget_lifecycle(&owner->widget, AB_UI_FRAME_EXIT, &cleanup);
    (void)ab_ui_level_selection_detach(owner, &cleanup);
    for (size_t index = 0; index < subscribed; ++index)
        (void)ab_event_manager_remove(menu->events, menu_event(menu, observed[index]),
                                      &listener, &cleanup);
    ab_image_free(&captured);
    free(owner);
    return okay;
}

#endif
