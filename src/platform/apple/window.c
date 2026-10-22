#include "ab/window.h"

#include "ab/metal.h"
#include "objects.h"

#include <CoreGraphics/CGGeometry.h>
#include <math.h>
#include <pthread.h>
#include <stdlib.h>

extern id MTLCopyAllDevices(void);

struct AbWindow {
    id application;
    id native;
    id view;
    id layer;
    id device;
    id queue;
    id last_command;
    AbMetal *renderer;
    uint32_t width;
    uint32_t height;
    bool shown;
    bool closed;
};

typedef struct {
    double red, green, blue, alpha;
} WindowClear;

static void set_integer(id object, const char *name, uintptr_t value) {
    ((void (*)(id, SEL, uintptr_t))objc_msgSend)(object, sel_registerName(name), value);
}

static void set_boolean(id object, const char *name, bool value) {
    ((void (*)(id, SEL, bool))objc_msgSend)(object, sel_registerName(name), value);
}

static void set_object(id object, const char *name, id value) {
    ((void (*)(id, SEL, id))objc_msgSend)(object, sel_registerName(name), value);
}

static id get_object(id object, const char *name) {
    return ((id(*)(id, SEL))objc_msgSend)(object, sel_registerName(name));
}

static bool main_window(AbWindow *window, AbError *error) {
    return (window && pthread_main_np()) ||
           ab_fail(error, "Native window operations require a window and main thread");
}

static bool create_surface(AbWindow *window, AbWindowConfig config, AbError *error) {
    CGRect rectangle = {{0, 0}, {config.width, config.height}};
    id allocated = ((id(*)(id, SEL))objc_msgSend)((id)objc_getClass("NSWindow"),
                                                  sel_registerName("alloc"));
    /* Titled, closable, miniaturizable; fixed size until aspect policy is recovered. */
    window->native = ((id(*)(id, SEL, CGRect, uintptr_t, uintptr_t, bool))objc_msgSend)(
        allocated, sel_registerName("initWithContentRect:styleMask:backing:defer:"),
        rectangle, 7, 2, false);
    set_boolean(window->native, "setReleasedWhenClosed:", false);
    allocated = ((id(*)(id, SEL))objc_msgSend)((id)objc_getClass("NSView"),
                                               sel_registerName("alloc"));
    window->view = ((id(*)(id, SEL, CGRect))objc_msgSend)(
        allocated, sel_registerName("initWithFrame:"), rectangle);
    window->layer = ab_apple_new("CAMetalLayer");
    window->queue = get_object(window->device, "newCommandQueue");
    if (!window->native || !window->view || !window->layer || !window->queue) {
        return ab_fail(error, "Cannot create native window or Metal surface");
    }
    set_boolean(window->native, "setAcceptsMouseMovedEvents:", true);
    id title = ab_apple_string(config.title);
    if (!title) {
        return ab_fail(error, "Cannot create native window title");
    }
    set_object(window->native, "setTitle:", title);
    ab_apple_release(title);
    set_object(window->layer, "setDevice:", window->device);
    set_integer(window->layer, "setPixelFormat:", 80);
    set_boolean(window->layer, "setFramebufferOnly:", true);
    set_boolean(window->layer, "setPresentsWithTransaction:", false);
    ((void (*)(id, SEL, CGSize))objc_msgSend)(
        window->layer, sel_registerName("setDrawableSize:"), rectangle.size);
    set_object(window->view, "setLayer:", window->layer);
    set_boolean(window->view, "setWantsLayer:", true);
    set_object(window->native, "setContentView:", window->view);
    ((void (*)(id, SEL))objc_msgSend)(window->native, sel_registerName("center"));
    if (config.visible) {
        set_object(window->native,
                   config.focus ? "makeKeyAndOrderFront:" : "orderBack:", nil);
        window->shown = true;
    }
    return true;
}

