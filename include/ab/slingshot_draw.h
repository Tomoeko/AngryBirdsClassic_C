#ifndef AB_SLINGSHOT_DRAW_H
#define AB_SLINGSHOT_DRAW_H

#include "ab/rubberband.h"
#include "ab/image_rectangle.h"

typedef enum {
    AB_SLING_DRAW_BACK,
    AB_SLING_DRAW_HOLDER,
    AB_SLING_DRAW_FRONT,
} AbSlingshotDrawPart;

typedef enum {
    AB_SLING_SPRITE_BACK,
    AB_SLING_SPRITE_HOLDER,
    AB_SLING_SPRITE_FRONT,
    AB_SLING_SPRITE_FRONT_SCOPE,
} AbSlingshotSprite;

typedef struct {
    AbVec2 translation;
    AbVec2 scale;
    float angle;
    AbVec2 pivot;
} AbSlingshotDrawState;

typedef struct {
    AbVec2 position;
    AbVec2 stretch_position;
    float band_width;
    float band_angle;
    AbVec2 camera_top_left;
    float zoom;
    AbVec2 holder_pivot;
    bool disabled;
    bool dragged;
    bool has_scope;
} AbSlingshotDrawInput;

typedef enum {
    AB_SLING_COMMAND_STATE,
    AB_SLING_COMMAND_SPRITE,
    AB_SLING_COMMAND_BAND,
    AB_SLING_COMMAND_EXTRAS,
} AbSlingshotDrawKind;

typedef struct {
    AbSlingshotDrawKind kind;
    union {
        AbSlingshotDrawState state;
        struct {
            AbSlingshotSprite image;
            AbVec2 position;
        } sprite;
        AbRubberband band;
    };
} AbSlingshotDrawCommand;

enum { AB_SLING_DRAW_COMMANDS = 5 };
typedef struct {
    size_t count;
    AbSlingshotDrawCommand commands[AB_SLING_DRAW_COMMANDS];
} AbSlingshotDrawPlan;

/* The base Slingshot class's sprite choices, including its scope front. Other
 * slingshot classes have separate selection rules. Returns NULL for invalid ID. */
const char *ab_slingshot_sprite_name(AbSlingshotSprite sprite);

/* SlingshotSystem back/holder/front script boundaries. Band commands retain
 * world units and must execute the Android projection/native identity route;
 * state commands apply to sprite draws. Extras is an ordered caller boundary,
 * without claiming its scope/trajectory implementation. The caller selects
 * each part at its recovered place in the full scene's draw order.
 * No allocation or callbacks. Only used numeric values must be finite; disabled
 * parts emit no commands. Invalid input preserves the complete output. */
bool ab_slingshot_draw_plan(const AbSlingshotDrawInput *input, AbSlingshotDrawPart part,
                            AbSlingshotDrawPlan *output, AbError *error);

/* Ordinary base-sling sprite geometry: Resources.drawSprite's default pivot
 * anchors, followed by the normal GL_Image rectangle route. UVs are a cached
 * sprite binding; current sprite state comes from the plan above. This excludes
 * direct-vertex mode, shader/material selection and actual GPU submission.
 * No allocation. Invalid input preserves both outputs. */
bool ab_slingshot_sprite_quad(const AbSpriteRegion *region, AbVec2 position,
                              AbSlingshotDrawState state, uint32_t viewport_width,
                              uint32_t viewport_height, const float uv[8],
                              AbDrawQuad *output, bool *visible, AbError *error);

#endif
