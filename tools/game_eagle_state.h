#ifndef AB_GAME_EAGLE_STATE_H
#define AB_GAME_EAGLE_STATE_H

#include "ab/ui_mighty_eagle_button.h"
#include "ab/ui_eagle_services.h"
#include "ab/mighty_eagle_timer.h"
#include "ab/classic_object_hooks.h"

typedef struct {
    AbUiTextState lines[32];
    char names[32][40];
    AbFontWrappedLine wrapped[8];
    AbSpan text[8];
    uint32_t codes[256];
    uint8_t bytes[2048];
} EagleTextClip;

struct PreviewEagle {
    PreviewGame *game;
    AbUiMightyEagleButton button;
    AbUiEagleServices services;
    AbDataTable globals;
    AbDataEntry fields[16];
    AbUiFontGetter font;
    const AbUiFontGetter *previous_getters;
    size_t previous_getter_count;
    EagleTextClip timer_clip;
    AbObjectUpdates updates[2];
    AbObjectUpdateFunction functions[2];
    AbDataEntry launch_fields[2];
    AbDataTable launch_event;
    AbDataTable result_events[3];
    AbDataEntry result_fields[3][4];
    float percent, previous_percent;
    size_t bait, bird;
    float height;
    bool entered;
};

#endif
