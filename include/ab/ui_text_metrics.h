#ifndef AB_UI_TEXT_METRICS_H
#define AB_UI_TEXT_METRICS_H

#include "ab/ui_init.h"

typedef struct {
    AbUiInitBoundaries resources;
    bool (*leading)(void *context, float *output, AbError *error);
    /* Optional custom tostring boundary. NULL uses ab_ui_value_text for plain
     * nil/Boolean/number/text. Returned bytes are borrowed through this call. */
    bool (*stringify)(void *context, AbUiPointerValue value, AbSpan *output,
                      AbError *error);
} AbUiTextMetricBoundaries;

/* Original builtin Text.getFontLeading: select self.font, query current font
 * leading, then multiply fontLeadingScale. These methods require font setting
 * to be available. Numeric inputs/results are finite binary32; no allocation.
 * Output is preserved on failure; earlier resource effects remain. */
bool ab_ui_text_get_font_leading(const AbUiTextState *state,
                                 const AbUiTextMetricBoundaries *boundaries,
                                 float *output, AbError *error);

/* Original builtin Text.getHeight with a prepared dense line count. Absent or
 * false scaleY defaults to one. For truthy clipped, multiply count by the
 * builtin getFontLeading result, then by scaleY; otherwise just by scaleY.
 * Count is inspected only when clipped is truthy and must be <=2^24. This
 * query ignores stored textBlockHeight and the earlier clip spacing argument.
 * Custom getFontLeading overrides/metatables are outside this contract. */
bool ab_ui_text_get_height(const AbUiTextState *state, AbUiPointerValue clipped,
                           size_t line_count,
                           const AbUiTextMetricBoundaries *boundaries, float *output,
                           AbError *error);

/* Original Text.getWidth: resolve scaleX's absent/false default, select font,
 * then return positive zero immediately for an empty-text self.text value.
 * Otherwise tostring self.text, localize even when self.group is empty, measure
 * the result, then multiply scaleX. Does not use stored width/widestLine or
 * invoke a stored font getter. Plain numeric conversion requires C numeric
 * locale/nearest rounding. Callback values/text remain borrowed through the
 * query; no state fields are assigned. Output is preserved on failure. */
bool ab_ui_text_get_width(const AbUiTextState *state,
                          const AbUiTextMetricBoundaries *boundaries, float *output,
                          AbError *error);

#endif
