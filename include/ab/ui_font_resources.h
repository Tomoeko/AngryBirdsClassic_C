#ifndef AB_UI_FONT_RESOURCES_H
#define AB_UI_FONT_RESOURCES_H

#include "ab/font_draw.h"
#include "ab/text_resources.h"
#include "ab/ui_text_metrics.h"
#include "ab/ui_text_draw.h"
#include "ab/ui_widget.h"

typedef struct {
    AbSpan name;
    const AbBitmapFont *font;
    uint32_t texture, texture_width, texture_height;
} AbUiFontBinding;

typedef struct {
    void *context;
    bool (*call)(void *context, AbUiPointerValue *output, AbError *error);
} AbUiFontGetter;

typedef struct {
    const AbUiFontBinding *bindings;
    size_t count;
    const AbUiFontBinding *current;
    const AbTextResources *strings;
    uint32_t *codes;
    AbFontGlyphPlacement *glyphs;
    size_t capacity;
    float screen_width;
    const AbUiFontGetter *getters;
    size_t getter_count;
} AbUiFontResources;

/* Borrow actual decoded FONT resources and uploaded textures. The owner keeps
 * these and the localization catalogs stable, supplies reusable UTF/glyph
 * storage, and serializes calls with the UI renderer. Font selection is retained
 * between methods, including those which intentionally query the current font.
 * A missing name preserves that selection, as Resources.useFont does. No I/O,
 * allocation, resource substitution, or generic script/function evaluation.
 * Optional actual C font-getter records are stable and immutable; FUNCTION
 * identities are their record addresses, not executable pointers. Resolve only
 * registered identities and invoke the real callback with no script arguments.
 * Returned values/strings follow the Text constructor/setter/layout contracts. */
AbUiTextMetricBoundaries ab_ui_font_resources_boundaries(AbUiFontResources *resources);
bool ab_ui_font_resources_draw(AbUiFontResources *resources, AbUiWidgetContext *context,
                               const AbUiTextDrawPlan *plan, AbError *error);

#endif
