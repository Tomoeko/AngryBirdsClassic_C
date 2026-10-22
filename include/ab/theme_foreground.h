#ifndef AB_THEME_FOREGROUND_H
#define AB_THEME_FOREGROUND_H

#include "ab/game_rectangle.h"
#include "ab/theme_background.h"

typedef struct {
    bool is_base_layer;
    float height, pivot_y, scale;
} AbThemeForegroundFirstLayer;

typedef struct {
    float ground_color[3];
    const uintptr_t *layers;
    size_t count;
    AbThemeForegroundFirstLayer first;
} AbThemeForeground;

typedef struct {
    AbCoordinateCamera camera;
    int32_t viewport_width, viewport_height;
    uint32_t filter;
} AbThemeForegroundInput;

typedef struct {
    void *context;
    bool (*fill)(void *context, const AbGameRectanglePlan *plan, AbError *error);
    AbThemeLayerDraw layer;
} AbThemeForegroundBoundaries;

/* Native foreground composition. If the first entry is a ThemeLayer, truncate
 * height-pivot.y to int32 and use its layer scale for the ground edge; other
 * types/empty lists use0 and1. Floor -(camera.top-units*scale)*camera.zoom.y.
 * Emit the GameLua ground rectangle plan first, then visit stable borrowed layer
 * handles in order with unchanged filter. Fill provider owns applying the plan's
 * full reset request. Completed traversal resets render alpha to1. Camera x fields
 * and unused first-layer metadata are ignored. Used values must be finite and
 * within integer/rectangle domains; positive viewport required. No allocation.
 * Retain earlier provider/state writes on failure. Construction, actual fill,
 * subtype bodies, resource ownership and GPU pixels are separate. */
bool ab_theme_foreground_draw(const AbThemeForeground *foreground,
                              const AbThemeForegroundInput *input,
                              AbThemeLayerRenderState *render,
                              const AbThemeForegroundBoundaries *boundaries,
                              AbError *error);

#endif
