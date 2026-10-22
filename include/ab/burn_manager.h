#ifndef AB_BURN_MANAGER_H
#define AB_BURN_MANAGER_H

#include "ab/base.h"

typedef struct {
    float pending_seconds, deadline;
    bool pending, deadline_present;
} AbBurnManager;

/* BurnManager.lua's closed original methods and shared module state. Fresh zero
 * storage represents chunk construction. addBurnTime retains the largest new
 * positive duration until update sets deadline=time+duration, clears pending
 * and zeroes duration. Expire only when deadline < time. reset clears all three
 * mutable upvalues. update ignores seconds, including nonfinite values; time is
 * used only while pending or a deadline exists. Used numeric state must be finite.
 * Standard tostring's discarded string result has no side effect. Rebinding
 * original methods/builtins is outside this fixed binding. No allocation/RNG.
 * The separate init method's diagnostic belongs to the outer bootstrap.
 */
bool ab_burn_manager_add(AbBurnManager *manager, float seconds, AbError *error);
bool ab_burn_manager_update(AbBurnManager *manager, float seconds, float time,
                            AbError *error);
bool ab_burn_manager_reset(AbBurnManager *manager, AbError *error);

/* Original isBurningObjects reads a different upvalue initialized false by the
 * chunk. None of its methods writes that upvalue: the return stays false even
 * during a burn deadline. Context/receiver is unused, as in the original.
 * Direct concrete callback for the ordinary level-loss query. */
bool ab_burn_manager_is_burning(void *context, bool *output, AbError *error);

#endif
