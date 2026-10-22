#ifndef AB_UI_TEXT_LINES_H
#define AB_UI_TEXT_LINES_H

#include "ab/ui_init.h"

typedef struct {
    const AbSpan *lines;
    size_t count;
    float widest_line;
} AbUiWrappedText;

typedef struct {
    AbUiTextState text;
    AbUiTextState *lines;
    size_t count, capacity;
    float text_block_height, widest_line;
    bool clipped;
} AbUiTextLines;

typedef struct {
    AbUiInitBoundaries init;
    bool (*cumulative_scale)(void *context, AbVec2 *output, AbError *error);
    bool (*wrap)(void *context, AbUiPointerValue group, AbUiPointerValue text,
                 float width, AbUiWrappedText *output, AbError *error);
    bool (*font_leading)(void *context, float *output, AbError *error);
} AbUiTextLineBoundaries;

/* Original Text.clip composition with explicit font/wrap/metric boundaries.
 * Always query cumulative scale first, even when own_scale is truthy. Divide
 * textBoxSize by selected X scale; line height is leading*Y*leadingScale*spacing
 * in that order. Absent/false spacing defaults to one; zero remains zero.
 * Supplied arguments, scales and arithmetic results must be finite numbers;
 * zero X scale and counts above 2^24 reject. Root anchors retain typed values.
 * Fresh line Text constructors receive the parent font/anchors and successive
 * Y positions. Their group defaults to TEXTS_BASIC, scales to one and visibility
 * to true. The supplied height argument is ignored by those constructors.
 * Font setting must be available for this method and its line constructors.
 * No allocation or Lua. Lines, resource text and callback output are borrowed;
 * providers must keep them stable for the resulting state. Line capacity is
 * caller-owned. Failure preserves completed prefix effects and lines. */
bool ab_ui_text_lines_build(AbUiTextLines *state, AbUiPointerValue own_scale,
                            AbUiPointerValue spacing,
                            const AbUiTextLineBoundaries *boundaries, AbError *error);

#endif
