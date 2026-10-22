#ifndef AB_UI_CUTSCENE_H
#define AB_UI_CUTSCENE_H

#include "ab/audio_bank.h"
#include "ab/event_manager.h"
#include "ab/settings_statistics.h"
#include "ab/ui_scaling.h"
#include "ab/ui_widget.h"

typedef struct {
    float reference_scale;
} AbUiCutsceneClass;

typedef struct {
    AbUiScaling *scaling;
    AbUiCutsceneClass *shared;
    AbAudioBank *audio;
    AbAudioMixer *mixer;
    AbSettingsStatistics *settings;
    AbEventManager *events;
    AbDataTable *end_event;
    const AbDataTable *commands;
    AbSpan name, device;
    AbVec2 margin;
    uint32_t white_texture;
    void *context;
    bool (*save_settings)(void *context, AbError *error);
} AbUiCutsceneHost;

typedef struct {
    AbUiWidget widget, skip;
    AbUiFrameNode *frames[1];
    AbUiDrawChild draw[1];
    AbUiPointerChild pointer[1];
    AbUiCutsceneHost host;
    AbSpan sprite_name, image, sounds[8];
    AbVec2 position, multiplier, reference, offset, scroll;
    AbVec2 scroll_begin, scroll_end;
    float scale, elapsed, scroll_begin_time, scroll_end_time;
    float color[3];
    size_t command_index, sound_count;
    bool sprite_present, fit_height, no_scroll, scrolling, scroll_cubic, ldown;
    bool trigger_skip, skip_allowed;
} AbUiCutscene;

/* Source CutScene over the single-sprite, non-film height-fit domain used by
 * pack1_intro: literal command records, direct resource audio, source scroll
 * arithmetic, actual settings mutation, skip and retained completion event.
 * Other commands/sprite transforms reject when reached. This is not a complete
 * cutscene catalog, bootstrap, native pixel/clock comparison or save transport.
 *
 * Borrow prepared assets, commands, stable class/settings/services and event
 * storage for this owner's lifetime. Own no allocation. Settings keys and
 * definition spans remain alive through settings close. The natural end saves
 * through its required real host; skip marks watched without the source save.
 * The host applies deferred scene changes after update/pointer traversal. */
bool ab_ui_cutscene_init(AbUiCutscene *owner, AbUiWidgetContext *context,
                         AbUiCutsceneHost host, AbError *error);
bool ab_ui_cutscene_skip(AbUiCutscene *owner, AbError *error);

#endif
