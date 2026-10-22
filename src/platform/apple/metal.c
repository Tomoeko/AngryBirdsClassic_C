#include "ab/metal.h"

#include "ab_metal_shader.h"
#include "objects.h"

#include <stddef.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uintptr_t x;
    uintptr_t y;
    uintptr_t z;
} MetalOrigin;

typedef struct {
    uintptr_t width;
    uintptr_t height;
    uintptr_t depth;
} MetalSize;

typedef struct {
    MetalOrigin origin;
    MetalSize size;
} MetalRegion;

typedef struct {
    double x;
    double y;
    double width;
    double height;
    double near_depth;
    double far_depth;
} MetalViewport;

typedef struct {
    uintptr_t x;
    uintptr_t y;
    uintptr_t width;
    uintptr_t height;
} MetalScissor;

typedef struct {
    float color[4];
    float size[2];
    float opacity;
    float coordinates_are_clip;
} MetalParameters;

typedef struct {
    id object;
    unsigned sampler;
} MetalTexture;

struct AbMetal {
    id device;
    AbMetalFormat format;
    id pipelines[AB_BLEND_MULTIPLY + 1];
    id samplers[4];
    id depth_state;
    MetalTexture *textures;
    size_t texture_capacity;
    bool unified_memory;
    SEL set_viewport;
    SEL set_scissor;
    SEL set_cull;
    SEL set_depth;
    SEL set_pipeline;
    SEL set_vertex_bytes;
    SEL set_fragment_bytes;
    SEL set_texture;
    SEL set_sampler;
    SEL draw;
};

_Static_assert(sizeof(uintptr_t) == 8, "Apple Metal boundary requires 64-bit ABI");
_Static_assert(sizeof(AbDrawVertex) == 16 && sizeof(MetalParameters) == 32 &&
                   offsetof(MetalParameters, size) == 16 &&
                   offsetof(MetalParameters, opacity) == 24 &&
                   offsetof(MetalParameters, coordinates_are_clip) == 28,
               "Metal shader layout must match C parameters");

static void set_integer(id object, const char *selector, uintptr_t value) {
    ((void (*)(id, SEL, uintptr_t))objc_msgSend)(object, sel_registerName(selector),
                                                 value);
}

static uintptr_t pixel_format(AbMetalFormat format) {
    switch (format) {
        case AB_METAL_RGBA8:
            return 70;
        case AB_METAL_BGRA8:
            return 80;
        case AB_METAL_RGB565:
            return 40;
    }
    return 0;
}

static void set_object(id object, const char *selector, id value) {
    ((void (*)(id, SEL, id))objc_msgSend)(object, sel_registerName(selector), value);
}

static bool build_samplers(AbMetal *renderer, AbError *error) {
    for (unsigned index = 0; index < 4; ++index) {
        id descriptor = ab_apple_new("MTLSamplerDescriptor");
        if (!descriptor) {
            return ab_fail(error, "Cannot allocate Metal sampler descriptor");
        }
        set_integer(descriptor, "setMinFilter:", 1);
        set_integer(descriptor, "setMagFilter:", 1);
        set_integer(descriptor, "setSAddressMode:", (index & 1) ? 2 : 0);
        set_integer(descriptor, "setTAddressMode:", (index & 2) ? 2 : 0);
        renderer->samplers[index] = ((id(*)(id, SEL, id))objc_msgSend)(
            renderer->device, sel_registerName("newSamplerStateWithDescriptor:"),
            descriptor);
        ab_apple_release(descriptor);
        if (!renderer->samplers[index]) {
            return ab_fail(error, "Cannot create Metal sampler");
        }
    }
    return true;
}

static bool build_depth_state(AbMetal *renderer, AbError *error) {
    id descriptor = ab_apple_new("MTLDepthStencilDescriptor");
    if (!descriptor) {
        return ab_fail(error, "Cannot allocate Metal depth descriptor");
    }
    set_integer(descriptor, "setDepthCompareFunction:", 7);
    ((void (*)(id, SEL, bool))objc_msgSend)(
        descriptor, sel_registerName("setDepthWriteEnabled:"), false);
    renderer->depth_state = ((id(*)(id, SEL, id))objc_msgSend)(
        renderer->device, sel_registerName("newDepthStencilStateWithDescriptor:"),
        descriptor);
    ab_apple_release(descriptor);
    return renderer->depth_state || ab_fail(error, "Cannot create Metal depth state");
}

