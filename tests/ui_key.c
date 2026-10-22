#include "ab/ui_key.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Frame key dispatch failed at %d\n", __LINE__);            \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    AbUiPointerReply reply;
    unsigned calls;
    bool fail;
} Child;

static AbSpan text(const char *value) {
    return (AbSpan){(const uint8_t *)value, strlen(value)};
}

static bool callback(void *context, AbSpan event, AbSpan key, AbUiPointerReply *reply,
                     AbError *error) {
    (void)error;
    Child *child = context;
    CHECK(event.size == 7 && memcmp(event.data, "RELEASE", 7) == 0 && key.size == 4 &&
          memcmp(key.data, "BACK", 4) == 0);
    ++child->calls;
    *reply = child->reply;
    return !child->fail;
}

static bool exercise(void) {
    Child first = {.reply.values = {{.kind = AB_UI_POINTER_NUMBER}}};
    Child last = {.reply.values = {{.kind = AB_UI_POINTER_BOOLEAN},
                                   {.kind = AB_UI_POINTER_OBJECT, .object = 17},
                                   {.kind = AB_UI_POINTER_FUNCTION, .function = 19}}};
    AbUiKeyChild routes[] = {{callback, &first, true, true},
                             {callback, &last, true, true}};
    AbUiPointerReply reply;
    CHECK(ab_ui_frame_key(routes, 2, text("RELEASE"), text("BACK"), &reply, NULL) &&
          !first.calls && last.calls == 1 &&
          reply.values[0].kind == AB_UI_POINTER_BOOLEAN && !reply.values[0].boolean &&
          reply.values[1].object == 17 && reply.values[2].function == 19);
    last.reply.values[0] = (AbUiPointerValue){0};
    CHECK(ab_ui_frame_key(routes, 2, text("RELEASE"), text("BACK"), &reply, NULL) &&
          first.calls == 1 && last.calls == 2 &&
          reply.values[0].kind == AB_UI_POINTER_NUMBER && !reply.values[0].number_bits);
    routes[1].visible = false;
    routes[1].callback = NULL;
    routes[0].active = false;
    CHECK(ab_ui_frame_key(routes, 2, text("RELEASE"), text("BACK"), &reply, NULL) &&
          first.calls == 1 && last.calls == 2 &&
          reply.values[0].kind == AB_UI_POINTER_NONE);
    routes[0].active = true;
    first.fail = true;
    reply.values[0] = (AbUiPointerValue){.kind = AB_UI_POINTER_OBJECT, .object = 91};
    CHECK(!ab_ui_frame_key(routes, 2, text("RELEASE"), text("BACK"), &reply, NULL) &&
          reply.values[0].kind == AB_UI_POINTER_OBJECT &&
          reply.values[0].object == 91 && first.calls == 2);
    CHECK(!ab_ui_frame_key(NULL, 1, text("RELEASE"), text("BACK"), &reply, NULL) &&
          !ab_ui_frame_key(NULL, 0, text("RELEASE"), (AbSpan){NULL, 1}, &reply, NULL));
    return true;
}

int main(void) {
    return exercise() ? 0 : 1;
}
