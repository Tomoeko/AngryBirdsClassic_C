#include "ab/gles2.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expression)                                                              \
    do {                                                                               \
        if (!(expression)) {                                                           \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expression);           \
            return false;                                                              \
        }                                                                              \
    } while (0)

/* A command/state recorder, not a rasterizer or a native fidelity oracle. */
typedef enum {
    FAIL_NONE,
    FAIL_VERTEX,
    FAIL_FRAGMENT,
    FAIL_PROGRAM,
    FAIL_LINK,
    FAIL_UNIFORM,
    FAIL_BUFFER,
    FAIL_UPLOAD,
    FAIL_DRAW,
} Failure;

typedef struct {
    Failure failure;
    bool high_precision;
    bool invalid_call;
    bool live[64];
    unsigned kind[64];
    uint32_t stage[64];
    uint32_t next_name;
    uint32_t error;
    uint32_t bound_texture;
    uint32_t bound_buffer;
    uint32_t bound_program;
    uint32_t source_factor;
    uint32_t destination_factor;
    uint32_t drawn_textures[8];
    uint32_t drawn_sources[8];
    uint32_t drawn_destinations[8];
    bool scissoring, drew_scissoring[8];
    int32_t scissor[4], drawn_scissors[8][4];
    bool blending;
    bool drew_blending[8];
    bool coordinates_are_clip, drawn_clip[8];
    size_t draw_count;
    unsigned deleted[5];
    unsigned source_count;
    unsigned high_precision_sources;
    int32_t wrap_s;
    int32_t wrap_t;
    const void *uploaded_pixels;
    AbDrawQuad buffered_quad;
} Recorder;

static Recorder recorder;

static uint32_t create_object(unsigned kind) {
    uint32_t name = ++recorder.next_name;
    if (name >= 64) {
        recorder.invalid_call = true;
        return 0;
    }
    recorder.live[name] = true;
    recorder.kind[name] = kind;
    return name;
}

static bool object_exists(uint32_t name, unsigned kind) {
    return name < 64 && recorder.live[name] && recorder.kind[name] == kind;
}

static void delete_object(uint32_t name, unsigned kind) {
    if (!object_exists(name, kind)) {
        recorder.invalid_call = true;
        return;
    }
    recorder.live[name] = false;
    ++recorder.deleted[kind];
}

static uint32_t create_shader(uint32_t stage) {
    uint32_t name = create_object(1);
    recorder.stage[name] = stage;
    return name;
}

static void shader_source(uint32_t shader, int32_t count, const char *const *strings,
                          const int32_t *lengths) {
    if (!object_exists(shader, 1) || count != 3 || lengths ||
        strcmp(strings[0], "#version 100\n") != 0 || strstr(strings[2], "#version")) {
        recorder.invalid_call = true;
    }
    ++recorder.source_count;
    if (strstr(strings[1], "AB_HIGH_PRECISION_UV")) {
        ++recorder.high_precision_sources;
    }
}

static void compile_shader(uint32_t shader) {
    if (!object_exists(shader, 1)) {
        recorder.invalid_call = true;
    }
}

static void shader_status(uint32_t shader, uint32_t name, int32_t *value) {
    if (!object_exists(shader, 1) || name != 0x8b81) {
        recorder.invalid_call = true;
    }
    *value = !((recorder.failure == FAIL_VERTEX && recorder.stage[shader] == 0x8b31) ||
               (recorder.failure == FAIL_FRAGMENT && recorder.stage[shader] == 0x8b30));
}

static void info_log(uint32_t object, int32_t capacity, int32_t *length, char *log) {
    (void)object;
    const char message[] = "injected failure";
    if (capacity < (int32_t)sizeof(message)) {
        recorder.invalid_call = true;
        return;
    }
    memcpy(log, message, sizeof(message));
    if (length) {
        *length = (int32_t)sizeof(message) - 1;
    }
}

static void delete_shader(uint32_t shader) {
    delete_object(shader, 1);
}

static uint32_t create_program(void) {
    return recorder.failure == FAIL_PROGRAM ? 0 : create_object(2);
}

static void attach_shader(uint32_t program, uint32_t shader) {
    if (!object_exists(program, 2) || !object_exists(shader, 1)) {
        recorder.invalid_call = true;
    }
}