static bool build_pipeline(AbMetal *renderer, AbMetalFormat format, AbBlend blend,
                           id vertex, id fragment, AbError *error) {
    id descriptor = ab_apple_new("MTLRenderPipelineDescriptor");
    if (!descriptor) {
        return ab_fail(error, "Cannot allocate Metal pipeline descriptor");
    }
    set_object(descriptor, "setVertexFunction:", vertex);
    set_object(descriptor, "setFragmentFunction:", fragment);
    id attachments = ((id(*)(id, SEL))objc_msgSend)(
        descriptor, sel_registerName("colorAttachments"));
    id attachment = ((id(*)(id, SEL, uintptr_t))objc_msgSend)(
        attachments, sel_registerName("objectAtIndexedSubscript:"), 0);
    set_integer(attachment, "setPixelFormat:", pixel_format(format));
    ((void (*)(id, SEL, bool))objc_msgSend)(
        attachment, sel_registerName("setBlendingEnabled:"), blend != AB_BLEND_OPAQUE);
    uintptr_t source = blend == AB_BLEND_STRAIGHT   ? 4
                       : blend == AB_BLEND_MULTIPLY ? 6
                                                    : 1;
    uintptr_t destination = blend == AB_BLEND_ADDITIVE   ? 1
                            : blend == AB_BLEND_MULTIPLY ? 0
                                                         : 5;
    set_integer(attachment, "setSourceRGBBlendFactor:", source);
    set_integer(attachment, "setSourceAlphaBlendFactor:", source);
    set_integer(attachment, "setDestinationRGBBlendFactor:", destination);
    set_integer(attachment, "setDestinationAlphaBlendFactor:", destination);
    set_integer(attachment, "setRgbBlendOperation:", 0);
    set_integer(attachment, "setAlphaBlendOperation:", 0);
    id native_error = nil;
    renderer->pipelines[blend] = ((id(*)(id, SEL, id, id *))objc_msgSend)(
        renderer->device,
        sel_registerName("newRenderPipelineStateWithDescriptor:error:"), descriptor,
        &native_error);
    ab_apple_release(descriptor);
    return renderer->pipelines[blend] ||
           ab_apple_error(error, "Metal pipeline creation failed", native_error);
}

static bool build_pipelines(AbMetal *renderer, AbMetalFormat format, AbError *error) {
    id source = ab_apple_string(ab_sprite_metal_source);
    id options = ab_apple_new("MTLCompileOptions");
    if (!source || !options) {
        ab_apple_release(source);
        ab_apple_release(options);
        return ab_fail(error, "Cannot allocate Metal shader source or options");
    }
    ((void (*)(id, SEL, bool))objc_msgSend)(
        options, sel_registerName("setFastMathEnabled:"), false);
    id native_error = nil;
    id library = ((id(*)(id, SEL, id, id, id *))objc_msgSend)(
        renderer->device, sel_registerName("newLibraryWithSource:options:error:"),
        source, options, &native_error);
    ab_apple_release(source);
    ab_apple_release(options);
    if (!library) {
        return ab_apple_error(error, "Metal shader compilation failed", native_error);
    }
    id vertex_name = ab_apple_string("ab_sprite_vertex");
    id fragment_name = ab_apple_string("ab_sprite_fragment");
    id vertex = ((id(*)(id, SEL, id))objc_msgSend)(
        library, sel_registerName("newFunctionWithName:"), vertex_name);
    id fragment = ((id(*)(id, SEL, id))objc_msgSend)(
        library, sel_registerName("newFunctionWithName:"), fragment_name);
    bool okay = vertex && fragment;
    if (!okay) {
        ab_fail(error, "Metal sprite shader entry point is missing");
    }
    for (AbBlend blend = AB_BLEND_OPAQUE; okay && blend <= AB_BLEND_MULTIPLY; ++blend) {
        okay = build_pipeline(renderer, format, blend, vertex, fragment, error);
    }
    ab_apple_release(vertex_name);
    ab_apple_release(fragment_name);
    ab_apple_release(vertex);
    ab_apple_release(fragment);
    ab_apple_release(library);
    return okay;
}

