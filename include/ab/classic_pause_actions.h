#ifndef AB_CLASSIC_PAUSE_ACTIONS_H
#define AB_CLASSIC_PAUSE_ACTIONS_H

#include "ab/audio_bank.h"
#include "ab/event_manager.h"

typedef struct {
    AbDataTable table;
    AbDataEntry id;
} AbClassicPauseNotice;

typedef struct {
    AbAudioBank *audio;
    AbEventManager *events;
    const AbDataTable *event_ids;
    AbDataValue *restarted_from, *zoom_level, *sotred_zoom_level;
    AbClassicPauseNotice *notices;
    size_t notice_capacity;
} AbClassicPauseActionConfig;

typedef struct {
    AbClassicPauseActionConfig config;
    AbDataValue stored_zoom;
    size_t notice_count;
    bool scaling_enabled, initialized, busy, failed;
} AbClassicPauseActions;

/* Common-game showPauseMenu87590 and setScalingEnabled162201. Initialize the
 * latter's actual private captures (enabled=true, stored=nil) once at bootstrap;
 * initialization does not write any global slot, stop audio or send events.
 * Borrow stable canonical globals, real audio/event owners, IDs and retained
 * notice storage. Optional owners are required at the source boundary that
 * reaches them. Never provide a successful substitute for missing work.
 *
 * Show publishes levelRestartedFrom, stops the three named rolling resources in
 * order, then emits the distinct ABOUT_MENU_OPENED notice. Scaling false captures
 * the raw zoom only on the enabled-to-disabled transition. Reenable restores it
 * and clears the separate misspelled source global sotredZoomLevel, preserving
 * the private snapshot. Numeric words, nil and false copy without coercion.
 * Diagnostic print/loginfo observation is omitted. No allocation, VM or device
 * output. Drain event consumers before releasing this owner/storage. Failure
 * retains the completed source prefix and stops the owner; recursion rejects. */
bool ab_classic_pause_actions_init(AbClassicPauseActions *actions,
                                   AbClassicPauseActionConfig config, AbError *error);
/* Direct callback signatures for the actual PausePage host. */
bool ab_classic_pause_show(void *context, AbError *error);
bool ab_classic_pause_scaling(void *context, bool enabled, AbError *error);

#endif
