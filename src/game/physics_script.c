#include "ab/physics_script.h"

#include <string.h>

bool ab_physics_script_step(AbPhysicsScriptStep *step, float seconds, AbError *error) {
    if (!step) {
        return ab_fail(error, "Missing physics script step");
    }
    if (step->level_step && !step->level_step(step->context, seconds, error)) {
        return false;
    }
    AbDataValue argument = {.kind = AB_DATA_NUMBER};
    memcpy(&argument.bits, &seconds, sizeof(argument.bits));
    return ab_block_global_event(step->objects, step->count, step->event, &argument, 1,
                                 step->host, error);
}
