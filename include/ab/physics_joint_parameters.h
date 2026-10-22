#ifndef AB_PHYSICS_JOINT_PARAMETERS_H
#define AB_PHYSICS_JOINT_PARAMETERS_H

#include <stdbool.h>
#include <stdint.h>

enum {
    AB_JOINT_SET_MOTOR = 1u << 0,
    AB_JOINT_SET_SPEED = 1u << 1,
    AB_JOINT_SET_MAXIMUM = 1u << 2,
    AB_JOINT_SET_LIMIT = 1u << 3,
    AB_JOINT_SET_LOWER = 1u << 4,
    AB_JOINT_SET_UPPER = 1u << 5,
    AB_JOINT_SET_FREQUENCY = 1u << 6,
    AB_JOINT_SET_DAMPING = 1u << 7,
};

typedef struct {
    uint32_t present;
    bool motor;
    bool limit;
    float speed;
    float maximum;
    float lower;
    float upper;
    float frequency;
    float damping;
} AbJointParameters;

#endif
