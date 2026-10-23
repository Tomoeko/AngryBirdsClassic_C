#ifndef AB_DRAW_VERTICES_H
#define AB_DRAW_VERTICES_H

#include "ab/draw.h"

typedef struct {
    AbDrawVertex base;
    float mask_uv[4];
} AbRenderVertex;

static inline void ab_render_vertices(const AbDrawCommand *command,
                                      AbRenderVertex output[4]) {
    for (size_t vertex = 0; vertex < 4; ++vertex) {
        output[vertex].base = command->quad.vertices[vertex];
        output[vertex].mask_uv[0] = command->mask.uv[vertex * 2];
        output[vertex].mask_uv[1] = command->mask.uv[vertex * 2 + 1];
        output[vertex].mask_uv[2] = output[vertex].mask_uv[3] = 0;
    }
}

#endif
