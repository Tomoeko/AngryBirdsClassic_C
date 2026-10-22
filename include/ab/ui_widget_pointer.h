#ifndef AB_UI_WIDGET_POINTER_H
#define AB_UI_WIDGET_POINTER_H

#include "ab/ui_image_button.h"
#include "ab/ui_image_state.h"

/* Compose verified pointer handling with canonical ImageButton fields. Resolve
 * visible/image/activate/click/deferred flags by Lua truthiness. Preserve each
 * complete typed flag until the original method actually assigns it; writes
 * become Boolean immediately, before subsequent hit/audio/child callbacks.
 * Object identity and hit/audio/child geometry remain explicit boundaries.
 * Boundaries must not mutate state. No allocation or ownership transfer.
 * Failure preserves reply while retaining original state/callback prefixes.
 * Finite point/touch count and the existing pointer boundary contract apply. */
bool ab_ui_image_button_state_handle(AbUiImageButtonState *state, uintptr_t object,
                                     const AbUiImageButtonBoundaries *boundaries,
                                     AbSpan event, AbVec2 point,
                                     AbUiPointerReply *output, AbError *error);
bool ab_ui_image_button_state_pointer(AbUiImageButtonState *state, uintptr_t object,
                                      const AbUiImageButtonBoundaries *boundaries,
                                      AbSpan event, AbVec2 point,
                                      AbUiPointerReply *output, AbError *error);
void ab_ui_image_button_state_clear_flags(AbUiImageButtonState *state);
void ab_ui_image_button_state_clear_events(AbUiImageButtonState *state);

#endif
