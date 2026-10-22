/* Render our cached scene with our GLES2 backend on the isolated Android driver.
 * This program does not load original game code. Its decoded assets and command
 * input are private research data exported by ab_theme_gpu_data. */
#include "ab/gles2.h"
#include "ab/hash.h"
#include "../gpu_scene_data.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

extern void *dlopen(const char *path, int flags);
extern void *dlsym(void *library, const char *name);
extern int dlclose(void *library);
extern char **environ;
_Noreturn void _exit(int status);

typedef struct {
    void *(*GetDisplay)(void *native_display);
    uint32_t (*Initialize)(void *display, int32_t *major, int32_t *minor);
    uint32_t (*ChooseConfig)(void *display, const int32_t *attributes, void **configs,
                             int32_t capacity, int32_t *count);
    void *(*CreatePbufferSurface)(void *display, void *config,
                                  const int32_t *attributes);
    void *(*CreateContext)(void *display, void *config, void *shared,
                           const int32_t *attributes);
    uint32_t (*MakeCurrent)(void *display, void *draw, void *read, void *context);
    uint32_t (*DestroyContext)(void *display, void *context);
    uint32_t (*DestroySurface)(void *display, void *surface);
    uint32_t (*Terminate)(void *display);
} EglApi;

typedef struct {
    void (*ClearColor)(float red, float green, float blue, float alpha);
    void (*Clear)(uint32_t mask);
    void (*ReadPixels)(int32_t x, int32_t y, int32_t width, int32_t height,
                       uint32_t format, uint32_t type, void *pixels);
} CaptureApi;

static void write_message(const char *message, size_t size, int file) {
    while (size) {
        register int result __asm__("r0") = file;
        register const char *bytes __asm__("r1") = message;
        register size_t length __asm__("r2") = size;
        register int operation __asm__("r7") = 4;
        __asm__ volatile("svc #0"
                         : "+r"(result)
                         : "r"(bytes), "r"(length), "r"(operation)
                         : "memory", "cc");
        if (result <= 0 || (size_t)result > size) {
            return;
        }
        message += result;
        size -= (size_t)result;
    }
}

static bool bind_symbol(void *library, const char *name, void *slot, size_t size,
                        AbError *error) {
    void *pointer = dlsym(library, name);
    if (!pointer || size != sizeof(pointer)) {
        return ab_fail(error, "Missing platform entry point: %s", name);
    }
    memcpy(slot, &pointer, size);
    return true;
}

#define BIND(library, api, member, symbol)                                             \
    do {                                                                               \
        if (!bind_symbol(library, symbol, &(api)->member, sizeof((api)->member),       \
                         error)) {                                                     \
            return false;                                                              \
        }                                                                              \
    } while (0)

static bool load_egl(void *library, EglApi *api, AbError *error) {
    BIND(library, api, GetDisplay, "eglGetDisplay");
    BIND(library, api, Initialize, "eglInitialize");
    BIND(library, api, ChooseConfig, "eglChooseConfig");
    BIND(library, api, CreatePbufferSurface, "eglCreatePbufferSurface");
    BIND(library, api, CreateContext, "eglCreateContext");
    BIND(library, api, MakeCurrent, "eglMakeCurrent");
    BIND(library, api, DestroyContext, "eglDestroyContext");
    BIND(library, api, DestroySurface, "eglDestroySurface");
    BIND(library, api, Terminate, "eglTerminate");
    return true;
}

