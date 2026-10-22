#ifndef AB_UI_TOGGLE_BUTTON_H
#define AB_UI_TOGGLE_BUTTON_H

#include "ab/lua_data.h"
#include "ab/ui_widget.h"

typedef struct {
    AbUiWidget widget;
    AbUiPointerValue state;
    AbDataTable empty_states;
    AbDataTable *states;
} AbUiToggleButton;

/* Concrete ToggleButton with numeric states and a borrowed, mutable dense array
 * of string image identifiers. State arrays/strings remain stable while bound;
 * state-image edits replace existing slots and do not create sparse tables.
 * The initial empty array is owned inline by this instance. Source init invokes
 * the ImageButton base before resetting state=1/states={}. A truthy constructor
 * image argument is an OBJECT identifying an actual AbDataTable of states; its
 * virtual setter runs during the base constructor. Source menu factories normally
 * bind their state arrays afterward. No allocation or VM; widget rules apply. */
bool ab_ui_toggle_button_init(AbUiToggleButton *button, AbUiWidgetContext *context,
                              AbUiWidgetChildren children,
                              const AbUiImageButtonArguments *arguments,
                              AbError *error);
bool ab_ui_toggle_button_set_images(AbUiToggleButton *button, AbDataTable *states,
                                    AbUiPointerValue disabled_image, AbError *error);
bool ab_ui_toggle_button_set_state(AbUiToggleButton *button, float state,
                                   AbError *error);
AbUiPointerValue ab_ui_toggle_button_get_state(const AbUiToggleButton *button);
bool ab_ui_toggle_button_get_state_image(AbUiToggleButton *button, size_t state,
                                         AbUiPointerValue *output, AbError *error);
bool ab_ui_toggle_button_set_state_image(AbUiToggleButton *button, size_t state,
                                         AbSpan image, AbError *error);

#endif
