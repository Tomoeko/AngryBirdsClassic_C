#include "ab/ui_image_button_draw.h"

static bool prepare(const AbUiImageButtonDraw *button, const AbUiDrawInput *input,
                    AbVec2 screen_size, AbUiImageDrawSink sink, void *context,
                    AbUiDrawInput *children, AbError *error) {
    if (!button || !button->image || !button->animation || !input) {
        return ab_fail(error, "Invalid ImageButton draw input");
    }
    AbUiDrawInput parent_input = *input;
    parent_input.transform.local_factor = 1;
    parent_input.present |= 32;
    AbUiDrawTransform parent;
    float factor;
    if (!ab_ui_draw_resolve(&parent_input, &parent) ||
        !ab_ui_image_button_hover_scale(button->animation, &factor)) {
        return ab_fail(error, "Invalid ImageButton draw transform or hover scale");
    }
    AbUiDrawInput self = {.transform = parent, .present = 63};
    self.transform.scale.x = self.transform.scale.x * factor;
    self.transform.scale.y = self.transform.scale.y * factor;
    AbUiImageDrawPlan plan;
    if (!ab_ui_image_draw_plan(button->image, &self, screen_size, &plan)) {
        return ab_fail(error, "Unsupported ImageButton image draw geometry");
    }
    if (plan.count) {
        if (!sink) {
            return ab_fail(error, "Missing ImageButton image draw sink");
        }
        if (!sink(context, &plan, error)) {
            return false;
        }
    }
    *children = (AbUiDrawInput){.transform = parent, .present = 63};
    children->transform.local_factor = factor;
    return true;
}

bool ab_ui_image_button_draw_self(const AbUiImageButtonDraw *button,
                                  const AbUiDrawInput *input, AbVec2 screen_size,
                                  AbUiImageDrawSink sink, void *context,
                                  AbError *error) {
    AbUiDrawInput children;
    return prepare(button, input, screen_size, sink, context, &children, error);
}

bool ab_ui_image_button_draw(const AbUiImageButtonDraw *button,
                             const AbUiDrawInput *input, AbVec2 screen_size,
                             AbUiImageDrawSink sink, void *context, AbError *error) {
    AbUiDrawInput children;
    if (!prepare(button, input, screen_size, sink, context, &children, error))
        return false;
    return ab_ui_frame_draw(&button->image->frame, button->children,
                            button->child_count, &children, NULL, NULL, error);
}
