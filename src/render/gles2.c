#include "ab/gles2.h"
#include "draw_vertices.h"

#include "ab_sprite_shaders.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

enum {
    GL_VERTEX_STAGE = 0x8b31,
    GL_FRAGMENT_STAGE = 0x8b30,
    GL_COMPILE_RESULT = 0x8b81,
    GL_LINK_RESULT = 0x8b82,
    GL_HIGH_FLOAT_FORMAT = 0x8df2,
    GL_TEXTURE_LIMIT = 0x0d33,
    GL_VERTEX_BUFFER = 0x8892,
    GL_STREAM_USAGE = 0x88e0,
    GL_FLOAT_DATA = 0x1406,
    GL_TEXTURE_UNIT_ZERO = 0x84c0,
    GL_TEXTURE_2D_TARGET = 0x0de1,
    GL_UNPACK_ALIGNMENT_BYTES = 0x0cf5,
    GL_MIN_FILTER = 0x2801,
    GL_MAG_FILTER = 0x2800,
    GL_LINEAR_FILTER = 0x2601,
    GL_WRAP_S = 0x2802,
    GL_WRAP_T = 0x2803,
    GL_REPEAT_WRAP = 0x2901,
    GL_CLAMP_WRAP = 0x812f,
    GL_RGBA_CHANNELS = 0x1908,
    GL_BYTE_DATA = 0x1401,
    GL_DEPTH_STATE = 0x0b71,
    GL_CULL_STATE = 0x0b44,
    GL_STENCIL_STATE = 0x0b90,
    GL_SCISSOR_STATE = 0x0c11,
    GL_BLEND_STATE = 0x0be2,
    GL_ADD_EQUATION = 0x8006,
    GL_SOURCE_ALPHA_FACTOR = 0x0302,
    GL_INVERSE_ALPHA_FACTOR = 0x0303,
    GL_DESTINATION_COLOR_FACTOR = 0x0306,
    GL_TRIANGLE_STRIP_MODE = 5,
};

struct AbGles2 {
    AbGles2Api api;
    uint32_t program;
    uint32_t buffer;
    uint32_t *textures;
    size_t texture_capacity;
    int32_t maximum_texture_size;
    int32_t size_uniform;
    int32_t texture_uniform;
    int32_t color_uniform;
    int32_t opacity_uniform;
    int32_t coordinates_uniform;
    int32_t mask_texture_uniform, mask_enabled_uniform;
    bool high_precision_uv;
};

static bool complete_api(const AbGles2Api *api) {
    return api && api->CreateShader && api->ShaderSource && api->CompileShader &&
           api->GetShaderiv && api->GetShaderInfoLog && api->DeleteShader &&
           api->CreateProgram && api->AttachShader && api->BindAttribLocation &&
           api->LinkProgram && api->GetProgramiv && api->GetProgramInfoLog &&
           api->DeleteProgram && api->GetUniformLocation && api->UseProgram &&
           api->Uniform1i && api->Uniform1f && api->Uniform2f && api->Uniform4fv &&
           api->GetShaderPrecisionFormat && api->GetIntegerv && api->GetError &&
           api->GenBuffers && api->BindBuffer && api->BufferData &&
           api->BufferSubData && api->DeleteBuffers && api->EnableVertexAttribArray &&
           api->VertexAttribPointer && api->GenTextures && api->ActiveTexture &&
           api->BindTexture && api->PixelStorei && api->TexParameteri &&
           api->TexImage2D && api->DeleteTextures && api->Viewport && api->Enable &&
           api->Disable && api->BlendEquation && api->BlendFunc && api->ColorMask &&
           api->DrawArrays && api->Scissor;
}

static bool driver_ok(const AbGles2Api *api, AbError *error) {
    uint32_t code = api->GetError();
    return code == 0 || ab_fail(error, "GLES2 driver error 0x%04x", code);
}

static uint32_t compile_shader(AbGles2 *renderer, uint32_t stage, const char *source,
                               AbError *error) {
    const AbGles2Api *api = &renderer->api;
    uint32_t shader = api->CreateShader(stage);
    if (!shader) {
        ab_fail(error, "Cannot create GLES2 shader");
        return 0;
    }
    const char *parts[] = {
        "#version 100\n",
        renderer->high_precision_uv ? "#define AB_HIGH_PRECISION_UV 1\n" : "",
        strchr(source, '\n') + 1,
    };
    api->ShaderSource(shader, 3, parts, NULL);
    api->CompileShader(shader);
    int32_t compiled = 0;
    api->GetShaderiv(shader, GL_COMPILE_RESULT, &compiled);
    if (compiled) {
        return shader;
    }
    char log[192] = {0};
    api->GetShaderInfoLog(shader, (int32_t)sizeof(log), NULL, log);
    log[sizeof(log) - 1] = '\0';
    ab_fail(error, "GLES2 shader compilation failed: %s", log);
    api->DeleteShader(shader);
    return 0;
}

