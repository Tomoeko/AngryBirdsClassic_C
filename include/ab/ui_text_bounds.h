#ifndef AB_UI_TEXT_BOUNDS_H
#define AB_UI_TEXT_BOUNDS_H

#include "ab/ui_text_metrics.h"

typedef struct {
    void *context;
    bool (*query)(void *context, AbVec2 point, AbUiPointerValue *output,
                  AbError *error);
} AbUiTextBoundsChild;

/* Original Text.getCorrectionParams. Query current font leading directly,
 * without selecting self.font or applying scales/fontLeadingScale. HCENTER and
 * RIGHT shift by stored width; VCENTER and BOTTOM shift by raw leading. Other
 * typed anchor values leave zero offsets. Inspect width only for those two
 * horizontal anchors. Finite used numbers/results, borrowed stable inputs,
 * no allocation. Failure preserves output and earlier provider effects. */
bool ab_ui_text_correction(const AbUiTextState *state,
                           const AbUiTextMetricBoundaries *boundaries, AbVec2 *output,
                           AbError *error);

/* Original Text.checkBounds over a prepared dense child array. First localize
 * raw self.text and measure it using the current font (no tostring or font
 * selection), increasing stored width only on a strict comparison. For truthy
 * clipped, call children forwards with the unchanged point; a truthy response
 * returns Boolean true, exhaustion returns absent. Child visibility is ignored.
 * Otherwise query raw font leading, call correction (querying leading again),
 * and test inclusive unscaled bounds in Y then X order. Leaf returns Boolean.
 * Fields/children/providers remain stable during calls except this width update.
 * Leaf positions and used arithmetic must be finite. Clipped count <=2^24;
 * children/count are unused for leaves. No allocation, owned tree or metatables.
 * Failure preserves output; prior width and callback effects remain. */
bool ab_ui_text_check_bounds(AbUiTextState *state, AbUiPointerValue clipped,
                             const AbUiTextBoundsChild *children, size_t count,
                             AbVec2 point, const AbUiTextMetricBoundaries *boundaries,
                             AbUiPointerValue *output, AbError *error);

#endif