static void attribute_location(uint32_t program, uint32_t location, const char *name) {
    if (!object_exists(program, 2) || location > 1 ||
        strcmp(name, location ? "vertex_uv" : "vertex_position") != 0) {
        recorder.invalid_call = true;
    }
}

static void link_program(uint32_t program) {
    if (!object_exists(program, 2)) {
        recorder.invalid_call = true;
    }
}

static void program_status(uint32_t program, uint32_t name, int32_t *value) {
    if (!object_exists(program, 2) || name != 0x8b82) {
        recorder.invalid_call = true;
    }
    *value = recorder.failure != FAIL_LINK;
}

static void delete_program(uint32_t program) {
    delete_object(program, 2);
}

static int32_t uniform_location(uint32_t program, const char *name) {
    if (!object_exists(program, 2)) {
        recorder.invalid_call = true;
    }
    const char *names[] = {"framebuffer_size", "sprite_texture", "channel_multiplier",
                           "opacity", "coordinates_are_clip"};
    for (int32_t index = 0; index < 5; ++index) {
        if (strcmp(name, names[index]) == 0) {
            return recorder.failure == FAIL_UNIFORM ? -1 : index;
        }
    }
    recorder.invalid_call = true;
    return -1;
}

static void use_program(uint32_t program) {
    recorder.bound_program = program;
}

static void uniform_int(int32_t location, int32_t value) {
    if (location != 1 || value != 0) {
        recorder.invalid_call = true;
    }
}

static void uniform_float(int32_t location, float value) {
    if (location == 4 && (value == 0 || value == 1)) {
        recorder.coordinates_are_clip = value != 0;
        return;
    }
    if (location != 3 || value < 0 || value > 1) {
        recorder.invalid_call = true;
    }
}

static void uniform_pair(int32_t location, float x, float y) {
    if (location != 0 || x != 640 || y != 480) {
        recorder.invalid_call = true;
    }
}

static void uniform_color(int32_t location, int32_t count, const float *value) {
    if (location != 2 || count != 1 || value[0] != 1) {
        recorder.invalid_call = true;
    }
}

static void shader_precision(uint32_t stage, uint32_t type, int32_t *range,
                             int32_t *precision) {
    if (stage != 0x8b30 || type != 0x8df2) {
        recorder.invalid_call = true;
    }
    range[0] = recorder.high_precision ? 127 : 0;
    range[1] = range[0];
    *precision = recorder.high_precision ? 23 : 0;
}

static void get_integer(uint32_t name, int32_t *value) {
    if (name != 0x0d33) {
        recorder.invalid_call = true;
    }
    *value = 2048;
}

static uint32_t get_error(void) {
    uint32_t code = recorder.error;
    recorder.error = 0;
    return code;
}

static void generate_buffer(int32_t count, uint32_t *buffers) {
    if (count != 1) {
        recorder.invalid_call = true;
    }
    *buffers = recorder.failure == FAIL_BUFFER ? 0 : create_object(3);
}

static void bind_buffer(uint32_t target, uint32_t buffer) {
    if (target != 0x8892 || !object_exists(buffer, 3)) {
        recorder.invalid_call = true;
    }
    recorder.bound_buffer = buffer;
}

static void buffer_data(uint32_t target, intptr_t size, const void *data,
                        uint32_t usage) {
    if (target != 0x8892 || size != (intptr_t)sizeof(AbDrawQuad) || data ||
        usage != 0x88e0) {
        recorder.invalid_call = true;
    }
}

static void buffer_update(uint32_t target, intptr_t offset, intptr_t size,
                          const void *data) {
    if (target != 0x8892 || offset || size != (intptr_t)sizeof(AbDrawQuad)) {
        recorder.invalid_call = true;
        return;
    }
    memcpy(&recorder.buffered_quad, data, sizeof(AbDrawQuad));
}

static void delete_buffer(int32_t count, const uint32_t *buffers) {
    if (count != 1) {
        recorder.invalid_call = true;
    }
    delete_object(*buffers, 3);
}

static void enable_attribute(uint32_t location) {
    if (location > 1) {
        recorder.invalid_call = true;
    }
}

static void attribute_pointer(uint32_t location, int32_t count, uint32_t type,
                              uint8_t normalized, int32_t stride, const void *offset) {
    if (location > 1 || count != 2 || type != 0x1406 || normalized ||
        stride != (int32_t)sizeof(AbDrawVertex) ||
        (uintptr_t)offset != (location ? offsetof(AbDrawVertex, u) : 0)) {
        recorder.invalid_call = true;
    }
}