static bool build_program(AbGles2 *renderer, AbError *error) {
    const AbGles2Api *api = &renderer->api;
    uint32_t vertex =
        compile_shader(renderer, GL_VERTEX_STAGE, ab_sprite_vertex_source, error);
    if (!vertex) {
        return false;
    }
    uint32_t fragment =
        compile_shader(renderer, GL_FRAGMENT_STAGE, ab_sprite_fragment_source, error);
    if (!fragment) {
        api->DeleteShader(vertex);
        return false;
    }
    renderer->program = api->CreateProgram();
    if (!renderer->program) {
        api->DeleteShader(vertex);
        api->DeleteShader(fragment);
        return ab_fail(error, "Cannot create GLES2 program");
    }
    api->AttachShader(renderer->program, vertex);
    api->AttachShader(renderer->program, fragment);
    api->BindAttribLocation(renderer->program, 0, "vertex_position");
    api->BindAttribLocation(renderer->program, 1, "vertex_uv");
    api->BindAttribLocation(renderer->program, 2, "vertex_mask_uv");
    api->LinkProgram(renderer->program);
    api->DeleteShader(vertex);
    api->DeleteShader(fragment);
    int32_t linked = 0;
    api->GetProgramiv(renderer->program, GL_LINK_RESULT, &linked);
    if (!linked) {
        char log[192] = {0};
        api->GetProgramInfoLog(renderer->program, (int32_t)sizeof(log), NULL, log);
        log[sizeof(log) - 1] = '\0';
        return ab_fail(error, "GLES2 shader link failed: %s", log);
    }
    renderer->size_uniform =
        api->GetUniformLocation(renderer->program, "framebuffer_size");
    renderer->texture_uniform =
        api->GetUniformLocation(renderer->program, "sprite_texture");
    renderer->color_uniform =
        api->GetUniformLocation(renderer->program, "channel_multiplier");
    renderer->opacity_uniform = api->GetUniformLocation(renderer->program, "opacity");
    renderer->coordinates_uniform =
        api->GetUniformLocation(renderer->program, "coordinates_are_clip");
    renderer->mask_texture_uniform =
        api->GetUniformLocation(renderer->program, "mask_texture");
    renderer->mask_enabled_uniform =
        api->GetUniformLocation(renderer->program, "mask_enabled");
    return (renderer->size_uniform >= 0 && renderer->texture_uniform >= 0 &&
            renderer->color_uniform >= 0 && renderer->opacity_uniform >= 0 &&
            renderer->coordinates_uniform >= 0 && renderer->mask_texture_uniform >= 0 &&
            renderer->mask_enabled_uniform >= 0) ||
           ab_fail(error, "GLES2 sprite program is missing required uniforms");
}

bool ab_gles2_create(const AbGles2Api *api, size_t texture_capacity,
                     size_t allocation_limit, AbGles2 **output, AbError *error) {
    if (!output) {
        return ab_fail(error, "Missing GLES2 output");
    }
    *output = NULL;
    if (!complete_api(api) || texture_capacity > UINT32_MAX ||
        allocation_limit < sizeof(AbGles2) ||
        texture_capacity > (allocation_limit - sizeof(AbGles2)) / sizeof(uint32_t)) {
        return ab_fail(error, "Incomplete GLES2 API or allocation exceeds limit");
    }
    AbGles2 *renderer = calloc(1, sizeof(*renderer));
    if (!renderer) {
        return ab_fail(error, "Cannot allocate GLES2 backend");
    }
    renderer->api = *api;
    if (texture_capacity) {
        renderer->textures = calloc(texture_capacity, sizeof(*renderer->textures));
        if (!renderer->textures) {
            ab_fail(error, "Cannot allocate GLES2 texture slots");
            goto release_renderer;
        }
    }
    renderer->texture_capacity = texture_capacity;
    int32_t range[2] = {0};
    int32_t precision = 0;
    api->GetShaderPrecisionFormat(GL_FRAGMENT_STAGE, GL_HIGH_FLOAT_FORMAT, range,
                                  &precision);
    renderer->high_precision_uv = precision > 0;
    api->GetIntegerv(GL_TEXTURE_LIMIT, &renderer->maximum_texture_size);
    if (renderer->maximum_texture_size <= 0 || !build_program(renderer, error)) {
        if (renderer->maximum_texture_size <= 0) {
            ab_fail(error, "Invalid GLES2 maximum texture size");
        }
        goto release_renderer;
    }
    api->GenBuffers(1, &renderer->buffer);
    if (!renderer->buffer) {
        ab_fail(error, "Cannot create GLES2 vertex buffer");
        goto release_renderer;
    }
    api->BindBuffer(GL_VERTEX_BUFFER, renderer->buffer);
    api->BufferData(GL_VERTEX_BUFFER, (intptr_t)(sizeof(AbRenderVertex) * 4), NULL,
                    GL_STREAM_USAGE);
    if (!driver_ok(api, error)) {
        goto release_renderer;
    }
    *output = renderer;
    return true;

release_renderer:
    ab_gles2_free(renderer);
    return false;
}

