#ifndef AB_WINDOW_H
#define AB_WINDOW_H

#include "ab/draw.h"
#include "ab/image.h"

typedef struct AbWindow AbWindow;

typedef struct {
    const char *title;
    uint32_t width;
    uint32_t height;
    size_t texture_capacity;
    size_t allocation_limit;
    /* A background window can be tested without taking keyboard focus. */
    bool visible;
    bool focus;
} AbWindowConfig;

typedef enum {
    AB_WINDOW_NONE,
    AB_WINDOW_CLOSE,
    AB_WINDOW_POINTER_MOVE,
    AB_WINDOW_POINTER_DOWN,
    AB_WINDOW_POINTER_UP,
    AB_WINDOW_KEY_DOWN,
    AB_WINDOW_KEY_UP,
} AbWindowEventType;

typedef struct {
    AbWindowEventType type;
    double time;
    float x;
    float y;
    uint16_t key;
    bool repeat;
} AbWindowEvent;

/* Native Apple Metal window boundary. All calls require the main thread.
 * Content coordinates and drawable pixels have the requested fixed dimensions;
 * input uses top-left coordinates. Aspect/resize/fullscreen policy is separate.
 * The window owns its renderer and native presentation objects. No audio starts.
 * Calls return errors instead of terminating the host application. */
bool ab_window_open_metal(AbWindowConfig config, AbWindow **output, AbError *error);
bool ab_window_upload(AbWindow *window, uint32_t texture, const AbImage *image,
                      bool repeat_s, bool repeat_t, AbError *error);
/* Drain native events until a recognized event or NONE, also dispatching them
 * to AppKit. Closing emits CLOSE once. Polling never blocks for new input. */
bool ab_window_poll(AbWindow *window, AbWindowEvent *output, AbError *error);
/* Clear channels are linear RGBA values in [0,1]. Submits ordered commands
 * asynchronously. Close waits for the final command before releasing objects. */
bool ab_window_present(AbWindow *window, const AbDrawList *list, const float clear[4],
                       AbError *error);
/* Explicit shutdown/capture boundary; ordinary frame submission does not wait. */
bool ab_window_finish(AbWindow *window, AbError *error);
void ab_window_close(AbWindow *window);

#endif
