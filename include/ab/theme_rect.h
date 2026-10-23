#ifndef AB_THEME_RECT_H
#define AB_THEME_RECT_H

#include "ab/game_rectangle.h"
#include "ab/lua_data.h"

typedef struct {
    float color[4];
    float left, bottom, width, height;
} AbThemeRect;

/* RectThemeLayer constructor (BN0x2f491c). Read its required nested rect table,
 * normalize r/g/b/a by the native binary32 reciprocal255, and retain relLeft/
 * relBottom/relWidth/relHeight with defaults0/0/1/1. Plain finite typed data;
 * numeric-string conversions are outside this reader. No resources/allocation. */
bool ab_theme_rect_read(const AbDataTable *layer, AbThemeRect *output, AbError *error);

/* RectThemeLayer.draw (BN0x2f2110) reaches GameLua.drawRect only for alpha>0.
 * Preserve the source relative-bottom field's actual top-down Y calculation,
 * multiply/add rounding, and the existing rectangle color/extent conversion.
 * Active draws request a full render-state reset; inactive success preserves
 * the plan. Positive int32 viewport and finite data required. */
bool ab_theme_rect_plan(const AbThemeRect *rect, uint32_t width, uint32_t height,
                        AbGameRectanglePlan *output, bool *active, AbError *error);

#endif