static bool load_gles(void *library, AbGles2Api *api, CaptureApi *capture,
                      AbError *error) {
#define GLES(member) BIND(library, api, member, "gl" #member)
    GLES(CreateShader);
    GLES(ShaderSource);
    GLES(CompileShader);
    GLES(GetShaderiv);
    GLES(GetShaderInfoLog);
    GLES(DeleteShader);
    GLES(CreateProgram);
    GLES(AttachShader);
    GLES(BindAttribLocation);
    GLES(LinkProgram);
    GLES(GetProgramiv);
    GLES(GetProgramInfoLog);
    GLES(DeleteProgram);
    GLES(GetUniformLocation);
    GLES(UseProgram);
    GLES(Uniform1i);
    GLES(Uniform1f);
    GLES(Uniform2f);
    GLES(Uniform4fv);
    GLES(GetShaderPrecisionFormat);
    GLES(GetIntegerv);
    GLES(GetError);
    GLES(GenBuffers);
    GLES(BindBuffer);
    GLES(BufferData);
    GLES(BufferSubData);
    GLES(DeleteBuffers);
    GLES(EnableVertexAttribArray);
    GLES(VertexAttribPointer);
    GLES(GenTextures);
    GLES(ActiveTexture);
    GLES(BindTexture);
    GLES(PixelStorei);
    GLES(TexParameteri);
    GLES(TexImage2D);
    GLES(DeleteTextures);
    GLES(Viewport);
    GLES(Enable);
    GLES(Disable);
    GLES(BlendEquation);
    GLES(BlendFunc);
    GLES(ColorMask);
    GLES(DrawArrays);
    GLES(Scissor);
#undef GLES
    BIND(library, capture, ClearColor, "glClearColor");
    BIND(library, capture, Clear, "glClear");
    BIND(library, capture, ReadPixels, "glReadPixels");
    return true;
}

#undef BIND

static void write_word(uint8_t *bytes, uint32_t word) {
    for (size_t index = 0; index < 4; ++index) {
        bytes[index] = (uint8_t)(word >> (unsigned)(index * 8));
    }
}

static bool capture_scene(const AbGpuSceneData *scene, const AbGles2Api *api,
                          const CaptureApi *capture, AbBuffer *output, AbError *error) {
    static const char magic[] = "AB_GL_CAPTURE_V1\n";
    int32_t header[32];
    for (size_t index = 0; index < 32; ++index) {
        header[index] = -1;
    }
    header[0] = (int32_t)scene->width;
    header[1] = (int32_t)scene->height;
    api->GetIntegerv(0x8ca6, header + 2);
    for (uint32_t index = 0; index < 6; ++index) {
        api->GetIntegerv(0x0d52 + index, header + 3 + index);
    }
    api->GetIntegerv(0x80a9, header + 9);
    api->GetIntegerv(0x0bd0, header + 10);
    api->GetIntegerv(0x0ba2, header + 16);
    api->GetIntegerv(0x8b9b, header + 25);
    api->GetIntegerv(0x8b9a, header + 26);
    size_t size = (size_t)scene->width * scene->height * 4;
    uint8_t *pixels = malloc(size);
    output->size = sizeof(magic) - 1 + sizeof(header) + size;
    output->data = malloc(output->size);
    if (!pixels || !output->data) {
        free(pixels);
        ab_buffer_free(output);
        return ab_fail(error, "Cannot allocate GPU capture");
    }
    capture->ReadPixels(0, 0, (int32_t)scene->width, (int32_t)scene->height, 0x1908,
                        0x1401, pixels);
    bool okay = api->GetError() == 0;
    memcpy(output->data, magic, sizeof(magic) - 1);
    for (size_t index = 0; index < 32; ++index) {
        write_word(output->data + sizeof(magic) - 1 + index * 4,
                   (uint32_t)header[index]);
    }
    memcpy(output->data + sizeof(magic) - 1 + sizeof(header), pixels, size);
    free(pixels);
    return okay || ab_fail(error, "GPU capture driver error");
}

