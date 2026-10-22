#ifndef AB_BIRD_RETIREMENT_H
#define AB_BIRD_RETIREMENT_H

#include <stdbool.h>

typedef struct {
    float ready_timer;
    float water_timer;
    bool ready_timer_present;
    bool water_timer_present;
} AbBirdRetirementState;

typedef struct {
    float seconds;
    float speed_squared;
    float speed_threshold;
    float reset_timer;
    bool controllable;
    bool mighty_bait;
    bool mighty_eagle;
    bool shot_literal_true;
    bool manually_removed;
    bool water_active;
    bool outside_boundaries;
    bool camera_targets_bird;
} AbBirdRetirementInput;

typedef struct {
    AbBirdRetirementState next;
    bool query_definition;
    bool query_speed_squared;
    bool query_camera_target;
    bool remove_bird;
} AbBirdRetirementFrame;

/* Ordinary museum bird-removal branch after specialty updates. Mighty birds,
 * unshot birds and noncontrollable objects are skipped. A manual-removal
 * definition skips the timer branch. Present ready timers count down only
 * below the strict squared-speed threshold; otherwise reset them. Water uses
 * its previous timer or ten seconds. Outside boundaries requests removal
 * directly. Negative timers request removal unless the camera targets this
 * same object. Flags encode
 * Lua truthiness except shot_literal_true. An absent water timer represents nil
 * or the false fallback during an active water update; false while inactive
 * is not a valid numeric original state and is outside this interface.
 * Definition, squared-speed and camera lookups are supplied boundary inputs;
 * the returned query flags describe when they are used. Specialty effects,
 * mutable lookup callbacks and destruction are separate. Used numeric values
 * and results must be finite. Failure preserves output. */
bool ab_bird_retirement_frame(const AbBirdRetirementState *state,
                              const AbBirdRetirementInput *input,
                              AbBirdRetirementFrame *output);

#endif