static void generate_texture(int32_t count, uint32_t *textures) {
    if (count != 1) {
        recorder.invalid_call = true;
    }
    *textures = create_object(4);
}

static void activate_texture(uint32_t unit) {
    if (unit != 0x84c0) {
        recorder.invalid_call = true;
    }
}

static void bind_texture(uint32_t target, uint32_t texture) {
    if (target != 0x0de1 || !object_exists(texture, 4)) {
        recorder.invalid_call = true;
    }
    recorder.bound_texture = texture;
}

static void pixel_store(uint32_t name, int32_t value) {
    if (name != 0x0cf5 || value != 1) {
        recorder.invalid_call = true;
    }
}

static void texture_parameter(uint32_t target, uint32_t name, int32_t value) {
    if (target != 0x0de1) {
        recorder.invalid_call = true;
    }
    if (name == 0x2802) {
        recorder.wrap_s = value;
    } else if (name == 0x2803) {
        recorder.wrap_t = value;
    } else if ((name != 0x2801 && name != 0x2800) || value != 0x2601) {
        recorder.invalid_call = true;
    }
}

static void texture_image(uint32_t target, int32_t level, int32_t internal_format,
                          int32_t width, int32_t height, int32_t border,
                          uint32_t format, uint32_t type, const void *pixels) {
    if (target != 0x0de1 || level || internal_format != 0x1908 || width < 1 ||
        height < 1 || border || format != 0x1908 || type != 0x1401) {
        recorder.invalid_call = true;
    }
    recorder.uploaded_pixels = pixels;
    if (recorder.failure == FAIL_UPLOAD) {
        recorder.error = 0x0505;
    }
}

static void delete_texture(int32_t count, const uint32_t *textures) {
    if (count != 1) {
        recorder.invalid_call = true;
    }
    delete_object(*textures, 4);
}

static void viewport(int32_t x, int32_t y, int32_t width, int32_t height) {
    if (x || y || width != 640 || height != 480) {
        recorder.invalid_call = true;
    }
}

static void scissor(int32_t x, int32_t y, int32_t width, int32_t height) {
    if (x < 0 || y < 0 || width < 0 || height < 0)
        recorder.invalid_call = true;
    recorder.scissor[0] = x;
    recorder.scissor[1] = y;
    recorder.scissor[2] = width;
    recorder.scissor[3] = height;
}

static void enable_state(uint32_t state) {
    if (state == 0x0be2)
        recorder.blending = true;
    else if (state == 0x0c11)
        recorder.scissoring = true;
    else
        recorder.invalid_call = true;
}

static void disable_state(uint32_t state) {
    if (state == 0x0be2) {
        recorder.blending = false;
    } else if (state == 0x0c11) {
        recorder.scissoring = false;
    } else if (state != 0x0b71 && state != 0x0b44 && state != 0x0b90 &&
               state != 0x0c11) {
        recorder.invalid_call = true;
    }
}

static void blend_equation(uint32_t equation) {
    if (equation != 0x8006) {
        recorder.invalid_call = true;
    }
}

static void blend_function(uint32_t source, uint32_t destination) {
    recorder.source_factor = source;
    recorder.destination_factor = destination;
}

static void color_mask(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha) {
    if (!red || !green || !blue || !alpha) {
        recorder.invalid_call = true;
    }
}

static void draw_arrays(uint32_t mode, int32_t first, int32_t count) {
    if (mode != 5 || first || count != 4 || recorder.draw_count >= 8 ||
        !object_exists(recorder.bound_buffer, 3) ||
        !object_exists(recorder.bound_texture, 4) ||
        !object_exists(recorder.bound_program, 2)) {
        recorder.invalid_call = true;
        return;
    }
    size_t index = recorder.draw_count++;
    recorder.drawn_textures[index] = recorder.bound_texture;
    recorder.drawn_sources[index] = recorder.source_factor;
    recorder.drawn_destinations[index] = recorder.destination_factor;
    recorder.drew_blending[index] = recorder.blending;
    recorder.drawn_clip[index] = recorder.coordinates_are_clip;
    recorder.drew_scissoring[index] = recorder.scissoring;
    memcpy(recorder.drawn_scissors[index], recorder.scissor, sizeof(recorder.scissor));
    if (recorder.failure == FAIL_DRAW) {
        recorder.error = 0x0502;
    }
}

