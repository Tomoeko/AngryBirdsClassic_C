#ifndef AB_RUBBERBAND_H
#define AB_RUBBERBAND_H

#include "ab/base.h"
#include "ab/physics_geometry.h"

typedef struct {
    AbVec2 start;
    AbVec2 end;
    float width;
} AbRubberband;

/* Android GameLua.drawRubberband and its identity Sprite.draw image vertices.
 * Endpoints/width are already in screen units; resource lookup, UVs, context
 * alpha and GPU clipping remain caller boundaries. A rounded squared length of
 * zero emits nothing and leaves vertices unchanged; width is unused there.
 * Otherwise finite endpoints, differences, width and resulting vertices are
 * required. Squared-length overflow is retained: infinite length makes zero
 * offsets. Failure preserves both outputs. No allocation or renderer calls. */
bool ab_rubberband_vertices(const AbRubberband *band, AbVec2 vertices[4], bool *drawn,
                            AbError *error);

/* SlingshotSystem's Android branch subtracts the rendering camera's left/top
 * before multiplying each endpoint and width by its scalar zoom. This accepts
 * resolved camera values, including zero/negative zoom. Finite endpoints,
 * camera values and projected endpoints are required. Width is deliberately
 * unrestricted: the native geometry ignores it for a zero squared length.
 * Failure preserves output. Input and output may alias. */
bool ab_rubberband_project(const AbRubberband *world, AbVec2 camera_top_left,
                           float zoom, AbRubberband *screen, AbError *error);

/* Compose the Android script projection and native image-vertex calculation.
 * The same output preservation and finite-domain rules apply. */
bool ab_rubberband_world_vertices(const AbRubberband *world, AbVec2 camera_top_left,
                                  float zoom, AbVec2 vertices[4], bool *drawn,
                                  AbError *error);

#endif
