#ifndef AB_UI_INIT_H
#define AB_UI_INIT_H

#include "ab/ui_pointer.h"

typedef struct {
    AbUiPointerValue name;
    AbUiPointerValue x, y;
    AbUiPointerValue scale_x, scale_y;
    AbUiPointerValue angle;
    AbUiPointerValue w, h;
    AbUiPointerValue visible, active;
    /* Class-owned properties; base Frame.init leaves these fields untouched. */
    AbUiPointerValue require_input;
    AbUiPointerValue notifications_frame_pause;
    AbUiPointerValue pause_page_show;
} AbUiFrameFields;

typedef struct {
    AbUiFrameFields fields;
    size_t child_count, lookup_count;
} AbUiFrameState;

typedef struct {
    AbUiPointerValue text, group, box_size;
    AbUiPointerValue horizontal_anchor, vertical_anchor, font;
} AbUiTextArguments;

typedef struct {
    AbUiFrameState frame;
    AbUiTextArguments fields;
    AbUiPointerValue width, floor_coordinates, leading_scale, font_getter;
} AbUiTextState;

typedef struct {
    void *context;
    bool (*default_name)(void *context, AbUiPointerValue *output, AbError *error);
    bool (*set_font)(void *context, AbUiPointerValue font, AbError *error);
    bool (*localize)(void *context, AbUiPointerValue group, AbUiPointerValue text,
                     AbUiPointerValue *output, AbError *error);
    bool (*measure)(void *context, AbUiPointerValue text, AbUiPointerValue *output,
                    AbError *error);
    bool (*call_font)(void *context, uintptr_t function, AbUiPointerValue *output,
                      AbError *error);
    AbUiPointerValue screen_width, basic_font;
    bool font_setting_available;
} AbUiInitBoundaries;

/* Original Frame.init field assignment over plain, stable argument fields.
 * NULL arguments cover absent/false argument tables. Reset logical child and
 * lookup counts; their storage and owned children remain the caller's concern.
 * All fields retain their complete supported typed values. Only absent/false
 * default geometry/name; visible/active default only when absent. Default name
 * is an explicit boundary because original tostring(self) contains an address.
 * No allocation, generic property copy, metatable or script execution. Inputs
 * must not alias output. Earlier state and callback effects remain on failure. */
bool ab_ui_frame_init(AbUiFrameState *state, const AbUiFrameFields *arguments,
                      const AbUiInitBoundaries *boundaries, AbError *error);

/* Original Text.init after Frame.init. A function font stores its identity and
 * calls it without arguments through call_font. Other font arguments preserve
 * a truthy existing font and the existing font_getter field. A false/absent
 * getter result falls back to the supplied function itself. Text resets both
 * scales to one, visibility/floorCoordinates to true and fontLeadingScale to
 * one, regardless of arguments. Optional font selection precedes localization
 * (unless group is empty text) and measurement. Their returned values retain
 * their types; width is zero when font setting is unavailable. Borrowed text
 * and object identities must outlive the state. No resource loading or layout. */
bool ab_ui_text_init(AbUiTextState *state, const AbUiFrameFields *frame_arguments,
                     const AbUiTextArguments *text_arguments,
                     const AbUiInitBoundaries *boundaries, AbError *error);

/* Original Text.setFont/layout. setFont(function) stores the getter before
 * calling it; other values replace font without clearing an older getter.
 * layout calls any non-absent getter, including a false value in the original;
 * this plain-value contract rejects non-function getters without side effects.
 * Setter/layout getter results have no constructor fallback. No resource font
 * selection or width refresh occurs here. Callback results/text remain borrowed;
 * callback failure retains earlier writes and the previous font. */
bool ab_ui_text_set_font(AbUiTextState *state, AbUiPointerValue font,
                         const AbUiInitBoundaries *boundaries, AbError *error);
bool ab_ui_text_layout(AbUiTextState *state, const AbUiInitBoundaries *boundaries,
                       AbError *error);

#endif