static bool valid_texture(const AbGles2 *renderer, uint32_t texture) {
    return renderer && texture && texture <= renderer->texture_capacity;
}

bool ab_gles2_upload(AbGles2 *renderer, uint32_t texture, const AbImage *image,
                     bool repeat_s, bool repeat_t, AbError *error) {
    if (!valid_texture(renderer, texture) || renderer->textures[texture - 1] ||
        !image || !image->width || !image->height || !image->rgba.data ||
        image->width > (uint32_t)renderer->maximum_texture_size ||
        image->height > (uint32_t)renderer->maximum_texture_size ||
        image->width > SIZE_MAX / 4 / image->height ||
        image->rgba.size != (size_t)image->width * image->height * 4) {
        return ab_fail(error, "Invalid GLES2 texture upload or slot is occupied");
    }
    bool power_of_two = ((image->width - 1) & image->width) == 0 &&
                        ((image->height - 1) & image->height) == 0;
    if (!power_of_two && (repeat_s || repeat_t)) {
        return ab_fail(error, "Core GLES2 requires clamp sampling for NPOT textures");
    }
    const AbGles2Api *api = &renderer->api;
    uint32_t name = 0;
    api->GenTextures(1, &name);
    if (!name) {
        return ab_fail(error, "Cannot create GLES2 texture");
    }
    api->ActiveTexture(GL_TEXTURE_UNIT_ZERO);
    api->BindTexture(GL_TEXTURE_2D_TARGET, name);
    api->PixelStorei(GL_UNPACK_ALIGNMENT_BYTES, 1);
    api->TexParameteri(GL_TEXTURE_2D_TARGET, GL_MIN_FILTER, GL_LINEAR_FILTER);
    api->TexParameteri(GL_TEXTURE_2D_TARGET, GL_MAG_FILTER, GL_LINEAR_FILTER);
    api->TexParameteri(GL_TEXTURE_2D_TARGET, GL_WRAP_S,
                       repeat_s ? GL_REPEAT_WRAP : GL_CLAMP_WRAP);
    api->TexParameteri(GL_TEXTURE_2D_TARGET, GL_WRAP_T,
                       repeat_t ? GL_REPEAT_WRAP : GL_CLAMP_WRAP);
    api->TexImage2D(GL_TEXTURE_2D_TARGET, 0, GL_RGBA_CHANNELS, (int32_t)image->width,
                    (int32_t)image->height, 0, GL_RGBA_CHANNELS, GL_BYTE_DATA,
                    image->rgba.data);
    if (!driver_ok(api, error)) {
        api->DeleteTextures(1, &name);
        return false;
    }
    renderer->textures[texture - 1] = name;
    return true;
}

bool ab_gles2_remove_texture(AbGles2 *renderer, uint32_t texture, AbError *error) {
    if (!valid_texture(renderer, texture) || !renderer->textures[texture - 1]) {
        return ab_fail(error, "Missing GLES2 texture");
    }
    renderer->api.DeleteTextures(1, renderer->textures + texture - 1);
    renderer->textures[texture - 1] = 0;
    return driver_ok(&renderer->api, error);
}

static void set_blend(const AbGles2Api *api, unsigned blend) {
    if (blend == AB_BLEND_OPAQUE) {
        api->Disable(GL_BLEND_STATE);
        return;
    }
    api->Enable(GL_BLEND_STATE);
    api->BlendEquation(GL_ADD_EQUATION);
    uint32_t source = blend == AB_BLEND_STRAIGHT   ? GL_SOURCE_ALPHA_FACTOR
                      : blend == AB_BLEND_MULTIPLY ? GL_DESTINATION_COLOR_FACTOR
                                                   : 1;
    uint32_t destination = blend == AB_BLEND_ADDITIVE   ? 1
                           : blend == AB_BLEND_MULTIPLY ? 0
                                                        : GL_INVERSE_ALPHA_FACTOR;
    api->BlendFunc(source, destination);
}

static bool valid_command(const AbGles2 *renderer, const AbDrawCommand *command) {
    if (!valid_texture(renderer, command->texture) ||
        !renderer->textures[command->texture - 1] ||
        (command->mask.texture && (!valid_texture(renderer, command->mask.texture) ||
                                   !renderer->textures[command->mask.texture - 1])) ||
        !ab_draw_command_check(command, NULL)) {
        return false;
    }
    return true;
}