bool ab_metal_create(void *device, AbMetalFormat format, size_t texture_capacity,
                     size_t allocation_limit, AbMetal **output, AbError *error) {
    if (!output) {
        return ab_fail(error, "Missing Metal output");
    }
    *output = NULL;
    if (!device || (unsigned)format > AB_METAL_RGB565 ||
        texture_capacity > UINT32_MAX || allocation_limit < sizeof(AbMetal) ||
        texture_capacity >
            (allocation_limit - sizeof(AbMetal)) / sizeof(MetalTexture)) {
        return ab_fail(error, "Invalid Metal configuration or allocation limit");
    }
    AbMetal *renderer = calloc(1, sizeof(*renderer));
    if (!renderer) {
        return ab_fail(error, "Cannot allocate Metal backend");
    }
    renderer->device = ab_apple_retain((id)device);
    renderer->format = format;
    if (texture_capacity) {
        renderer->textures = calloc(texture_capacity, sizeof(*renderer->textures));
        if (!renderer->textures) {
            ab_fail(error, "Cannot allocate Metal texture slots");
            ab_metal_free(renderer);
            return false;
        }
    }
    renderer->texture_capacity = texture_capacity;
    renderer->unified_memory = ((bool (*)(id, SEL))objc_msgSend)(
        renderer->device, sel_registerName("hasUnifiedMemory"));
    renderer->set_viewport = sel_registerName("setViewport:");
    renderer->set_scissor = sel_registerName("setScissorRect:");
    renderer->set_cull = sel_registerName("setCullMode:");
    renderer->set_depth = sel_registerName("setDepthStencilState:");
    renderer->set_pipeline = sel_registerName("setRenderPipelineState:");
    renderer->set_vertex_bytes = sel_registerName("setVertexBytes:length:atIndex:");
    renderer->set_fragment_bytes = sel_registerName("setFragmentBytes:length:atIndex:");
    renderer->set_texture = sel_registerName("setFragmentTexture:atIndex:");
    renderer->set_sampler = sel_registerName("setFragmentSamplerState:atIndex:");
    renderer->draw = sel_registerName("drawPrimitives:vertexStart:vertexCount:");
    id pool = ab_apple_new("NSAutoreleasePool");
    bool okay = build_samplers(renderer, error) && build_depth_state(renderer, error) &&
                build_pipelines(renderer, format, error);
    ab_apple_release(pool);
    if (!okay) {
        ab_metal_free(renderer);
        return false;
    }
    *output = renderer;
    return true;
}

static bool valid_texture(const AbMetal *renderer, uint32_t texture) {
    return renderer && texture && texture <= renderer->texture_capacity;
}

bool ab_metal_upload(AbMetal *renderer, uint32_t texture, const AbImage *image,
                     bool repeat_s, bool repeat_t, AbError *error) {
    if (!valid_texture(renderer, texture) || renderer->textures[texture - 1].object ||
        !image || !image->width || !image->height || image->width > 8192 ||
        image->height > 8192 || !image->rgba.data ||
        image->rgba.size != (size_t)image->width * image->height * 4) {
        return ab_fail(error, "Invalid Metal texture upload or slot is occupied");
    }
    bool power_of_two = ((image->width - 1) & image->width) == 0 &&
                        ((image->height - 1) & image->height) == 0;
    if (!power_of_two && (repeat_s || repeat_t)) {
        return ab_fail(error,
                       "Shared core GLES2 contract requires NPOT clamp sampling");
    }
    id descriptor = ab_apple_new("MTLTextureDescriptor");
    if (!descriptor) {
        return ab_fail(error, "Cannot allocate Metal texture descriptor");
    }
    set_integer(descriptor, "setTextureType:", 2);
    set_integer(descriptor, "setPixelFormat:", 70);
    set_integer(descriptor, "setWidth:", image->width);
    set_integer(descriptor, "setHeight:", image->height);
    set_integer(descriptor, "setUsage:", 1);
    set_integer(descriptor, "setStorageMode:", renderer->unified_memory ? 0 : 1);
    id object = ((id(*)(id, SEL, id))objc_msgSend)(
        renderer->device, sel_registerName("newTextureWithDescriptor:"), descriptor);
    ab_apple_release(descriptor);
    if (!object) {
        return ab_fail(error, "Cannot allocate Metal texture");
    }
    MetalRegion region = {{0, 0, 0}, {image->width, image->height, 1}};
    ((void (*)(id, SEL, MetalRegion, uintptr_t, const void *, uintptr_t))objc_msgSend)(
        object, sel_registerName("replaceRegion:mipmapLevel:withBytes:bytesPerRow:"),
        region, 0, image->rgba.data, (uintptr_t)image->width * 4);
    renderer->textures[texture - 1] =
        (MetalTexture){object, (repeat_s ? 1u : 0u) | (repeat_t ? 2u : 0u)};
    return true;
}

