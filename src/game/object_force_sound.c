#include "ab/object_force_sound.h"

#include <limits.h>
#include <math.h>

bool ab_object_force_sound_plan(const AbObjectForceSound *sound, bool pulse,
                                AbRandom48 *random, AbObjectForceSoundPlan *output,
                                AbError *error) {
    if (!output || (pulse && !sound)) {
        return ab_fail(error, "Missing object force sound state or output");
    }
    AbObjectForceSoundPlan plan = {0};
    if (!pulse || sound->kind == AB_FORCE_SOUND_NONE) {
        *output = plan;
        return true;
    }
    if (sound->kind != AB_FORCE_SOUND_NAMED && sound->kind != AB_FORCE_SOUND_ARRAY) {
        return ab_fail(error, "Invalid object force sound kind");
    }
    if (sound->gain_present && !isfinite(sound->gain)) {
        return ab_fail(error, "Invalid object force sound gain");
    }
    AbSpan name = sound->name;
    if (sound->kind == AB_FORCE_SOUND_ARRAY) {
        if (!sound->choices || !sound->count || sound->count > INT32_MAX) {
            return ab_fail(error, "Invalid object force sound choices");
        }
        uint32_t draw;
        if (!ab_random48_next(random, &draw)) {
            return ab_fail(error, "Invalid object force sound random state");
        }
        name = sound->choices[draw % sound->count];
    }
    if (name.size && !name.data) {
        return ab_fail(error, "Invalid object force sound name");
    }
    plan.name = name;
    plan.gain = sound->gain_present ? sound->gain : 1;
    plan.play = true;
    *output = plan;
    return true;
}
