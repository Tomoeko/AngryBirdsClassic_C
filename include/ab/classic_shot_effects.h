#ifndef AB_CLASSIC_SHOT_EFFECTS_H
#define AB_CLASSIC_SHOT_EFFECTS_H

#include "ab/classic_level.h"
#include "ab/powerup_uses.h"

/* Ordinary attachParticleSpawnersToShotBird path. Read the live shared kingsling
 * use state and raw usedPowerPotion only when powerups_enabled is truthy. Publish
 * both flags directly to the canonical physical object, independently of a
 * rebound script.physicsObject. Preserve falsy->Boolean-false potion behavior;
 * numeric zero and strings remain truthy. No spawner is emitted when both flags
 * are falsy, as in the original. Powered trail creation/particle-definition
 * replacement is still unsupported and rejects after the two preceding writes.
 * No allocation or assumed empty powerup state; an enabled path requires uses.
 * Earlier writes survive any later failure. */
bool ab_classic_shot_attach_particles(AbClassicLevel *level, size_t object,
                                      const AbPowerupUses *uses, bool powerups_enabled,
                                      AbError *error);

/* redBirdShot's inactive/previous-collision gates. A falsy feature flag returns
 * without touching the argument. An enabled call reads the live raw collided
 * field; a truthy value returns. Active red-special setup still rejects before
 * its unimplemented first update-hook operation. No replacement ability. */
bool ab_classic_red_bird_shot(AbClassicLevel *level, size_t object,
                              bool red_special_available, AbError *error);

/* flyingRedBirdCollided prototype 9526 returns before its argument when the
 * feature is falsy. Active red-special collision/draw cleanup remains open and
 * rejects explicitly. No collision flags or specialty state change on skip. */
bool ab_classic_red_bird_collided(bool red_special_available, AbError *error);

/* redBirdRemoved prototype 13015 returns before reading its argument when the
 * feature is falsy. Active target/overlay/tutorial cleanup remains unsupported. */
bool ab_classic_red_bird_removed(bool red_special_available, AbError *error);

#endif
