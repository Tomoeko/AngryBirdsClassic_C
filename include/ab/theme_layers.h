#ifndef AB_THEME_LAYERS_H
#define AB_THEME_LAYERS_H

#include "ab/composite_resources.h"
#include "ab/theme_colors.h"
#include "ab/theme_foreground.h"
#include "ab/theme_layer_data.h"
#include "ab/sprite_provider.h"
#include "ab/theme_rect.h"

typedef struct AbThemeLayers AbThemeLayers;

typedef enum {
    AB_THEME_SPRITE_MISSING,
    AB_THEME_SPRITE_ORDINARY,
    AB_THEME_SPRITE_COMPOSITE,
    AB_THEME_SPRITE_BORDER,
    AB_THEME_SPRITE_RECT,
} AbThemeSpriteKind;

typedef struct {
    AbThemeLayerDefinition definition;
    AbThemeSpriteKind kind;
    AbSpriteBinding ordinary;
    AbResolvedComposite composite;
    float border_size, border_alpha;
    AbThemeRect rect;
} AbThemeLayerBinding;

/* Initialize owned base-layer bindings/handles from static bgLayers/fgLayers.
 * Visit consecutive table-valued integer slots starting at1, stopping at the
 * first other value. Support default/unknown base types, rectangle layers and
 * screen borders; screen/menu_particles subtypes remain unsupported. Borders retain
 * their named sprite, size and alpha. Resolve ordinary
 * sprites before composites; absent names retain native missing-base behavior.
 * Numeric fields follow the static constructor/color contracts. Data graph,
 * sprite set and optional composite owner remain borrowed until close. The limit
 * bounds all retained allocations. Output starts NULL; failures release all new
 * storage. No VM, pixels or GPU objects. Dynamic subtype/overlay mutations and
 * native group/variant policy remain separate. */
bool ab_theme_layers_open(const AbDataTable *theme, const AbSpriteSet *sprites,
                          const AbCompositeResources *composites,
                          size_t allocation_limit, AbThemeLayers **output,
                          AbError *error);
/* Same constructor over the live resource registry. Cache its actual ordinary
 * or composite selections; resource owners must remain stable until close.
 * Construction performs no pixel decoding or texture upload. */
bool ab_theme_layers_open_provider(const AbDataTable *theme,
                                   const AbSpriteProvider *provider,
                                   size_t allocation_limit, AbThemeLayers **output,
                                   AbError *error);
void ab_theme_layers_close(AbThemeLayers *layers);
size_t ab_theme_layers_count(const AbThemeLayers *layers);
size_t ab_theme_layers_bytes(const AbThemeLayers *layers);
const AbThemeColors *ab_theme_layers_colors(const AbThemeLayers *layers);

/* Handles are stable1-based binding indices within this owner. Views borrow all
 * owners above. Queries allocate/decode nothing and preserve output on failure.
 * Foreground first-layer metrics describe its actual base class, including a
 * missing base sprite. Background view is for the no-spotlight composition path.
 * Drawing, reset-state ownership and GPU output remain caller/provider work. */
bool ab_theme_layers_binding(const AbThemeLayers *layers, uintptr_t handle,
                             const AbThemeLayerBinding **output);
bool ab_theme_layers_background(const AbThemeLayers *layers, AbThemeBackground *output);
bool ab_theme_layers_foreground(const AbThemeLayers *layers, AbThemeForeground *output);

typedef struct {
    AbCoordinateCamera camera;
    int32_t viewport_width;
    float time;
    size_t placement_limit;
    /* Preserve integer DAT attachment coordinates at adjoining scenery edges.
     * False retains the original add-half/truncate getter for native probes. */
    bool align_integer_pivots;
} AbThemeLayerDrawInput;

typedef bool (*AbThemeLayerImage)(void *context, const AbSpriteBinding *sprite,
                                  const AbVec2 vertices[4], const float tint[4],
                                  AbError *error);

/* Draw one cached base layer through ordinary sprites or ordered composite
 * parts. Missing base sprites leave state untouched and emit nothing. Present
 * layers use the verified placement/pivot paths; placement_limit bounds repeats,
 * each of which can emit all composite parts. Image providers borrow cached
 * bindings and already transformed points. No lookup, allocation, decoding or
 * I/O during traversal. Preserve preceding state/provider writes on failure.
 * Dynamic overlays, image clipping, shaders and GPU output remain separate. */
bool ab_theme_layers_draw_base(const AbThemeLayers *layers, uintptr_t handle,
                               const AbThemeLayerDrawInput *input,
                               AbThemeLayerRenderState *state, AbThemeLayerImage image,
                               void *context, AbError *error);

/* ScreenBorderThemeLayer's four ordered, rotated point quads. A nonzero filter
 * suppresses every effect. Otherwise reset geometry/alpha before sprite lookup;
 * missing sprites emit nothing after that reset. The image callback must use
 * DrawUtils' explicit alpha shader, including on an opaque source texture.
 * Viewport dimensions are positive signed integers. No camera or allocation. */
bool ab_theme_layers_draw_border(const AbThemeLayers *layers, uintptr_t handle,
                                 uint32_t width, uint32_t height, uint32_t filter,
                                 AbThemeLayerRenderState *state,
                                 AbThemeLayerImage image, void *context,
                                 AbError *error);

#endif
