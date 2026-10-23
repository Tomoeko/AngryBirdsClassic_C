#ifndef AB_CLASSIC_OBJECT_HOOKS_H
#define AB_CLASSIC_OBJECT_HOOKS_H

#include "ab/classic_controller.h"

typedef struct AbClassicObjectHooks AbClassicObjectHooks;

/* Shared addObjectUpdateFunction name registry for components and dynamically
 * constructed birds. Borrow stable per-object AbObjectUpdates and physical
 * receivers. Preserve the native string-map layout and resume after the current
 * retained key when a callback inserts/deletes/rehashes. Missing world objects
 * prune only the registry, leaving their physical updateFunction value intact.
 * No allocation after open; all names remain borrowed until close. */
bool ab_classic_object_hooks_open(AbClassicLevel *level, size_t storage_bytes,
                                  AbClassicObjectHooks **output, AbError *error);
void ab_classic_object_hooks_close(AbClassicObjectHooks *hooks);
size_t ab_classic_object_hooks_bytes(const AbClassicObjectHooks *hooks);
bool ab_classic_object_hooks_register(AbClassicObjectHooks *hooks, size_t object,
                                      AbObjectUpdates *updates, AbError *error);
bool ab_classic_object_hooks_bind(AbClassicObjectHooks *hooks,
                                  AbClassicController *controller,
                                  AbGameTailHooks **output, AbError *error);

#endif
