#include "ab/common_renderer.h"

#include "ab_common_shaders.h"
#include "console_common/platform/indexed.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

struct AbCommonRenderer {
    CcIndexedRenderer *native;
    CcIndexedProgram *programs[AB_BLEND_MULTIPLY + 1];
    CcIndexedMesh *mesh;
    CcIndexedTexture **textures;
    AbDrawVertex *vertices;
    size_t draw_capacity;
    size_t texture_capacity;
};

static bool diagnostic(AbError *error, const char *message) {
    return ab_fail(error, "Common renderer: %s", message);
}

static CcIndexedState blend_state(AbBlend blend) {
    CcIndexedState state = {.blend = blend != AB_BLEND_OPAQUE,
                            .source_rgb = CC_INDEXED_ONE,
                            .source_alpha = CC_INDEXED_ONE,
                            .depth_compare = CC_INDEXED_ALWAYS,
                            .color_write = {true, true, true, true}};
    if (blend == AB_BLEND_PREMULTIPLIED || blend == AB_BLEND_STRAIGHT) {
        state.destination_rgb = state.destination_alpha =
            CC_INDEXED_INVERSE_SOURCE_ALPHA;
    }
    if (blend == AB_BLEND_STRAIGHT) {
        state.source_rgb = state.source_alpha = CC_INDEXED_SOURCE_ALPHA;
    } else if (blend == AB_BLEND_ADDITIVE) {
        state.destination_rgb = state.destination_alpha = CC_INDEXED_ONE;
    } else if (blend == AB_BLEND_MULTIPLY) {
        state.source_rgb = state.source_alpha = CC_INDEXED_DESTINATION_COLOR;
    }
    return state;
}

static bool create_programs(AbCommonRenderer *renderer, AbError *error) {
    CcIndexedProgramDescription description = {
        .gles_vertex_source = ab_common_vertex_source,
        .gles_fragment_source = ab_common_fragment_source,
        .metal_vertex_source = ab_common_metal_source,
        .metal_fragment_source = ab_common_metal_source,
        .metal_vertex_entry = "ab_common_vertex",
        .metal_fragment_entry = "ab_common_fragment",
        .attributes = {{"sprite_vertex", 0}},
        .attribute_count = 1,
        .vertex_stride = sizeof(AbDrawVertex),
        .vertex_uniforms = {"sprite_viewport"},
        .vertex_uniform_count = 1,
        .fragment_uniforms = {"sprite_color", "sprite_opacity", "sprite_clip"},
        .fragment_uniform_count = 3,
        .texture_uniforms = {"sprite_texture"},
        .texture_count = 1,
        .metal_vertex_uniform_buffer = 1,
        .metal_fragment_uniform_buffer = 2};
    for (AbBlend blend = AB_BLEND_OPAQUE; blend <= AB_BLEND_MULTIPLY; ++blend) {
        char message[256] = {0};
        description.state = blend_state(blend);
        renderer->programs[blend] = cc_indexed_program_create(
            renderer->native, &description, message, sizeof(message));
        if (!renderer->programs[blend]) {
            return diagnostic(error, message);
        }
    }
    return true;
}

