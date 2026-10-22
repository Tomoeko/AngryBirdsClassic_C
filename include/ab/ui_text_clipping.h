#ifndef AB_UI_TEXT_CLIPPING_H
#define AB_UI_TEXT_CLIPPING_H

#include "ab/font_wrap.h"
#include "ab/ui_text_lines.h"
#include "ab/ui_text_widget.h"

typedef struct {
    AbUiTextState *lines;
    char (*names)[40];
    size_t line_capacity;
    AbFontWrappedLine *wrapped;
    AbSpan *wrapped_text;
    size_t wrapped_capacity;
    uint32_t *codes;
    size_t code_capacity;
    uint8_t *bytes;
    size_t byte_capacity;
} AbUiTextClipStorage;

struct AbUiTextClipping {
    AbUiTextClipStorage storage;
    /* Read-only root projection during clip; lines are the actual separate
     * source Text.lines list, not Frame children. Old generations stay owned. */
    AbUiTextLines generation;
    AbUiTextWidget *text;
    AbUiTextMetricBoundaries resources;
    size_t used_lines, used_bytes, next_name;
    bool initialized, busy, failed;
};

/* Bind caller-owned pools once. Text.clip constructs fresh standalone line Text
 * states in the source's lines list, leaving Frame children/parents untouched.
 * Retain old line states/names/UTF bytes across re-clipping; never recycle their
 * identities. Wrapped slices/codes are reusable workspace. All storage and the
 * borrowed real Text/widget/font/resource owners must remain stable.
 *
 * Ordinary text/group strings, builtin font methods, finite used scales and
 * bounds only. Embedded-NUL localized labels and arbitrary line-class/property
 * overrides are outside this contract. Numeric raw clipText arguments remain
 * separate from the supported native numeric draw path. No allocation, VM or
 * resource I/O. Pool exhaustion/failure preserves prefixes and stops the widget
 * context and clipping owner. Lifetime initialization has no source effects. */
bool ab_ui_text_clipping_init(AbUiTextClipping *owner, AbUiTextClipStorage storage,
                              AbError *error);
bool ab_ui_text_widget_clip(AbUiTextWidget *text, AbUiTextClipping *owner,
                            AbUiPointerValue own_scale, AbUiPointerValue spacing,
                            AbError *error);

#endif
