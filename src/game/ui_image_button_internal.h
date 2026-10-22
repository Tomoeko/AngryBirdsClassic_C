#ifndef AB_UI_IMAGE_BUTTON_INTERNAL_H
#define AB_UI_IMAGE_BUTTON_INTERNAL_H

#include "ab/ui_image_button.h"

typedef enum {
    AB_UI_BUTTON_CLICK_STARTED,
    AB_UI_BUTTON_CLEAR_POINTER_FLAGS,
} AbUiButtonFlag;

/* Observe original assignments as they happen, including redundant writes.
 * The canonical adapter uses this to preserve absent/arbitrary flags until
 * their first actual assignment and expose each prefix to later boundaries.
 * The sink must only update its separate canonical storage, without failure. */
typedef void (*AbUiButtonFlagSink)(void *context, AbUiButtonFlag flag, bool value);

bool ab_ui_image_button_dispatch(AbUiImageButton *button,
                                 const AbUiImageButtonBoundaries *boundaries,
                                 AbSpan event, AbVec2 point, bool traverse_children,
                                 AbUiButtonFlagSink sink, void *context,
                                 AbUiPointerReply *output, AbError *error);

#endif