bool ab_metal_remove_texture(AbMetal *renderer, uint32_t texture, AbError *error) {
    if (!valid_texture(renderer, texture) || !renderer->textures[texture - 1].object) {
        return ab_fail(error, "Missing Metal texture");
    }
    ab_apple_release(renderer->textures[texture - 1].object);
    renderer->textures[texture - 1] = (MetalTexture){0};
    return true;
}

bool ab_metal_render(AbMetal *renderer, void *encoder, const AbDrawList *list,
                     uint32_t width, uint32_t height, AbError *error) {
    if (!renderer || !encoder || !list || list->count > list->capacity ||
        (list->count && !list->commands) || !width || !height || width > INT32_MAX ||
        height > INT32_MAX) {
        return ab_fail(error, "Invalid Metal draw list, encoder, or framebuffer size");
    }
    for (size_t index = 0; index < list->count; ++index) {
        const AbDrawCommand *command = list->commands + index;
        if (!valid_texture(renderer, command->texture) ||
            !renderer->textures[command->texture - 1].object ||
            !ab_draw_command_check(command, NULL)) {
            return ab_fail(error, "Invalid Metal draw command at index %zu", index);
        }
    }
    id native_encoder = (id)encoder;
    MetalViewport viewport = {0, 0, width, height, 0, 1};
    MetalScissor scissor = {0, 0, width, height};
    ((void (*)(id, SEL, MetalViewport))objc_msgSend)(native_encoder,
                                                     renderer->set_viewport, viewport);
    ((void (*)(id, SEL, MetalScissor))objc_msgSend)(native_encoder,
                                                    renderer->set_scissor, scissor);
    ((void (*)(id, SEL, uintptr_t))objc_msgSend)(native_encoder, renderer->set_cull, 0);
    ((void (*)(id, SEL, id))objc_msgSend)(native_encoder, renderer->set_depth,
                                          renderer->depth_state);
    void (*bind_object)(id, SEL, id) = (void (*)(id, SEL, id))objc_msgSend;
    void (*bind_bytes)(id, SEL, const void *, uintptr_t, uintptr_t) =
        (void (*)(id, SEL, const void *, uintptr_t, uintptr_t))objc_msgSend;
    void (*bind_indexed)(id, SEL, id, uintptr_t) =
        (void (*)(id, SEL, id, uintptr_t))objc_msgSend;
    void (*draw)(id, SEL, uintptr_t, uintptr_t, uintptr_t) =
        (void (*)(id, SEL, uintptr_t, uintptr_t, uintptr_t))objc_msgSend;
    for (size_t index = 0; index < list->count; ++index) {
        const AbDrawCommand *command = list->commands + index;
        AbDrawScissor clip;
        if (!ab_draw_clip_scissor(command->scissor, width, height, &clip, error))
            return false;
        if (!clip.width || !clip.height)
            continue;
        if (scissor.x != clip.x || scissor.y != clip.y || scissor.width != clip.width ||
            scissor.height != clip.height) {
            scissor = (MetalScissor){clip.x, clip.y, clip.width, clip.height};
            ((void (*)(id, SEL, MetalScissor))objc_msgSend)(
                native_encoder, renderer->set_scissor, scissor);
        }
        const MetalTexture *texture = renderer->textures + command->texture - 1;
        MetalParameters parameters = {.size = {(float)width, (float)height},
                                      .opacity = command->opacity,
                                      .coordinates_are_clip =
                                          command->coordinates == AB_DRAW_CLIP};
        memcpy(parameters.color, command->color, sizeof(parameters.color));
        bind_object(native_encoder, renderer->set_pipeline,
                    renderer->pipelines[command->blend]);
        bind_bytes(native_encoder, renderer->set_vertex_bytes, &command->quad,
                   sizeof(command->quad), 0);
        bind_bytes(native_encoder, renderer->set_vertex_bytes, &parameters,
                   sizeof(parameters), 1);
        bind_bytes(native_encoder, renderer->set_fragment_bytes, &parameters,
                   sizeof(parameters), 1);
        bind_indexed(native_encoder, renderer->set_texture, texture->object, 0);
        bind_indexed(native_encoder, renderer->set_sampler,
                     renderer->samplers[texture->sampler], 0);
        draw(native_encoder, renderer->draw, 4, 0, 4);
    }
    return true;
}

