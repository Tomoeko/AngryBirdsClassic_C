#ifndef AB_UI_IMAGE_BUTTON_DRAW_H
#define AB_UI_IMAGE_BUTTON_DRAW_H

#include "ab/ui_image_button_animation.h"
#include "ab/ui_image_draw.h"

typedef struct {
    const AbUiImageDraw *image;
    const AbUiImageButtonAnimation *animation;
    const AbUiDrawChild *children;
    size_t child_count;
} AbUiImageButtonDraw;

typedef bool (*AbUiImageDrawSink)(void *context, const AbUiImageDrawPlan *plan,
                                  AbError *error);

/* Original ImageButton.draw/drawSelf hover-enabled method route. Draw self
 * first with parent scales multiplied by hoverScale and local_factor fixed1.
 * Draw children through base Frame using unmodified parent scales and the
 * button's hoverScale as their local_factor. The incoming local_factor is
 * discarded by the original button method's shorter argument list.
 * Emit each nonempty ordered image plan before child callbacks; a culled own
 * image still traverses children. All inputs remain borrowed/stable and sink
 * and child callbacks must not mutate them. No allocation, GPU or assets.
 * Earlier callback effects remain on failure. This route is selected only
 * when the original class's hover/touch-scaling method installation applies. */
bool ab_ui_image_button_draw(const AbUiImageButtonDraw *button,
                             const AbUiDrawInput *input, AbVec2 screen_size,
                             AbUiImageDrawSink sink, void *context, AbError *error);
/* Same real drawSelf route, without visiting or reading children. */
bool ab_ui_image_button_draw_self(const AbUiImageButtonDraw *button,
                                  const AbUiDrawInput *input, AbVec2 screen_size,
                                  AbUiImageDrawSink sink, void *context,
                                  AbError *error);

#endif
