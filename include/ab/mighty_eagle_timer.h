#ifndef AB_MIGHTY_EAGLE_TIMER_H
#define AB_MIGHTY_EAGLE_TIMER_H

#include "ab/lua_data.h"
#include "ab/physics_geometry.h"

typedef struct {
    AbDataValue timer, moving, bait_x, bait_y, sound_played;
    AbDataValue camera_shake, destroy_joints;
} AbMightyEagleTimer;

typedef enum {
    AB_MIGHTY_EAGLE_CREATE,
    AB_MIGHTY_EAGLE_YELL,
    AB_MIGHTY_EAGLE_FLY,
    AB_MIGHTY_EAGLE_REMOVE_GOALS,
    AB_MIGHTY_EAGLE_DESTROY_JOINTS,
} AbMightyEagleTimerOperation;

typedef struct {
    AbMightyEagleTimerOperation operation;
    AbVec2 position;
} AbMightyEagleTimerCall;

typedef bool (*AbMightyEagleTimerHost)(void *context, AbMightyEagleTimerCall call,
                                       AbError *error);

/* mightyEagle.lua prototype 6277: decrement the live timer, summon from a saved
 * exited-bait position, play the two original audio cues, decay camera shake,
 * request remaining-goal cleanup and then joint destruction. The concrete world
 * loops and resource/body creation belong to the host; this boundary invents no
 * effects, clocks, default flags or cooldown policy. Callbacks may update the
 * same state; later source reads see those writes. Used numeric values/results
 * must be finite; nil timers skip even unused invalid seconds/missing services.
 * No allocation. A late failure retains preceding assignments/calls. */
bool ab_mighty_eagle_timer_update(AbMightyEagleTimer *state,
                                  AbDataValue *specialty_available, float seconds,
                                  AbMightyEagleTimerHost host, void *context,
                                  AbError *error);

#endif
