#ifndef AB_GLES2_H
#define AB_GLES2_H

#include "ab/draw.h"
#include "ab/image.h"

/* Core GLES2 ABI only. The native platform supplies entry points and a context. */
typedef struct {
    uint32_t (*CreateShader)(uint32_t type);
    void (*ShaderSource)(uint32_t shader, int32_t count, const char *const *strings,
                         const int32_t *lengths);
    void (*CompileShader)(uint32_t shader);
    void (*GetShaderiv)(uint32_t shader, uint32_t name, int32_t *value);
    void (*GetShaderInfoLog)(uint32_t shader, int32_t capacity, int32_t *length,
                             char *log);
    void (*DeleteShader)(uint32_t shader);
    uint32_t (*CreateProgram)(void);
    void (*AttachShader)(uint32_t program, uint32_t shader);
    void (*BindAttribLocation)(uint32_t program, uint32_t location, const char *name);
    void (*LinkProgram)(uint32_t program);
    void (*GetProgramiv)(uint32_t program, uint32_t name, int32_t *value);
    void (*GetProgramInfoLog)(uint32_t program, int32_t capacity, int32_t *length,
                              char *log);
    void (*DeleteProgram)(uint32_t program);
    int32_t (*GetUniformLocation)(uint32_t program, const char *name);
    void (*UseProgram)(uint32_t program);
    void (*Uniform1i)(int32_t location, int32_t value);
    void (*Uniform1f)(int32_t location, float value);
    void (*Uniform2f)(int32_t location, float x, float y);
    void (*Uniform4fv)(int32_t location, int32_t count, const float *values);
    void (*GetShaderPrecisionFormat)(uint32_t stage, uint32_t type, int32_t *range,
                                     int32_t *precision);
    void (*GetIntegerv)(uint32_t name, int32_t *value);
    uint32_t (*GetError)(void);
    void (*GenBuffers)(int32_t count, uint32_t *buffers);
    void (*BindBuffer)(uint32_t target, uint32_t buffer);
    void (*BufferData)(uint32_t target, intptr_t size, const void *data,
                       uint32_t usage);
    void (*BufferSubData)(uint32_t target, intptr_t offset, intptr_t size,
                          const void *data);
    void (*DeleteBuffers)(int32_t count, const uint32_t *buffers);
    void (*EnableVertexAttribArray)(uint32_t location);
    void (*VertexAttribPointer)(uint32_t location, int32_t count, uint32_t type,
                                uint8_t normalized, int32_t stride, const void *offset);
    void (*GenTextures)(int32_t count, uint32_t *textures);
    void (*ActiveTexture)(uint32_t unit);
    void (*BindTexture)(uint32_t target, uint32_t texture);
    void (*PixelStorei)(uint32_t name, int32_t value);
    void (*TexParameteri)(uint32_t target, uint32_t name, int32_t value);
    void (*TexImage2D)(uint32_t target, int32_t level, int32_t internal_format,
                       int32_t width, int32_t height, int32_t border, uint32_t format,
                       uint32_t type, const void *pixels);
    void (*DeleteTextures)(int32_t count, const uint32_t *textures);
    void (*Viewport)(int32_t x, int32_t y, int32_t width, int32_t height);
    void (*Enable)(uint32_t state);
    void (*Disable)(uint32_t state);
    void (*BlendEquation)(uint32_t equation);
    void (*BlendFunc)(uint32_t source, uint32_t destination);
    void (*ColorMask)(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha);
    void (*DrawArrays)(uint32_t mode, int32_t first, int32_t count);
    void (*Scissor)(int32_t x, int32_t y, int32_t width, int32_t height);
} AbGles2Api;

typedef struct AbGles2 AbGles2;

/* All calls, including free, require the same current native GLES2 context.
 * The backend owns its pass state. It does not create, clear, or present a window.
 * Texture IDs belong to the shared draw contract, in 1..texture_capacity.
 */
bool ab_gles2_create(const AbGles2Api *api, size_t texture_capacity,
                     size_t allocation_limit, AbGles2 **output, AbError *error);
bool ab_gles2_upload(AbGles2 *renderer, uint32_t texture, const AbImage *image,
                     bool repeat_s, bool repeat_t, AbError *error);
bool ab_gles2_remove_texture(AbGles2 *renderer, uint32_t texture, AbError *error);
bool ab_gles2_render(AbGles2 *renderer, const AbDrawList *list, uint32_t width,
                     uint32_t height, AbError *error);
bool ab_gles2_high_precision_uv(const AbGles2 *renderer);
void ab_gles2_free(AbGles2 *renderer);

#endif
