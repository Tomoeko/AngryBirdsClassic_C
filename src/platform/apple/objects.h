#ifndef AB_APPLE_OBJECTS_H
#define AB_APPLE_OBJECTS_H

#include "ab/base.h"

#include <objc/message.h>
#include <objc/runtime.h>

static inline id ab_apple_new(const char *class_name) {
    id object = ((id(*)(id, SEL))objc_msgSend)((id)objc_getClass(class_name),
                                               sel_registerName("alloc"));
    return ((id(*)(id, SEL))objc_msgSend)(object, sel_registerName("init"));
}

static inline void ab_apple_release(id object) {
    if (object) {
        ((void (*)(id, SEL))objc_msgSend)(object, sel_registerName("release"));
    }
}

static inline id ab_apple_retain(id object) {
    return ((id(*)(id, SEL))objc_msgSend)(object, sel_registerName("retain"));
}

static inline id ab_apple_string(const char *text) {
    id object = ((id(*)(id, SEL))objc_msgSend)((id)objc_getClass("NSString"),
                                               sel_registerName("alloc"));
    return ((id(*)(id, SEL, const char *))objc_msgSend)(
        object, sel_registerName("initWithUTF8String:"), text);
}

static inline bool ab_apple_error(AbError *error, const char *context,
                                  id native_error) {
    id description = ((id(*)(id, SEL))objc_msgSend)(
        native_error, sel_registerName("localizedDescription"));
    const char *text = ((const char *(*)(id, SEL))objc_msgSend)(
        description, sel_registerName("UTF8String"));
    return ab_fail(error, "%s: %s", context, text ? text : "native operation failed");
}

#endif