bool ab_window_open_metal(AbWindowConfig config, AbWindow **output, AbError *error) {
    if (!output) {
        return ab_fail(error, "Missing native window output");
    }
    *output = NULL;
    if (!pthread_main_np() || !config.title || !config.width || !config.height ||
        config.width > 16384 || config.height > 16384 ||
        (config.focus && !config.visible)) {
        return ab_fail(error, "Invalid native window configuration or thread");
    }
    id pool = ab_apple_new("NSAutoreleasePool");
    AbWindow *window = calloc(1, sizeof(*window));
    bool okay = false;
    if (!window) {
        ab_fail(error, "Cannot allocate native window");
        goto release_pool;
    }
    window->width = config.width;
    window->height = config.height;
    window->application = ((id(*)(id, SEL))objc_msgSend)(
        (id)objc_getClass("NSApplication"), sel_registerName("sharedApplication"));
    id devices = MTLCopyAllDevices();
    uintptr_t count =
        ((uintptr_t (*)(id, SEL))objc_msgSend)(devices, sel_registerName("count"));
    if (count) {
        window->device = ab_apple_retain(((id(*)(id, SEL, uintptr_t))objc_msgSend)(
            devices, sel_registerName("objectAtIndex:"), 0));
    }
    ab_apple_release(devices);
    if (!window->application || !window->device) {
        ab_fail(error, "Native application or Metal device is unavailable");
        goto release_window;
    }
    set_integer(window->application, "setActivationPolicy:", 0);
    ((void (*)(id, SEL))objc_msgSend)(window->application,
                                      sel_registerName("finishLaunching"));
    if (!create_surface(window, config, error) ||
        !ab_metal_create(window->device, AB_METAL_BGRA8, config.texture_capacity,
                         config.allocation_limit, &window->renderer, error)) {
        goto release_window;
    }
    *output = window;
    okay = true;
    goto release_pool;

release_window:
    ab_window_close(window);
release_pool:
    ab_apple_release(pool);
    return okay;
}

bool ab_window_upload(AbWindow *window, uint32_t texture, const AbImage *image,
                      bool repeat_s, bool repeat_t, AbError *error) {
    if (!main_window(window, error) || window->closed) {
        return false;
    }
    id pool = ab_apple_new("NSAutoreleasePool");
    bool okay =
        ab_metal_upload(window->renderer, texture, image, repeat_s, repeat_t, error);
    ab_apple_release(pool);
    return okay;
}

static AbWindowEvent native_event(AbWindow *window, id event) {
    AbWindowEvent result = {0};
    id event_window = get_object(event, "window");
    if (event_window != window->native) {
        return result;
    }
    uintptr_t type =
        ((uintptr_t (*)(id, SEL))objc_msgSend)(event, sel_registerName("type"));
    switch (type) {
        case 1:
            result.type = AB_WINDOW_POINTER_DOWN;
            break;
        case 2:
            result.type = AB_WINDOW_POINTER_UP;
            break;
        case 5:
        case 6:
            result.type = AB_WINDOW_POINTER_MOVE;
            break;
        case 10:
        case 11:
            result.type = type == 10 ? AB_WINDOW_KEY_DOWN : AB_WINDOW_KEY_UP;
            result.key = ((uint16_t (*)(id, SEL))objc_msgSend)(
                event, sel_registerName("keyCode"));
            result.repeat =
                ((bool (*)(id, SEL))objc_msgSend)(event, sel_registerName("isARepeat"));
            break;
        default:
            return result;
    }
    if (result.type <= AB_WINDOW_POINTER_UP) {
        CGPoint point = ((CGPoint(*)(id, SEL))objc_msgSend)(
            event, sel_registerName("locationInWindow"));
        result.x = (float)point.x;
        result.y = (float)((double)window->height - point.y);
    }
    result.time =
        ((double (*)(id, SEL))objc_msgSend)(event, sel_registerName("timestamp"));
    return result;
}

bool ab_window_poll(AbWindow *window, AbWindowEvent *output, AbError *error) {
    if (!main_window(window, error) || !output) {
        return ab_fail(error, "Invalid native window event output");
    }
    AbWindowEvent result = {0};
    id pool = ab_apple_new("NSAutoreleasePool");
    bool visible = ((bool (*)(id, SEL))objc_msgSend)(window->native,
                                                     sel_registerName("isVisible"));
    if (window->shown && !visible && !window->closed) {
        window->closed = true;
        result.type = AB_WINDOW_CLOSE;
    }
    id date = ((id(*)(id, SEL))objc_msgSend)((id)objc_getClass("NSDate"),
                                             sel_registerName("distantPast"));
    id mode = ab_apple_string("kCFRunLoopDefaultMode");
    while (result.type == AB_WINDOW_NONE) {
        id event = ((id(*)(id, SEL, uint64_t, id, id, bool))objc_msgSend)(
            window->application,
            sel_registerName("nextEventMatchingMask:untilDate:inMode:dequeue:"),
            UINT64_MAX, date, mode, true);
        if (!event) {
            break;
        }
        result = native_event(window, event);
        set_object(window->application, "sendEvent:", event);
    }
    ((void (*)(id, SEL))objc_msgSend)(window->application,
                                      sel_registerName("updateWindows"));
    ab_apple_release(mode);
    ab_apple_release(pool);
    *output = result;
    return true;
}

