#ifndef AB_UI_ANIMATED_IMAGE_H
#define AB_UI_ANIMATED_IMAGE_H

#include "ab/lua_data.h"
#include "ab/ui_widget.h"

typedef struct {
    AbUiWidget widget;
    const AbDataTable *frames;
    AbDataTable empty_frames;
    AbUiPointerValue timer, speed, looping, stopped, current_frame;
    bool initialized;
} AbUiAnimatedImage;

/* Actual AnimatedImage.lua around the concrete Image base. Dense frame tables,
 * frame strings and caller storage stay borrowed. Fields above are canonical:
 * setLooping/setSpeed assign looping/speed directly; animate assigns false to
 * stopped without resetting timer/current_frame. setFrames publishes its table
 * before resetting timer, retaining stopped/current_frame. No allocation or VM.
 *
 * Update visits real children before inspecting stopped; looping subtracts one
 * exactly once even for a large delta. A nonlooping end sets frame zero. Drawing
 * refreshes the selected image even when hidden, with empty-string fallback for
 * missing/false frames. Used numeric fields must be finite; setCurrentFrame
 * requires a nonempty dense list. Reached failures stop the widget context. */
bool ab_ui_animated_image_init(AbUiAnimatedImage *image, AbUiWidgetContext *context,
                               AbUiWidgetChildren children,
                               const AbUiImageArguments *arguments, AbError *error);
bool ab_ui_animated_image_frames(AbUiAnimatedImage *image, const AbDataTable *frames,
                                 AbError *error);
bool ab_ui_animated_image_current(AbUiAnimatedImage *image, float frame,
                                  AbError *error);

#endif