static const AbGles2Api api = {
    .CreateShader = create_shader,
    .ShaderSource = shader_source,
    .CompileShader = compile_shader,
    .GetShaderiv = shader_status,
    .GetShaderInfoLog = info_log,
    .DeleteShader = delete_shader,
    .CreateProgram = create_program,
    .AttachShader = attach_shader,
    .BindAttribLocation = attribute_location,
    .LinkProgram = link_program,
    .GetProgramiv = program_status,
    .GetProgramInfoLog = info_log,
    .DeleteProgram = delete_program,
    .GetUniformLocation = uniform_location,
    .UseProgram = use_program,
    .Uniform1i = uniform_int,
    .Uniform1f = uniform_float,
    .Uniform2f = uniform_pair,
    .Uniform4fv = uniform_color,
    .GetShaderPrecisionFormat = shader_precision,
    .GetIntegerv = get_integer,
    .GetError = get_error,
    .GenBuffers = generate_buffer,
    .BindBuffer = bind_buffer,
    .BufferData = buffer_data,
    .BufferSubData = buffer_update,
    .DeleteBuffers = delete_buffer,
    .EnableVertexAttribArray = enable_attribute,
    .VertexAttribPointer = attribute_pointer,
    .GenTextures = generate_texture,
    .ActiveTexture = activate_texture,
    .BindTexture = bind_texture,
    .PixelStorei = pixel_store,
    .TexParameteri = texture_parameter,
    .TexImage2D = texture_image,
    .DeleteTextures = delete_texture,
    .Viewport = viewport,
    .Enable = enable_state,
    .Disable = disable_state,
    .BlendEquation = blend_equation,
    .BlendFunc = blend_function,
    .ColorMask = color_mask,
    .DrawArrays = draw_arrays,
    .Scissor = scissor,
};

static bool no_live_objects(void) {
    for (size_t index = 0; index < 64; ++index) {
        CHECK(!recorder.live[index]);
    }
    CHECK(!recorder.invalid_call);
    return true;
}

static bool creation_cleanup(void) {
    AbError error = {0};
    AbGles2 *renderer = NULL;
    for (Failure failure = FAIL_VERTEX; failure <= FAIL_BUFFER; ++failure) {
        recorder = (Recorder){.failure = failure};
        CHECK(!ab_gles2_create(&api, 4, 4096, &renderer, &error));
        CHECK(!renderer);
        CHECK(no_live_objects());
    }
    recorder = (Recorder){.high_precision = true};
    CHECK(ab_gles2_create(&api, 4, 4096, &renderer, &error));
    CHECK(ab_gles2_high_precision_uv(renderer));
    CHECK(recorder.source_count == 2 && recorder.high_precision_sources == 2);
    ab_gles2_free(renderer);
    CHECK(no_live_objects());
    recorder = (Recorder){0};
    AbGles2Api incomplete = api;
    incomplete.DrawArrays = NULL;
    CHECK(!ab_gles2_create(&incomplete, 4, 4096, &renderer, &error));
    CHECK(!ab_gles2_create(&api, SIZE_MAX, SIZE_MAX, &renderer, &error));
    CHECK(!ab_gles2_create(&api, 4, 0, &renderer, &error));
    CHECK(!renderer && no_live_objects());
    return true;
}

