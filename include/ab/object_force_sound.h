#ifndef AB_OBJECT_FORCE_SOUND_H
#define AB_OBJECT_FORCE_SOUND_H

#include "ab/base.h"
#include "ab/random.h"

typedef enum {
    AB_FORCE_SOUND_NONE,
    AB_FORCE_SOUND_NAMED,
    AB_FORCE_SOUND_ARRAY,
} AbObjectForceSoundKind;

typedef struct {
    AbObjectForceSoundKind kind;
    AbSpan name;
    const AbSpan *choices;
    size_t count;
    float gain;
    bool gain_present;
} AbObjectForceSound;

typedef struct {
    AbSpan name;
    float gain;
    bool play;
} AbObjectForceSoundPlan;

/* Force pulse audio selection after its sprite request, before timer publication
 * and impulse application. No pulse or NONE yields no request. NAMED consumes
 * no random draw; ARRAY uses one native lrand48 remainder by count, independently
 * of Lua math.random's float scaling. The supplied RNG is shared caller state.
 * Missing numeric gain defaults to one. Requests are nonlooping on
 * track zero, unit speed and all remaining native play parameters zero.
 * Values are already resolved from Lua string/number/table predicates; ordered
 * choices and string bytes stay borrowed. Empty arrays reject before a draw.
 * Used gains must be finite. Invalid initial input preserves output/RNG; an
 * invalid selected span retains its consumed draw. No resource lookup, mixing,
 * allocation or device playback executes here. */
bool ab_object_force_sound_plan(const AbObjectForceSound *sound, bool pulse,
                                AbRandom48 *random, AbObjectForceSoundPlan *output,
                                AbError *error);

#endif
