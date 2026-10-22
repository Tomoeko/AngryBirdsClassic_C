#ifndef AB_SLINGSHOT_BODY_H
#define AB_SLINGSHOT_BODY_H

#include "ab/object_body.h"
#include "ab/slingshot.h"

/* Apply one active transfer's measured position/rotation/type sequence to an
 * existing body. input.rotation is the caller's current game-object angle;
 * position changes preserve the body's current angle until each rotation call.
 * Rotation calls use the recovered game angle normalization. On completion,
 * snap to target minus 0.1, switch to dynamic and set zero rotation in order.
 * Caller owns queue/time/script/render state and consumes frame.next_time.
 * World must be unlocked/healthy and object/body/fixture live. Invalid numeric
 * input rejects before mutation. A later world failure can leave partial work;
 * frame changes only after success. No extra workspace is allocated; the world's
 * bounded broadphase buffer growth rules still apply.
 */
bool ab_slingshot_transfer_body(AbPhysicsWorld *world, const AbObjectBody *object,
                                const AbSlingTransferInput *input,
                                AbSlingTransferFrame *frame, AbError *error);

/* Held ready bird: set band position, zero linear velocity, set zero rotation,
 * then zero angular velocity. Readiness/name and hat behavior are caller state.
 * The same world validity, finite-input and partial-failure rules apply.
 */
bool ab_slingshot_hold_body(AbPhysicsWorld *world, const AbObjectBody *object,
                            AbVec2 rubber_band, AbError *error);

#endif
