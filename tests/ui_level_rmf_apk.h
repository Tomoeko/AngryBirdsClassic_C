#ifndef AB_UI_LEVEL_RMF_APK_TEST_H
#define AB_UI_LEVEL_RMF_APK_TEST_H

static bool rmf_level_widgets(PreviewMenu *menu, AbMetal *renderer, AbError *error) {
    menu->episodes->selected_episode = menu_route_number(9);
    if (!menu_route_level(menu, error) || !menu->rmf_levels ||
        menu->levels->levels.count != 30)
        return ab_fail(error, "RMF episode did not construct both actual level pages");
    AbUiLevelRmf *rmf = menu->rmf_levels;
    AbUiLevelSelection *levels = menu->levels;
    if (rmf->slide_phase != 0.5f || rmf->tint_phase != 0)
        return ab_fail(error, "RMF entry phases differ from the source");
    for (size_t frame = 0; frame < 90; ++frame)
        if (!menu_preview_draw(menu, 1.0f / 60, error))
            return false;
    if (rmf->slide_phase != 0 || rmf->tint_phase != 0 ||
        !route_click(menu, &levels->right, "SCROLL_RIGHT", error))
        return ab_fail(error, "RMF first page failed its decoration/arrow state");
    for (size_t frame = 0; frame < 150; ++frame)
        if (!menu_preview_draw(menu, 1.0f / 60, error))
            return false;
    if (levels->current_selection != 2 || rmf->slide_phase != 1 ||
        rmf->tint_phase != 1 || menu_real(rmf->left.frame->fields.x.number_bits) != 0 ||
        menu_real(rmf->right.frame->fields.x.number_bits) != menu->screen.x ||
        !(menu_real(rmf->left.frame->fields.w.number_bits) > 0) ||
        !(menu_real(rmf->right.frame->fields.w.number_bits) > 0))
        return ab_fail(error, "RMF page two omitted its alternate decorations/tint");
    AbImage image = {0};
    float clear[4] = {0, 0, 0, 1};
    menu_background_clear(menu, clear);
    bool okay = ab_metal_capture(renderer, menu->context.draw_list,
                                 (uint32_t)menu->screen.x, (uint32_t)menu->screen.y,
                                 clear, 16 * 1024 * 1024, &image, error);
    ab_image_free(&image);
    if (!okay || !route_click(menu, &levels->left, "SCROLL_LEFT", error))
        return false;
    for (size_t frame = 0; frame < 150; ++frame)
        if (!menu_preview_draw(menu, 1.0f / 60, error))
            return false;
    if (levels->current_selection != 1 || rmf->slide_phase != 0 || rmf->tint_phase != 0)
        return ab_fail(error, "RMF page return did not restore its source phases");
    puts("RMF level screen: all 30 catalog buttons, two Metal pages, alternate "
         "decorations, source slide/tint and reverse navigation.");
    return true;
}

#endif