bool ab_common_create(CcPlatform *platform, size_t draw_capacity,
                      size_t texture_capacity, size_t allocation_limit,
                      AbCommonRenderer **output, AbError *error) {
    if (!output) {
        return ab_fail(error, "Missing Common renderer output");
    }
    *output = NULL;
    size_t per_draw = sizeof(AbDrawVertex) * 4 + sizeof(uint16_t) * 6;
    if (!platform || !draw_capacity || draw_capacity > 16384 ||
        texture_capacity > UINT32_MAX || allocation_limit < sizeof(AbCommonRenderer) ||
        draw_capacity > (allocation_limit - sizeof(AbCommonRenderer)) / per_draw) {
        return ab_fail(error, "Invalid Common renderer capacity or allocation limit");
    }
    size_t remaining =
        allocation_limit - sizeof(AbCommonRenderer) - draw_capacity * per_draw;
    if (texture_capacity > remaining / sizeof(CcIndexedTexture *)) {
        return ab_fail(error, "Common texture slots exceed allocation limit");
    }
    bool okay = false;
    AbCommonRenderer *renderer = calloc(1, sizeof(*renderer));
    uint16_t *indices = malloc(draw_capacity * 6 * sizeof(*indices));
    if (!renderer || !indices) {
        ab_fail(error, "Cannot allocate Common renderer or indices");
        goto release_renderer;
    }
    renderer->draw_capacity = draw_capacity;
    renderer->texture_capacity = texture_capacity;
    renderer->vertices = calloc(draw_capacity * 4, sizeof(*renderer->vertices));
    if (texture_capacity) {
        renderer->textures = calloc(texture_capacity, sizeof(*renderer->textures));
    }
    char message[256] = {0};
    renderer->native = cc_indexed_create(platform, message, sizeof(message));
    if (!renderer->vertices || (texture_capacity && !renderer->textures) ||
        !renderer->native) {
        diagnostic(error, message[0] ? message : "cannot allocate scene storage");
        goto release_renderer;
    }
    static const uint16_t triangle[] = {0, 1, 2, 2, 1, 3};
    for (size_t draw = 0; draw < draw_capacity; ++draw) {
        for (size_t index = 0; index < 6; ++index) {
            indices[draw * 6 + index] = (uint16_t)(draw * 4 + triangle[index]);
        }
    }
    CcIndexedMeshDescription mesh = {.vertices = renderer->vertices,
                                     .vertex_count = draw_capacity * 4,
                                     .vertex_stride = sizeof(AbDrawVertex),
                                     .indices = indices,
                                     .index_count = draw_capacity * 6};
    renderer->mesh = cc_indexed_mesh_create_dynamic(renderer->native, &mesh, message,
                                                    sizeof(message));
    if (!renderer->mesh ||
        !cc_indexed_reserve(renderer->native, draw_capacity, message,
                            sizeof(message)) ||
        !cc_indexed_reserve_passes(renderer->native, 1, message, sizeof(message)) ||
        !create_programs(renderer, error)) {
        if (message[0]) {
            diagnostic(error, message);
        }
        goto release_renderer;
    }
    *output = renderer;
    okay = true;

release_renderer:
    free(indices);
    if (!okay) {
        ab_common_free(renderer);
    }
    return okay;
}

bool ab_common_upload(AbCommonRenderer *renderer, uint32_t texture,
                      const AbImage *image, bool repeat_s, bool repeat_t,
                      AbError *error) {
    if (!renderer || !texture || texture > renderer->texture_capacity || !image ||
        !image->width || !image->height || !image->rgba.data ||
        (size_t)image->width > SIZE_MAX / 4 / image->height ||
        image->rgba.size != (size_t)image->width * image->height * 4) {
        return ab_fail(error, "Invalid Common texture upload");
    }
    CcIndexedTextureDescription description = {
        .levels = {{image->rgba.data, image->rgba.size, image->width, image->height}},
        .level_count = 1,
        .min_filter = CC_INDEXED_LINEAR,
        .mag_filter = CC_INDEXED_LINEAR,
        .wrap_s = repeat_s ? CC_INDEXED_REPEAT : CC_INDEXED_CLAMP,
        .wrap_t = repeat_t ? CC_INDEXED_REPEAT : CC_INDEXED_CLAMP,
        .max_anisotropy = 1};
    char message[256] = {0};
    CcIndexedTexture *replacement = cc_indexed_texture_create(
        renderer->native, &description, message, sizeof(message));
    if (!replacement) {
        return diagnostic(error, message);
    }
    CcIndexedTexture **slot = renderer->textures + texture - 1;
    if (*slot &&
        !cc_indexed_texture_release(renderer->native, slot, message, sizeof(message))) {
        cc_indexed_texture_release(renderer->native, &replacement, NULL, 0);
        return diagnostic(error, message);
    }
    *slot = replacement;
    return true;
}

bool ab_common_unload(AbCommonRenderer *renderer, uint32_t texture, AbError *error) {
    if (!renderer || !texture || texture > renderer->texture_capacity) {
        return ab_fail(error, "Invalid Common texture release slot");
    }
    char message[256] = {0};
    return cc_indexed_texture_release(renderer->native,
                                      renderer->textures + texture - 1, message,
                                      sizeof(message)) ||
           diagnostic(error, message);
}

