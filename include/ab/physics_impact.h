#ifndef AB_PHYSICS_IMPACT_H
#define AB_PHYSICS_IMPACT_H

#include "ab/physics_separation.h"

typedef enum {
    AB_IMPACT_UNKNOWN,
    AB_IMPACT_FAILED,
    AB_IMPACT_OVERLAPPED,
    AB_IMPACT_TOUCHING,
    AB_IMPACT_SEPARATED,
} AbImpactKind;

typedef struct {
    AbDistanceProxy first;
    AbDistanceProxy second;
    AbSweep first_sweep;
    AbSweep second_sweep;
    float maximum_fraction;
} AbImpactInput;

typedef struct {
    AbImpactKind kind;
    float fraction;
    uint32_t iterations;
    uint32_t root_iterations;
} AbImpactResult;

/* Bounded time-of-impact query over borrowed support vertices. Sweeps are
 * normalized in private copies. Distance/rotation numeric contracts apply;
 * maximum_fraction is in [0,1]. Input remains unchanged. Invalid arguments or
 * nonfinite/unsupported arithmetic preserve output and return false.
 * AB_IMPACT_FAILED is a valid native search outcome after bounded iteration;
 * it is distinct from an unsupported query. No allocation occurs.
 */
bool ab_time_of_impact(const AbImpactInput *input, AbImpactResult *output);

#endif
