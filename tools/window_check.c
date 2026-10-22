#include "ab/window.h"

#include "../src/platform/apple/objects.h"

#include <CoreGraphics/CGGeometry.h>
#include <stdio.h>
#include <string.h>

static id check_native_window(void) {
    id application = ((id(*)(id, SEL))objc_msgSend)(
        (id)objc_getClass("NSApplication"), sel_registerName("sharedApplication"));
    id windows =
        ((id(*)(id, SEL))objc_msgSend)(application, sel_registerName("windows"));
    uintptr_t count =
        ((uintptr_t (*)(id, SEL))objc_msgSend)(windows, sel_registerName("count"));
    for (uintptr_t index = 0; index < count; ++index) {
        id window = ((id(*)(id, SEL, uintptr_t))objc_msgSend)(
            windows, sel_registerName("objectAtIndex:"), index);
        id title = ((id(*)(id, SEL))objc_msgSend)(window, sel_registerName("title"));
        const char *name = ((const char *(*)(id, SEL))objc_msgSend)(
            title, sel_registerName("UTF8String"));
        if (name && strcmp(name, "AngryBirdsC native presentation check") == 0) {
            return window;
        }
    }
    return nil;
}

static bool check_events(AbWindow *window, AbError *error) {
    id pool = ab_apple_new("NSAutoreleasePool");
    id native = check_native_window();
    bool okay = false;
    if (!native) {
        ab_fail(error, "Native input check cannot locate its own window");
        goto release_pool;
    }
    id application = ((id(*)(id, SEL))objc_msgSend)(
        (id)objc_getClass("NSApplication"), sel_registerName("sharedApplication"));
    intptr_t number =
        ((intptr_t (*)(id, SEL))objc_msgSend)(native, sel_registerName("windowNumber"));
    const uintptr_t types[] = {1, 6, 2};
    const AbWindowEventType expected[] = {AB_WINDOW_POINTER_DOWN,
                                          AB_WINDOW_POINTER_MOVE, AB_WINDOW_POINTER_UP};
    for (size_t index = 0; index < 3; ++index) {
        CGPoint location = {8, 20};
        id native_event = ((id(*)(id, SEL, uintptr_t, CGPoint, uintptr_t, double,
                                  intptr_t, id, intptr_t, intptr_t,
                                  float))objc_msgSend)(
            (id)objc_getClass("NSEvent"),
            sel_registerName("mouseEventWithType:location:modifierFlags:timestamp:"
                             "windowNumber:context:eventNumber:clickCount:pressure:"),
            types[index], location, 0, 17.0 + (double)index, number, nil,
            (intptr_t)index + 1, 1, 1.0f);
        if (!native_event) {
            ab_fail(error, "Native input check could not create its controlled event");
            goto release_pool;
        }
        ((void (*)(id, SEL, id, bool))objc_msgSend)(
            application, sel_registerName("postEvent:atStart:"), native_event, true);
        AbWindowEvent event;
        if (!ab_window_poll(window, &event, error)) {
            goto release_pool;
        }
        if (event.type != expected[index] || event.x != 8 || event.y != 44 ||
            event.time != 17.0 + (double)index) {
            ab_fail(error, "Native input event mapping differs at index %zu", index);
            goto release_pool;
        }
    }
    ((void (*)(id, SEL))objc_msgSend)(native, sel_registerName("close"));
    AbWindowEvent event;
    if (!ab_window_poll(window, &event, error) || event.type != AB_WINDOW_CLOSE ||
        !ab_window_poll(window, &event, error) || event.type != AB_WINDOW_NONE) {
        ab_fail(error, "Native window close event is missing or repeated");
        goto release_pool;
    }
    okay = true;

release_pool:
    ab_apple_release(pool);
    return okay;
}

/* Explicit native presentation check; not part of unattended default tests. */
int main(int argc, char **argv) {
    if (argc > 2 || (argc == 2 && strcmp(argv[1], "--background") != 0)) {
        fprintf(stderr, "Usage: ab_window_check [--background]\n");
        return 2;
    }
    AbError error = {0};
    AbWindow *window = NULL;
    AbDrawList list = {0};
    int result = 1;
    AbWindowConfig config = {.title = "AngryBirdsC native presentation check",
                             .width = 96,
                             .height = 64,
                             .texture_capacity = 1,
                             .allocation_limit = 16384,
                             .visible = true,
                             .focus = argc == 1};
    if (!ab_window_open_metal(config, &window, &error) ||
        !ab_draw_list_init(1, 4096, &list, &error)) {
        goto release_window;
    }
    uint8_t pixels[] = {255, 0, 0,   255, 0,   255, 0,   255,
                        0,   0, 255, 255, 255, 255, 255, 255};
    AbImage image = {2, 2, {pixels, sizeof(pixels)}};
    AbSpriteRegion region = {.width = 2, .height = 2};
    AbSpritePlacement placement = {.scale_x = 48, .scale_y = 32};
    AbDrawCommand command = {.texture = 1, .color = {1, 1, 1, 1}, .opacity = 1};
    if (!ab_window_upload(window, 1, &image, false, false, &error) ||
        !ab_sprite_quad(&region, 2, 2, placement, &command.quad, &error) ||
        !ab_draw_list_push(&list, &command, &error)) {
        goto release_window;
    }
    const float clear[] = {0, 0, 0, 1};
    for (size_t frame = 0; frame < 8; ++frame) {
        AbWindowEvent event;
        for (size_t pending = 0; pending < 256; ++pending) {
            if (!ab_window_poll(window, &event, &error)) {
                goto release_window;
            }
            if (event.type == AB_WINDOW_NONE) {
                break;
            }
            if (event.type == AB_WINDOW_CLOSE) {
                result = 0;
                goto release_window;
            }
        }
        if (!ab_window_present(window, &list, clear, &error)) {
            goto release_window;
        }
    }
    if (!ab_window_finish(window, &error)) {
        goto release_window;
    }
    if (!check_events(window, &error)) {
        goto release_window;
    }
    puts("Native Metal window completed eight textured frames; native pointer "
         "down/drag/up and one-shot close mapping pass.");
    result = 0;

release_window:
    if (result) {
        fprintf(stderr, "%s\n", error.message);
    }
    ab_draw_list_free(&list);
    ab_window_close(window);
    return result;
}
