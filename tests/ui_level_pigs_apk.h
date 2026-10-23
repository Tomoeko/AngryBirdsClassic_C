#ifndef AB_UI_LEVEL_PIGS_APK_TEST_H
#define AB_UI_LEVEL_PIGS_APK_TEST_H

static bool pigs_clicked(void *context, AbDataTable *event, AbError *error) {
    (void)event;
    (void)error;
    ++*(size_t *)context;
    return true;
}

static bool pigs_level_widgets(PreviewMenu *menu, AbMetal *renderer, AbError *error) {
    menu->episodes->selected_episode = menu_route_number(8);
    if (!menu_route_level(menu, error) || !menu->pigs || !menu->pigs->loaded ||
        !menu->pigs->particles || menu->levels->levels.count != 45 ||
        menu->levels->levels.page_count != 3)
        return ab_fail(error, "Bad Piggies did not construct its three level pages");
    AbUiLevelPigs *pigs = &menu->pigs->controls;
    if (pigs->timer != 0 || pigs->animation != 1 || pigs->particle_timer != 0 ||
        pigs->animation_timer < 2.5f || pigs->animation_timer > 6 ||
        menu->levels->frames[0] != &pigs->renderer.tree ||
        menu->levels->frames[1] != &pigs->pig.tree ||
        !ab_ui_pointer_value_truthy(&pigs->pig.state.image.floor_coordinates))
        return ab_fail(error, "Bad Piggies entry/order/unused spelling differs");
    for (size_t frame = 0; frame < 300; ++frame)
        if (!menu_preview_draw(menu, 1.0f / 60, error))
            return false;
    if (!ab_particles_count(menu->pigs->particles, true) ||
        !(menu_real(pigs->pig.frame->fields.w.number_bits) > 0) ||
        menu_real(pigs->pig.frame->fields.scale_x.number_bits) !=
            0.8f * (menu->screen.x / 1024))
        return ab_fail(error, "Bad Piggies art/rocket particles were not recovered");
    for (size_t index = 0; index < ab_particles_count(menu->pigs->particles, true);
         ++index)
        if (!ab_particles_at(menu->pigs->particles, true, index)->sprite_handle)
            return ab_fail(error, "Bad Piggies rocket art was not actually loaded");
    size_t previous = pigs->animation;
    pigs->animation_timer = 0;
    if (!menu_preview_draw(menu, 1.0f / 60, error) || pigs->animation == previous)
        return ab_fail(error, "Bad Piggies animation repeated its previous choice");
    size_t clicks = 0;
    AbEventListener listener = {.context = &clicks, .event_triggered = pigs_clicked};
    AbDataValue clicked = menu_event(menu, "EID_PIGGIES_CLICKED");
    if (!ab_event_manager_add(menu->events, clicked, &listener, error) ||
        !route_click(menu, &pigs->pig, "GOTO_BAD_PIGGIES", error) || clicks != 1 ||
        !ab_event_manager_remove(menu->events, clicked, &listener, error))
        return ab_fail(error, "Bad Piggies actual source button/event route differs");
    AbImage image = {0};
    float clear[4] = {0, 0, 0, 1};
    menu_background_clear(menu, clear);
    bool okay = ab_metal_capture(renderer, menu->context.draw_list,
                                 (uint32_t)menu->screen.x, (uint32_t)menu->screen.y,
                                 clear, 16 * 1024 * 1024, &image, error);
    const char *capture = getenv("AB_PIGS_CAPTURE");
    if (okay && capture)
        okay = save(capture, &image, error);
    ab_image_free(&image);
    if (!okay)
        return false;
    for (size_t page = 1; page < 3; ++page) {
        if (!route_click(menu, &menu->levels->right, "SCROLL_RIGHT", error))
            return false;
        for (size_t frame = 0; frame < 90; ++frame)
            if (!menu_preview_draw(menu, 1.0f / 60, error))
                return false;
    }
    if (menu->levels->current_selection != 3)
        return ab_fail(error, "Bad Piggies source arrows did not reach page three");
    for (size_t index = 0; index < 45; ++index) {
        AbClassicLevelRequest request;
        if (!ab_classic_level_request_resolve(
                ab_episode_catalog_episodes(menu->catalog),
                &menu->levels->levels.buttons[index].meta, &request, error) ||
            !ab_zip_find(menu->archive, request.resource_path))
            return ab_fail(error, "Bad Piggies catalog level lacks its actual file");
    }
    if (!menu_episode_change(menu, true, error) || menu->pigs->loaded ||
        menu->pigs->particles || !menu_route_level(menu, error) ||
        !menu->pigs->loaded || menu->pigs->controls.timer != 0 ||
        !menu_episode_change(menu, true, error))
        return ab_fail(error, "Bad Piggies exit/re-entry retained assets or state");
    puts("Bad Piggies: all 45 catalog records, three Metal pages, real sprite "
         "animation/rocket particles, source pointer event and fresh re-entry.");
    return true;
}

#endif
