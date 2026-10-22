#include "ab/sprite_scene.h"

bool ab_sprite_scene_order(const AbSceneSprite *items, size_t count, size_t *order,
                           size_t order_capacity, AbError *error) {
    if (count > order_capacity || (count && (!items || !order))) {
        return ab_fail(error, "Invalid sprite scene ordering storage");
    }
    for (size_t index = 0; index < count; ++index) {
        size_t cursor = index;
        while (cursor && items[order[cursor - 1]].layer > items[index].layer) {
            order[cursor] = order[cursor - 1];
            --cursor;
        }
        order[cursor] = index;
    }
    return true;
}

static bool append_sprite(const AbSceneSprite *item, AbCoordinateCamera camera,
                          uint32_t width, uint32_t height, AbDrawList *list,
                          AbError *error) {
    if (!item->visible || item->unbound || item->collider == 7) {
        return true;
    }
    if (item->special_animation) {
        return ab_fail(error, "Scene animation wrapper is not recovered yet");
    }
    AbImageRectangle rectangle;
    AbSpriteRenderState state;
    if (!item->texture) {
        return ab_fail(error, "Missing scene sprite texture binding");
    }
    if (!ab_object_sprite_state(item->region, item->pose, camera, &rectangle, &state,
                                error)) {
        return false;
    }
    AbDrawQuad atlas;
    AbSpritePlacement placement = {0, 0, 1, 1, AB_ANCHOR_TOP, AB_ANCHOR_LEFT};
    if (!ab_sprite_quad(item->region, item->texture_width, item->texture_height,
                        placement, &atlas, error)) {
        return false;
    }
    float uv[8];
    for (size_t vertex = 0; vertex < 4; ++vertex) {
        uv[vertex * 2] = atlas.vertices[vertex].u;
        uv[vertex * 2 + 1] = atlas.vertices[vertex].v;
    }
    AbDrawCommand command = {.texture = item->texture,
                             .blend = item->blend,
                             .color = {1, 1, 1, 1},
                             .opacity = 1,
                             .coordinates = AB_DRAW_CLIP};
    bool visible;
    if (!ab_image_rectangle_quad(rectangle, state, width, height, uv, &command.quad,
                                 &visible, error)) {
        return false;
    }
    return !visible || ab_draw_list_push(list, &command, error);
}

bool ab_sprite_scene_draw(const AbSceneSprite *items, size_t count, const size_t *order,
                          AbCoordinateCamera camera, uint32_t viewport_width,
                          uint32_t viewport_height, AbDrawList *list, AbError *error) {
    if (!list || list->count > list->capacity || (list->capacity && !list->commands) ||
        (count && (!items || !order)) || !viewport_width || !viewport_height) {
        return ab_fail(error, "Invalid sprite scene or output list");
    }
    size_t original_count = list->count;
    for (size_t index = 0; index < count; ++index) {
        if (order[index] >= count) {
            list->count = original_count;
            return ab_fail(error, "Invalid sprite scene order at index %zu", index);
        }
        if (!append_sprite(items + order[index], camera, viewport_width,
                           viewport_height, list, error)) {
            list->count = original_count;
            return false;
        }
    }
    return true;
}
