#ifndef AB_CLASSIC_JOINTS_INTERNAL_H
#define AB_CLASSIC_JOINTS_INTERNAL_H

#include "ab/classic_joints.h"

bool ab_classic_joints_healthy(const AbClassicJoints *joints);
bool ab_classic_joint_name_matches(AbSpan first, AbSpan second);
AbClassicJointRecord *ab_classic_joints_find_metadata(const AbClassicJoints *joints,
                                                      AbSpan name);

#endif
