#ifndef AB_PHYSICS_ISLAND_INTERNAL_H
#define AB_PHYSICS_ISLAND_INTERNAL_H

#include "ab/physics_island.h"

bool ab_island_contacts_valid(const AbIslandContact *contacts, size_t contact_count,
                              size_t body_count, const AbIslandWorkspace *workspace);
AbPositionConstraint ab_island_position_constraint(const AbBodyMotion *bodies,
                                                   const AbIslandContact *input);
bool ab_island_initialize_contact(const AbBodyMotion *bodies,
                                  const AbIslandContact *input,
                                  const AbIslandStep *step,
                                  AbIslandWorkspace *workspace, size_t index);
void ab_island_report_contacts(const AbIslandStep *step,
                               const AbVelocityContact *contacts, size_t count);

bool ab_island_joints_valid(const AbIslandJoint *joints, size_t joint_count,
                            size_t body_count, const AbIslandWorkspace *workspace);
bool ab_island_initialize_joints(const AbBodyMotion *bodies,
                                 const AbIslandJoint *joints, size_t joint_count,
                                 const AbIslandStep *step,
                                 AbIslandWorkspace *workspace);
bool ab_island_solve_joint_velocities(const AbIslandJoint *joints, size_t joint_count,
                                      float seconds, AbIslandWorkspace *workspace);
AbPositionResult ab_island_solve_joint_positions(const AbIslandJoint *joints,
                                                 size_t joint_count,
                                                 AbIslandWorkspace *workspace);

#endif