static bool upload_order_and_errors(void) {
    recorder = (Recorder){0};
    AbError error = {0};
    AbGles2 *renderer = NULL;
    CHECK(ab_gles2_create(&api, 2, 4096, &renderer, &error));
    CHECK(!ab_gles2_high_precision_uv(renderer));
    CHECK(recorder.high_precision_sources == 0);
    uint8_t pixels[16] = {20, 30, 40, 64};
    AbImage image = {2, 2, {pixels, sizeof(pixels)}};
    CHECK(ab_gles2_upload(renderer, 1, &image, true, false, &error));
    uint32_t first_texture = recorder.bound_texture;
    CHECK(recorder.uploaded_pixels == pixels && pixels[0] == 20 && pixels[3] == 64);
    CHECK(recorder.wrap_s == 0x2901 && recorder.wrap_t == 0x812f);
    CHECK(!ab_gles2_upload(renderer, 1, &image, false, false, &error));
    CHECK(!ab_gles2_upload(renderer, 0, &image, false, false, &error));
    image.width = 3;
    image.height = 1;
    image.rgba.size = 12;
    CHECK(!ab_gles2_upload(renderer, 2, &image, true, false, &error));
    recorder.failure = FAIL_UPLOAD;
    CHECK(!ab_gles2_upload(renderer, 2, &image, false, false, &error));
    CHECK(recorder.deleted[4] == 1);
    recorder.failure = FAIL_NONE;
    CHECK(ab_gles2_upload(renderer, 2, &image, false, false, &error));
    uint32_t second_texture = recorder.bound_texture;
    AbDrawList list = {0};
    CHECK(ab_draw_list_init(5, 4096, &list, &error));
    AbDrawCommand command = {.texture = 1, .color = {1, 1, 1, 1}, .opacity = 0.5f};
    const AbBlend blends[] = {AB_BLEND_PREMULTIPLIED, AB_BLEND_ADDITIVE,
                              AB_BLEND_STRAIGHT, AB_BLEND_MULTIPLY, AB_BLEND_OPAQUE};
    for (size_t index = 0; index < 5; ++index) {
        command.texture = index % 2 ? 2 : 1;
        command.blend = blends[index];
        command.coordinates = index % 2 ? AB_DRAW_CLIP : AB_DRAW_PIXELS;
        CHECK(ab_draw_list_push(&list, &command, &error));
    }
    list.commands[4].texture = 3;
    CHECK(!ab_gles2_render(renderer, &list, 640, 480, &error));
    CHECK(recorder.draw_count == 0);
    list.commands[4].texture = 1;
    CHECK(ab_gles2_render(renderer, &list, 640, 480, &error));
    CHECK(recorder.draw_count == 5);
    CHECK(!recorder.drawn_clip[0] && recorder.drawn_clip[1] &&
          !recorder.drawn_clip[2] && recorder.drawn_clip[3] && !recorder.drawn_clip[4]);
    CHECK(recorder.drawn_textures[0] == first_texture &&
          recorder.drawn_textures[1] == second_texture &&
          recorder.drawn_textures[2] == first_texture);
    CHECK(recorder.drawn_sources[0] == 1 && recorder.drawn_destinations[0] == 0x0303);
    CHECK(recorder.drawn_sources[1] == 1 && recorder.drawn_destinations[1] == 1);
    CHECK(recorder.drawn_sources[2] == 0x0302 &&
          recorder.drawn_destinations[2] == 0x0303);
    CHECK(recorder.drawn_sources[3] == 0x0306 && recorder.drawn_destinations[3] == 0);
    CHECK(!recorder.drew_blending[4]);
    recorder.draw_count = 0;
    list.commands[1].scissor = (AbDrawClip){-10, 20, 100, 80, true};
    list.commands[3].scissor = (AbDrawClip){700, 0, 800, 10, true};
    CHECK(ab_gles2_render(renderer, &list, 640, 480, &error));
    CHECK(recorder.draw_count == 4 && !recorder.drew_scissoring[0] &&
          recorder.drew_scissoring[1] && !recorder.drew_scissoring[2] &&
          recorder.drawn_scissors[1][0] == 0 && recorder.drawn_scissors[1][1] == 400 &&
          recorder.drawn_scissors[1][2] == 100 && recorder.drawn_scissors[1][3] == 60);
    list.commands[1].scissor = list.commands[3].scissor = (AbDrawClip){0};
    recorder.draw_count = 0;
    recorder.failure = FAIL_DRAW;
    CHECK(!ab_gles2_render(renderer, &list, 640, 480, &error));
    CHECK(strstr(error.message, "0x0502"));
    CHECK(!ab_gles2_render(renderer, &list, 0, 480, &error));
    recorder.failure = FAIL_NONE;
    CHECK(ab_gles2_remove_texture(renderer, 1, &error));
    CHECK(!ab_gles2_remove_texture(renderer, 1, &error));
    ab_draw_list_free(&list);
    ab_gles2_free(renderer);
    CHECK(recorder.deleted[4] == 3);
    CHECK(no_live_objects());
    return true;
}

int main(void) {
    if (!creation_cleanup() || !upload_order_and_errors()) {
        return 1;
    }
    puts("GLES2 ownership, upload, draw-order, blend-state, and driver-error checks "
         "passed.");
    return 0;
}
