#include "ab/burn_manager.h"

#include <math.h>

bool ab_burn_manager_add(AbBurnManager *manager, float seconds, AbError *error) {
    if (!manager || !isfinite(seconds) || !isfinite(manager->pending_seconds))
        return ab_fail(error, "Unsupported burn-time addition");
    if (manager->pending_seconds < seconds) {
        manager->pending_seconds = seconds;
        manager->pending = true;
    }
    return true;
}

bool ab_burn_manager_reset(AbBurnManager *manager, AbError *error) {
    if (!manager)
        return ab_fail(error, "Missing burn manager state");
    *manager = (AbBurnManager){0};
    return true;
}

bool ab_burn_manager_update(AbBurnManager *manager, float seconds, float time,
                            AbError *error) {
    (void)seconds;
    if (!manager)
        return ab_fail(error, "Missing burn manager update state");
    if (manager->pending) {
        float deadline = time + manager->pending_seconds;
        if (!isfinite(time) || !isfinite(manager->pending_seconds) ||
            !isfinite(deadline))
            return ab_fail(error, "Unsupported pending burn deadline");
        manager->deadline = deadline;
        manager->deadline_present = true;
        manager->pending = false;
        manager->pending_seconds = 0;
    }
    if (manager->deadline_present) {
        if (!isfinite(time) || !isfinite(manager->deadline))
            return ab_fail(error, "Unsupported active burn deadline");
        if (manager->deadline < time)
            return ab_burn_manager_reset(manager, error);
    }
    return true;
}

bool ab_burn_manager_is_burning(void *context, bool *output, AbError *error) {
    (void)context;
    if (!output)
        return ab_fail(error, "Missing original burn-status output");
    *output = false;
    return true;
}