static bool previous_submission(AbWindow *window, AbError *error) {
    if (window->last_command) {
        uintptr_t status = ((uintptr_t (*)(id, SEL))objc_msgSend)(
            window->last_command, sel_registerName("status"));
        if (status == 5) {
            return ab_apple_error(error, "Native window Metal command failed",
                                  get_object(window->last_command, "error"));
        }
    }
    return true;
}

bool ab_window_present(AbWindow *window, const AbDrawList *list, const float clear[4],
                       AbError *error) {
    if (!main_window(window, error) || window->closed || !clear) {
        return ab_fail(error, "Invalid or closed native window presentation");
    }
    for (size_t channel = 0; channel < 4; ++channel) {
        if (!isfinite(clear[channel]) || clear[channel] < 0 || clear[channel] > 1) {
            return ab_fail(error, "Invalid native window clear color");
        }
    }
    bool okay = false;
    id pool = ab_apple_new("NSAutoreleasePool");
    id pass = nil;
    if (!previous_submission(window, error)) {
        goto release_pool;
    }
    id drawable = get_object(window->layer, "nextDrawable");
    id command = get_object(window->queue, "commandBuffer");
    pass = ab_apple_new("MTLRenderPassDescriptor");
    if (!drawable || !command || !pass) {
        ab_fail(error, "Native window drawable or Metal command is unavailable");
        goto release_pool;
    }
    id texture = get_object(drawable, "texture");
    uintptr_t width =
        ((uintptr_t (*)(id, SEL))objc_msgSend)(texture, sel_registerName("width"));
    uintptr_t height =
        ((uintptr_t (*)(id, SEL))objc_msgSend)(texture, sel_registerName("height"));
    if (width != window->width || height != window->height) {
        ab_fail(error, "Native drawable dimensions changed unexpectedly");
        goto release_pool;
    }
    id attachments = get_object(pass, "colorAttachments");
    id attachment = ((id(*)(id, SEL, uintptr_t))objc_msgSend)(
        attachments, sel_registerName("objectAtIndexedSubscript:"), 0);
    set_object(attachment, "setTexture:", texture);
    set_integer(attachment, "setLoadAction:", 2);
    set_integer(attachment, "setStoreAction:", 1);
    WindowClear color = {clear[0], clear[1], clear[2], clear[3]};
    ((void (*)(id, SEL, WindowClear))objc_msgSend)(
        attachment, sel_registerName("setClearColor:"), color);
    id encoder = ((id(*)(id, SEL, id))objc_msgSend)(
        command, sel_registerName("renderCommandEncoderWithDescriptor:"), pass);
    if (!encoder) {
        ab_fail(error, "Cannot create native window render encoder");
        goto release_pool;
    }
    okay = ab_metal_render(window->renderer, encoder, list, window->width,
                           window->height, error);
    ((void (*)(id, SEL))objc_msgSend)(encoder, sel_registerName("endEncoding"));
    if (!okay) {
        goto release_pool;
    }
    set_object(command, "presentDrawable:", drawable);
    ((void (*)(id, SEL))objc_msgSend)(command, sel_registerName("commit"));
    ab_apple_release(window->last_command);
    window->last_command = ab_apple_retain(command);

release_pool:
    ab_apple_release(pass);
    ab_apple_release(pool);
    return okay;
}

bool ab_window_finish(AbWindow *window, AbError *error) {
    if (!main_window(window, error)) {
        return false;
    }
    id pool = ab_apple_new("NSAutoreleasePool");
    if (window->last_command) {
        ((void (*)(id, SEL))objc_msgSend)(window->last_command,
                                          sel_registerName("waitUntilCompleted"));
    }
    bool okay = previous_submission(window, error);
    ab_apple_release(pool);
    return okay;
}

void ab_window_close(AbWindow *window) {
    if (!window || !pthread_main_np()) {
        return;
    }
    id pool = ab_apple_new("NSAutoreleasePool");
    if (window->last_command) {
        ((void (*)(id, SEL))objc_msgSend)(window->last_command,
                                          sel_registerName("waitUntilCompleted"));
    }
    ((void (*)(id, SEL))objc_msgSend)(window->native, sel_registerName("close"));
    ab_metal_free(window->renderer);
    ab_apple_release(window->last_command);
    ab_apple_release(window->queue);
    ab_apple_release(window->native);
    ab_apple_release(window->view);
    ab_apple_release(window->layer);
    ab_apple_release(window->device);
    free(window);
    ab_apple_release(pool);
}