bool ab_metal_capture(AbMetal *renderer, const AbDrawList *list, uint32_t width,
                      uint32_t height, const float clear[4], size_t decoded_limit,
                      AbImage *output, AbError *error) {
    if (!output) {
        return ab_fail(error, "Missing Metal capture output");
    }
    *output = (AbImage){0};
    if (!renderer || !list || !clear || !width || !height || width > 16384 ||
        height > 16384 || (size_t)width > decoded_limit / 4 / height) {
        return ab_fail(error, "Invalid Metal capture input or pixel budget");
    }
    for (size_t channel = 0; channel < 4; ++channel) {
        if (!isfinite(clear[channel]) || clear[channel] < 0 || clear[channel] > 1) {
            return ab_fail(error, "Invalid Metal capture clear color");
        }
    }
    typedef struct {
        double red, green, blue, alpha;
    } ClearColor;
    bool okay = false;
    size_t byte_count = (size_t)width * height * 4;
    uint8_t *pixels = malloc(byte_count);
    id pool = ab_apple_new("NSAutoreleasePool");
    id descriptor = ab_apple_new("MTLTextureDescriptor");
    id texture = nil, queue = nil, pass = nil;
    if (!pixels || !descriptor) {
        ab_fail(error, "Cannot allocate Metal capture pixels or descriptor");
        goto release_capture;
    }
    set_integer(descriptor, "setTextureType:", 2);
    set_integer(descriptor, "setPixelFormat:", pixel_format(renderer->format));
    set_integer(descriptor, "setWidth:", width);
    set_integer(descriptor, "setHeight:", height);
    set_integer(descriptor, "setUsage:", 4);
    set_integer(descriptor, "setStorageMode:", renderer->unified_memory ? 0 : 1);
    texture = ((id(*)(id, SEL, id))objc_msgSend)(
        renderer->device, sel_registerName("newTextureWithDescriptor:"), descriptor);
    queue = ((id(*)(id, SEL))objc_msgSend)(renderer->device,
                                           sel_registerName("newCommandQueue"));
    pass = ab_apple_new("MTLRenderPassDescriptor");
    if (!texture || !queue || !pass) {
        ab_fail(error, "Cannot create offscreen Metal capture objects");
        goto release_capture;
    }
    id attachments =
        ((id(*)(id, SEL))objc_msgSend)(pass, sel_registerName("colorAttachments"));
    id attachment = ((id(*)(id, SEL, uintptr_t))objc_msgSend)(
        attachments, sel_registerName("objectAtIndexedSubscript:"), 0);
    set_object(attachment, "setTexture:", texture);
    set_integer(attachment, "setLoadAction:", 2);
    set_integer(attachment, "setStoreAction:", 1);
    ClearColor color = {clear[0], clear[1], clear[2], clear[3]};
    ((void (*)(id, SEL, ClearColor))objc_msgSend)(
        attachment, sel_registerName("setClearColor:"), color);
    id command =
        ((id(*)(id, SEL))objc_msgSend)(queue, sel_registerName("commandBuffer"));
    id encoder = ((id(*)(id, SEL, id))objc_msgSend)(
        command, sel_registerName("renderCommandEncoderWithDescriptor:"), pass);
    if (!command || !encoder) {
        ab_fail(error, "Cannot create Metal capture commands");
        goto release_capture;
    }
    bool rendered = ab_metal_render(renderer, encoder, list, width, height, error);
    ((void (*)(id, SEL))objc_msgSend)(encoder, sel_registerName("endEncoding"));
    if (!rendered) {
        goto release_capture;
    }
    if (!renderer->unified_memory) {
        id blit = ((id(*)(id, SEL))objc_msgSend)(
            command, sel_registerName("blitCommandEncoder"));
        if (!blit) {
            ab_fail(error, "Cannot synchronize managed Metal capture texture");
            goto release_capture;
        }
        set_object(blit, "synchronizeResource:", texture);
        ((void (*)(id, SEL))objc_msgSend)(blit, sel_registerName("endEncoding"));
    }
    ((void (*)(id, SEL))objc_msgSend)(command, sel_registerName("commit"));
    ((void (*)(id, SEL))objc_msgSend)(command, sel_registerName("waitUntilCompleted"));
    uintptr_t status =
        ((uintptr_t (*)(id, SEL))objc_msgSend)(command, sel_registerName("status"));
    if (status != 4) {
        id native_error =
            ((id(*)(id, SEL))objc_msgSend)(command, sel_registerName("error"));
        ab_apple_error(error, "Metal capture command failed", native_error);
        goto release_capture;
    }
    MetalRegion region = {{0, 0, 0}, {width, height, 1}};
    uintptr_t row_bytes =
        (uintptr_t)width * (renderer->format == AB_METAL_RGB565 ? 2u : 4u);
    ((void (*)(id, SEL, void *, uintptr_t, MetalRegion, uintptr_t))objc_msgSend)(
        texture, sel_registerName("getBytes:bytesPerRow:fromRegion:mipmapLevel:"),
        pixels, row_bytes, region, 0);
    if (renderer->format == AB_METAL_RGB565) {
        for (size_t pixel = byte_count / 4; pixel > 0; --pixel) {
            size_t index = pixel - 1;
            uint16_t packed = ab_u16le(pixels + index * 2);
            unsigned red = packed >> 11;
            unsigned green = (packed >> 5) & 63;
            unsigned blue = packed & 31;
            pixels[index * 4] = (uint8_t)((red * 255 + 15) / 31);
            pixels[index * 4 + 1] = (uint8_t)((green * 255 + 31) / 63);
            pixels[index * 4 + 2] = (uint8_t)((blue * 255 + 15) / 31);
            pixels[index * 4 + 3] = 255;
        }
    } else if (renderer->format == AB_METAL_BGRA8) {
        for (size_t pixel = 0; pixel < byte_count; pixel += 4) {
            uint8_t red = pixels[pixel + 2];
            pixels[pixel + 2] = pixels[pixel];
            pixels[pixel] = red;
        }
    }
    *output = (AbImage){width, height, {pixels, byte_count}};
    pixels = NULL;
    okay = true;

release_capture:
    free(pixels);
    ab_apple_release(pass);
    ab_apple_release(queue);
    ab_apple_release(texture);
    ab_apple_release(descriptor);
    ab_apple_release(pool);
    return okay;
}

void ab_metal_free(AbMetal *renderer) {
    if (renderer) {
        if (renderer->textures) {
            for (size_t index = 0; index < renderer->texture_capacity; ++index) {
                ab_apple_release(renderer->textures[index].object);
            }
        }
        for (size_t index = 0; index <= AB_BLEND_MULTIPLY; ++index) {
            ab_apple_release(renderer->pipelines[index]);
        }
        for (size_t index = 0; index < 4; ++index) {
            ab_apple_release(renderer->samplers[index]);
        }
        ab_apple_release(renderer->depth_state);
        ab_apple_release(renderer->device);
        free(renderer->textures);
        free(renderer);
    }
}
