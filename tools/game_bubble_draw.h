#ifndef AB_GAME_BUBBLE_DRAW_H
#define AB_GAME_BUBBLE_DRAW_H

static const char *const game_bubble_sprites[] = {"P_BIRDBUBBLE_1", "P_BIRDBUBBLE_2",
                                                  "P_BIRDBUBBLE_3"};

static bool game_bubble_draw_one(PreviewBubbles *owner, const AbDataTable *physical,
                                 AbExtraDrawInput input, AbError *error) {
    float timer, scale = 1;
    if (!game_bubble_read(physical, "bubbleAntiGravityTimer", &timer, error))
        return false;
    if (!(timer > 0))
        return true;
    if (menu_route_truthy(menu_route_field(physical, "overrideBubblePosition")))
        return ab_fail(error, "Bubble requires its actual position override");
    AbDataValue sprite = menu_route_field(physical, "bubbleSprite");
    size_t index = 0;
    while (index < 3 &&
           !menu_route_equal(sprite, game_text(game_bubble_sprites[index])))
        ++index;
    if (index == 3)
        return ab_fail(error, "Unknown actual bubble sprite");
    AbDataValue raw_scale = menu_route_field(physical, "bubbleSpriteScale");
    if (menu_route_truthy(raw_scale) &&
        !game_bubble_read(physical, "bubbleSpriteScale", &scale, error))
        return false;
    AbVec2 position, screen;
    if (!game_bubble_read(physical, "x", &position.x, error) ||
        !game_bubble_read(physical, "y", &position.y, error) ||
        !ab_coordinate_point(&input.camera, AB_SPACE_PHYSICS, AB_SPACE_SCREEN, position,
                             &screen))
        return false;
    float zoom = (2 * input.camera.zoom.x) * scale;
    if (!isfinite(zoom) || zoom == 0)
        return ab_fail(error, "Invalid actual bubble draw scale");
    const AbLuaAssetSpriteBinding *binding = owner->sprites + index;
    const AbSpriteRegion *region = binding->region;
    float pivot_x = (float)(int32_t)((float)region->pivot_x + .5f);
    float pivot_y = (float)(int32_t)((float)region->pivot_y + .5f);
    /* drawIngameBubbles4872 retains the screen division, explicit shake
     * subtraction and native setRenderState scaling in this order. */
    AbImageRectangle rectangle = {
        screen.x / zoom - owner->game->shake.offset.x - pivot_x,
        screen.y / zoom - owner->game->shake.offset.y - pivot_y, (float)region->width,
        (float)region->height};
    AbDrawCommand command;
    bool visible;
    return ab_image_rectangle_command(
               rectangle, (AbSpriteRenderState){.scale_x = zoom, .scale_y = zoom},
               input.width, input.height, binding->uv,
               (AbImageRectangleMaterial){binding->texture, binding->has_alpha, 1},
               &command, &visible, error) &&
           (!visible || ab_draw_list_push(input.list, &command, error));
}

static bool game_bubble_draw(void *context, void *parameter, AbExtraDrawInput input,
                             AbError *error) {
    (void)parameter;
    PreviewBubbles *owner = context;
    AbClassicLevelView level = ab_classic_level_view(owner->game->playfield.level);
    AbClassicWorldCursor cursor = {0};
    for (;;) {
        size_t object;
        bool present;
        if (!ab_classic_level_world_next(owner->game->playfield.level, &cursor, &object,
                                         &present, error))
            return false;
        if (!present)
            return true;
        const AbDataTable *physical = &level.properties->objects[object].physical;
        if (menu_route_truthy(menu_route_field(physical, "inBubble")) &&
            !menu_route_truthy(menu_route_field(physical, "dontDrawBubble")) &&
            !game_bubble_draw_one(owner, physical, input, error))
            return false;
    }
}

#endif
