#ifndef AB_UI_WIDGET_DRAW_H
#define AB_UI_WIDGET_DRAW_H

#include "ab/ui_image_button_draw.h"
#include "ab/ui_image_state.h"

/* Borrow canonical widget fields as existing numeric draw views. Frame geometry
 * must be finite numbers, without another round of constructor defaults. Image
 * visibility requires Boolean true. Size, identifier and pivot fields are read
 * only for a present, visible image; numeric alpha is used whenever non-absent.
 * False/absent pivots select the existing draw defaults. Present drawable image
 * identifiers must be text. No localization, resource refresh or allocation.
 * Unused fields retain their types and contents; failed projection preserves
 * output. Input storage/text remains borrowed and must not alias output. */
bool ab_ui_frame_state_draw_view(const AbUiFrameState *state, AbUiDrawFrame *output);
bool ab_ui_image_state_draw_view(const AbUiImageState *state, AbUiImageDraw *output);

/* Original installed ImageButton draw route over canonical fields. Only enabled
 * hover reads hover_scale; other animation/pointer fields are unused. Children
 * are a prepared stable draw view. Share original self-before-children ordering,
 * transform rules and failure prefixes with ab_ui_image_button_draw. No state
 * writes, lifecycle calls or ownership transfer. Full scene traversal and GPU
 * application remain caller boundaries. */
bool ab_ui_image_button_state_draw(const AbUiImageButtonState *state,
                                   const AbUiDrawChild *children, size_t child_count,
                                   const AbUiDrawInput *input, AbVec2 screen_size,
                                   AbUiImageDrawSink sink, void *context,
                                   AbError *error);
bool ab_ui_image_button_state_draw_self(const AbUiImageButtonState *state,
                                        const AbUiDrawInput *input, AbVec2 screen_size,
                                        AbUiImageDrawSink sink, void *context,
                                        AbError *error);

#endif
