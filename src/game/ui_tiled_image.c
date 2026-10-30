#include "ab/ui_tiled_image.h"

#include <math.h>
#include <string.h>

static AbUiPointerValue number(float value) {
    AbUiPointerValue result = {.kind = AB_UI_POINTER_NUMBER};
    memcpy(&result.number_bits, &value, sizeof(value));
    return result;
}

static bool numeric(AbUiPointerValue value, float *output, AbError *error) {
    if (value.kind != AB_UI_POINTER_NUMBER)
        return ab_fail(error, "Nonnumeric TiledImage geometry");
    memcpy(output, &value.number_bits, sizeof(*output));
    return isfinite(*output) || ab_fail(error, "Nonfinite TiledImage geometry");
}

bool ab_ui_tiled_image_set_image(AbUiTiledImage *image, AbSpan name, AbError *error) {
    if (!image || !image->widget.initialized || (name.size && !name.data) ||
        !image->widget.context->images.bounds)
        return ab_fail(error, "Missing TiledImage resource boundary");
    /* Source assigns the identifier before querying the resource dimensions. */
    image->image = name;
    AbVec2 size;
    if (!image->widget.context->images.bounds(image->widget.context->images.context,
                                              name, &size, error))
        return false;
    if (!isfinite(size.x) || !isfinite(size.y))
        return ab_fail(error, "Nonfinite TiledImage resource dimensions");
    image->widget.frame->fields.w = number(size.x);
    image->widget.frame->fields.h = number(size.y);
    return true;
}

bool ab_ui_tiled_image_repeat(AbUiTiledImage *image, float x, float y, AbError *error) {
    if (!image || !image->widget.initialized || !isfinite(x) || !isfinite(y) ||
        fabsf(x) >= 0x1p24f || fabsf(y) >= 0x1p24f)
        return ab_fail(error, "Invalid TiledImage repeat endpoints");
    image->repeat_x = x;
    image->repeat_y = y;
    return true;
}

static bool lifecycle(void *context, AbUiWidget *widget, AbUiFrameMethod method,
                      AbError *error) {
    AbUiTiledImage *image = context;
    return ab_ui_widget_lifecycle_base(widget, method, error) &&
           (method != AB_UI_FRAME_LAYOUT ||
            ab_ui_tiled_image_set_image(image, image->image, error));
}

static bool draw(void *context, AbUiWidget *widget, const AbUiDrawInput *input,
                 AbError *error) {
    AbUiTiledImage *image = context;
    AbUiWidgetContext *host = widget->context;
    AbUiDrawTransform parent;
    float x = 0, y = 0, width = 0, height = 0;
    if (!host->render_state || !host->draw_list ||
        !ab_ui_draw_resolve(input, &parent) ||
        !numeric(widget->frame->fields.x, &x, error) ||
        !numeric(widget->frame->fields.y, &y, error) ||
        !numeric(widget->frame->fields.w, &width, error) ||
        !numeric(widget->frame->fields.h, &height, error))
        return ab_fail(error, "Invalid TiledImage draw context/geometry");
    x += parent.position.x;
    y += parent.position.y;
    AbSpriteRenderState *state = &host->render_state->geometry;
    state->translation_x = state->translation_y = state->angle_radians = 0;
    state->rotation_origin_x = state->rotation_origin_y = 0;
    state->scale_x = state->scale_y = 1;
    for (float row = 1; row <= image->repeat_y; row += 1) {
        for (float column = 1; column <= image->repeat_x; column += 1) {
            AbUiSpriteRender sprite = {
                .image = image->image,
                .position = {x + (column - 1) * width, y + (row - 1) * height},
                .vertical = AB_ANCHOR_VPIVOT,
                .horizontal = AB_ANCHOR_HPIVOT};
            if (!ab_ui_sprite_render(&sprite, host->render_state, &host->render,
                                     host->draw_list, error))
                return false;
        }
    }
    return ab_ui_widget_draw_base(widget, input, error);
}

bool ab_ui_tiled_image_init(AbUiTiledImage *image, AbUiWidgetContext *context,
                            AbUiWidgetChildren children,
                            const AbUiFrameFields *arguments, AbError *error) {
    if (!image || image->widget.initialized)
        return ab_fail(error, "TiledImage needs fresh caller storage");
    if (!ab_ui_widget_init(&image->widget, context, AB_UI_WIDGET_FRAME, children,
                           arguments, NULL, NULL, error))
        return false;
    image->image = (AbSpan){0};
    image->repeat_x = image->repeat_y = 1;
    image->widget.frame->fields.visible =
        (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN, .boolean = true};
    image->widget.frame->fields.w = image->widget.frame->fields.h = number(0);
    image->widget.methods =
        (AbUiWidgetMethods){.context = image, .lifecycle = lifecycle, .draw = draw};
    return true;
}