bool ab_gles2_render(AbGles2 *renderer, const AbDrawList *list, uint32_t width,
                     uint32_t height, AbError *error) {
    if (!renderer || !list || list->count > list->capacity ||
        (list->count && !list->commands) || !width || !height || width > INT32_MAX ||
        height > INT32_MAX) {
        return ab_fail(error, "Invalid GLES2 draw list or framebuffer size");
    }
    for (size_t index = 0; index < list->count; ++index) {
        if (!valid_command(renderer, list->commands + index)) {
            return ab_fail(error, "Invalid GLES2 draw command at index %zu", index);
        }
    }
    const AbGles2Api *api = &renderer->api;
    api->Viewport(0, 0, (int32_t)width, (int32_t)height);
    api->Disable(GL_DEPTH_STATE);
    api->Disable(GL_CULL_STATE);
    api->Disable(GL_STENCIL_STATE);
    api->Disable(GL_SCISSOR_STATE);
    api->ColorMask(1, 1, 1, 1);
    api->UseProgram(renderer->program);
    api->Uniform2f(renderer->size_uniform, (float)width, (float)height);
    api->Uniform1i(renderer->texture_uniform, 0);
    api->Uniform1i(renderer->mask_texture_uniform, 1);
    api->ActiveTexture(GL_TEXTURE_UNIT_ZERO);
    api->BindBuffer(GL_VERTEX_BUFFER, renderer->buffer);
    api->EnableVertexAttribArray(0);
    api->EnableVertexAttribArray(1);
    api->EnableVertexAttribArray(2);
    api->VertexAttribPointer(0, 2, GL_FLOAT_DATA, 0, (int32_t)sizeof(AbRenderVertex),
                             NULL);
    api->VertexAttribPointer(1, 2, GL_FLOAT_DATA, 0, (int32_t)sizeof(AbRenderVertex),
                             (const void *)(uintptr_t)offsetof(AbDrawVertex, u));
    api->VertexAttribPointer(
        2, 2, GL_FLOAT_DATA, 0, (int32_t)sizeof(AbRenderVertex),
        (const void *)(uintptr_t)offsetof(AbRenderVertex, mask_uv));
    for (size_t index = 0; index < list->count; ++index) {
        const AbDrawCommand *command = list->commands + index;
        AbDrawScissor clip;
        if (!ab_draw_clip_scissor(command->scissor, width, height, &clip, error))
            return false;
        if (!clip.width || !clip.height)
            continue;
        if (command->scissor.enabled) {
            api->Enable(GL_SCISSOR_STATE);
            api->Scissor((int32_t)clip.x, (int32_t)(height - clip.y - clip.height),
                         (int32_t)clip.width, (int32_t)clip.height);
        } else {
            api->Disable(GL_SCISSOR_STATE);
        }
        api->ActiveTexture(GL_TEXTURE_UNIT_ZERO + 1);
        api->BindTexture(
            GL_TEXTURE_2D_TARGET,
            renderer->textures[(command->mask.texture ? command->mask.texture
                                                      : command->texture) -
                               1]);
        api->ActiveTexture(GL_TEXTURE_UNIT_ZERO);
        api->BindTexture(GL_TEXTURE_2D_TARGET,
                         renderer->textures[command->texture - 1]);
        set_blend(api, (unsigned)command->blend);
        api->Uniform4fv(renderer->color_uniform, 1, command->color);
        api->Uniform1f(renderer->opacity_uniform, command->opacity);
        api->Uniform1f(renderer->coordinates_uniform,
                       command->coordinates == AB_DRAW_CLIP ? 1.0f : 0.0f);
        api->Uniform1f(renderer->mask_enabled_uniform,
                       command->mask.texture ? 1.0f : 0.0f);
        AbRenderVertex vertices[4];
        ab_render_vertices(command, vertices);
        api->BufferSubData(GL_VERTEX_BUFFER, 0, (intptr_t)sizeof(vertices), vertices);
        api->DrawArrays(GL_TRIANGLE_STRIP_MODE, 0, 4);
    }
    return driver_ok(api, error);
}

bool ab_gles2_high_precision_uv(const AbGles2 *renderer) {
    return renderer && renderer->high_precision_uv;
}

void ab_gles2_free(AbGles2 *renderer) {
    if (renderer) {
        const AbGles2Api *api = &renderer->api;
        if (renderer->textures) {
            for (size_t index = 0; index < renderer->texture_capacity; ++index) {
                if (renderer->textures[index]) {
                    api->DeleteTextures(1, renderer->textures + index);
                }
            }
        }
        if (renderer->buffer) {
            api->DeleteBuffers(1, &renderer->buffer);
        }
        if (renderer->program) {
            api->DeleteProgram(renderer->program);
        }
        free(renderer->textures);
        free(renderer);
    }
}
