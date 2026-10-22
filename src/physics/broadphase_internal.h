#ifndef AB_PHYSICS_BROADPHASE_INTERNAL_H
#define AB_PHYSICS_BROADPHASE_INTERNAL_H

#include "ab/physics_broadphase.h"

bool ab_broadphase_valid(const AbBroadPhase *phase);
bool ab_broadphase_reserve_pairs(AbBroadPhase *phase, AbError *error);

#endif