void theme_gpu_main(const uintptr_t *initial_stack) {
    if (initial_stack[0] < 1 || initial_stack[0] > 16) {
        _exit(2);
    }
    environ = (char **)(initial_stack + initial_stack[0] + 2);
    AbError error = {0};
    AbBuffer input = {0}, output = {0};
    AbGpuSceneData scene = {0};
    EglApi egl = {0};
    AbGles2Api api = {0};
    CaptureApi capture = {0};
    AbGles2 *renderer = NULL;
    void *display = NULL, *surface = NULL, *context = NULL;
    bool initialized = false, okay = false;
    void *egl_library = dlopen("/system/lib/libEGL.so", 2);
    void *gles_library = dlopen("/system/lib/libGLESv2.so", 2);
    if (!egl_library || !gles_library || !load_egl(egl_library, &egl, &error) ||
        !load_gles(gles_library, &api, &capture, &error) ||
        !ab_file_read("/data/local/tmp/ab520/theme-gpu-input.bin", 32 * 1024 * 1024,
                      &input, &error) ||
        !ab_gpu_scene_data_read((AbSpan){input.data, input.size}, &scene, &error)) {
        goto release_capture;
    }
    display = egl.GetDisplay(NULL);
    int32_t major, minor;
    initialized = display && egl.Initialize(display, &major, &minor);
    if (!initialized) {
        ab_fail(&error, "Cannot initialize isolated EGL display");
        goto release_capture;
    }
    const int32_t attributes[] = {0x3033, 1,      0x3040, 4,      0x3024, 8,     0x3023,
                                  8,      0x3022, 8,      0x3021, 8,      0x3038};
    int32_t count = 0;
    void *config = NULL;
    if (!egl.ChooseConfig(display, attributes, &config, 1, &count) || count != 1) {
        ab_fail(&error, "No isolated GLES2 pbuffer configuration");
        goto release_capture;
    }
    const int32_t extent[] = {0x3057, (int32_t)scene.width, 0x3056,
                              (int32_t)scene.height, 0x3038};
    const int32_t version[] = {0x3098, 2, 0x3038};
    surface = egl.CreatePbufferSurface(display, config, extent);
    context = egl.CreateContext(display, config, NULL, version);
    if (!surface || !context || !egl.MakeCurrent(display, surface, surface, context) ||
        !ab_gles2_create(&api, scene.texture_count, 1024 * 1024, &renderer, &error)) {
        if (!error.message[0]) {
            ab_fail(&error, "Cannot create isolated GLES2 context");
        }
        goto release_capture;
    }
    for (uint32_t index = 0; index < scene.texture_count; ++index) {
        if (!ab_gles2_upload(renderer, index + 1, scene.images + index, false, false,
                             &error)) {
            goto release_capture;
        }
    }
    capture.ClearColor(scene.clear[0], scene.clear[1], scene.clear[2], scene.clear[3]);
    capture.Clear(0x4000);
    okay = ab_gles2_render(renderer, &scene.list, scene.width, scene.height, &error) &&
           capture_scene(&scene, &api, &capture, &output, &error) &&
           ab_file_write("/data/local/tmp/ab520/theme-gpu-output.bin",
                         (AbSpan){output.data, output.size}, &error);

release_capture:
    ab_gles2_free(renderer);
    if (initialized) {
        egl.MakeCurrent(display, NULL, NULL, NULL);
        if (context) {
            egl.DestroyContext(display, context);
        }
        if (surface) {
            egl.DestroySurface(display, surface);
        }
        egl.Terminate(display);
    }
    if (gles_library) {
        dlclose(gles_library);
    }
    if (egl_library) {
        dlclose(egl_library);
    }
    if (okay) {
        uint8_t digest[32];
        char hexadecimal[65];
        okay = ab_sha256((AbSpan){input.data, input.size}, digest, &error);
        if (okay) {
            ab_digest_hex(digest, hexadecimal);
            static const char complete[] = "AB_THEME_GPU_COMPLETE ";
            write_message(complete, sizeof(complete) - 1, 1);
            write_message(hexadecimal, 64, 1);
            write_message("\n", 1, 1);
        }
    }
    if (!okay) {
        write_message(error.message, strlen(error.message), 2);
        write_message("\n", 1, 2);
    }
    ab_buffer_free(&output);
    ab_buffer_free(&input);
    _exit(okay ? 0 : 1);
}