static bool render_view(AbCommonRenderer *renderer, const AbDrawList *list,
                        const float clear[4], int width, int height, bool native,
                        AbError *error) {
    if (!renderer || !list || !clear || width <= 0 || height <= 0 ||
        list->count > list->capacity || list->count > renderer->draw_capacity ||
        (list->count && !list->commands)) {
        return ab_fail(error, "Invalid Common frame or command capacity");
    }
    for (size_t channel = 0; channel < 4; ++channel) {
        if (!isfinite(clear[channel]) || clear[channel] < 0 || clear[channel] > 1) {
            return ab_fail(error, "Invalid Common clear color");
        }
    }
    for (size_t draw = 0; draw < list->count; ++draw) {
        const AbDrawCommand *command = list->commands + draw;
        if (!ab_draw_command_check(command, error) || !command->texture ||
            command->texture > renderer->texture_capacity ||
            !renderer->textures[command->texture - 1]) {
            return ab_fail(error, "Invalid Common draw at index %zu", draw);
        }
        for (size_t channel = 0; channel < 4; ++channel) {
            if (!isfinite(command->color[channel])) {
                return ab_fail(error, "Invalid Common draw color");
            }
        }
        memcpy(renderer->vertices + draw * 4, command->quad.vertices,
               sizeof(command->quad.vertices));
    }
    char message[256] = {0};
    if (!cc_indexed_mesh_update(renderer->native, renderer->mesh, renderer->vertices,
                                renderer->draw_capacity * 4 * sizeof(AbDrawVertex),
                                message, sizeof(message))) {
        return diagnostic(error, message);
    }
    CcIndexedFrame frame = {.clear_color = {clear[0], clear[1], clear[2], clear[3]},
                            .clear_color_enabled = true};
    bool begun =
        native ? cc_indexed_begin_passes(renderer->native, &frame, message,
                                         sizeof(message))
               : cc_indexed_begin(renderer->native, &frame, message, sizeof(message));
    if (!begun)
        return diagnostic(error, message);
    if (native) {
        CcIndexedPass pass = {.viewport = {0, 0, width, height},
                              .color_load = CC_INDEXED_CLEAR,
                              .clear_color = frame.clear_color};
        if (!cc_indexed_pass_begin(renderer->native, &pass, message, sizeof(message)))
            return diagnostic(error, message);
    }
    bool okay = true;
    for (size_t draw = 0; draw < list->count; ++draw) {
        const AbDrawCommand *command = list->commands + draw;
        AbDrawScissor scissor;
        if (!ab_draw_clip_scissor(command->scissor, (uint32_t)width, (uint32_t)height,
                                  &scissor, error)) {
            okay = false;
            break;
        }
        if (!scissor.width || !scissor.height)
            continue;
        const float viewport[1][4] = {
            {(float)width, (float)height, command->coordinates == AB_DRAW_CLIP, 0}};
        const float horizontal = 2.0f / (float)width;
        const float vertical = -2.0f / (float)height;
        const float parameters[3][4] = {
            {command->color[0], command->color[1], command->color[2],
             command->color[3]},
            {command->opacity, command->scissor.enabled ? 1 : 0, 0, 0},
            {-1.0f + horizontal * (float)scissor.x, 1.0f + vertical * (float)scissor.y,
             -1.0f + horizontal * (float)(scissor.x + scissor.width),
             1.0f + vertical * (float)(scissor.y + scissor.height)}};
        CcIndexedDraw submission = {
            .program = renderer->programs[command->blend],
            .mesh = renderer->mesh,
            .vertex_uniforms = viewport,
            .vertex_uniform_count = 1,
            .fragment_uniforms = parameters,
            .fragment_uniform_count = 3,
            .textures = {renderer->textures[command->texture - 1]},
            .texture_count = 1,
            .first_index = draw * 6,
            .index_count = 6};
        if (!cc_indexed_draw(renderer->native, &submission, message, sizeof(message))) {
            okay = false;
            break;
        }
    }
    bool ended = cc_indexed_end(renderer->native, message, sizeof(message));
    return (okay && ended) || diagnostic(error, message);
}

bool ab_common_render(AbCommonRenderer *renderer, const AbDrawList *list,
                      const float clear[4], AbError *error) {
    return render_view(renderer, list, clear, CC_FRAME_WIDTH, CC_FRAME_HEIGHT, false,
                       error);
}

bool ab_common_render_native(AbCommonRenderer *renderer, const AbDrawList *list,
                             const float clear[4], int width, int height,
                             AbError *error) {
    return render_view(renderer, list, clear, width, height, true, error);
}

bool ab_common_wait(AbCommonRenderer *renderer, AbError *error) {
    if (!renderer) {
        return ab_fail(error, "Missing Common renderer");
    }
    char message[256] = {0};
    return cc_indexed_wait(renderer->native, message, sizeof(message)) ||
           diagnostic(error, message);
}

void ab_common_free(AbCommonRenderer *renderer) {
    if (renderer) {
        cc_indexed_destroy(renderer->native);
        free(renderer->vertices);
        free(renderer->textures);
        free(renderer);
    }
}
