#ifndef AB_CAMERA_BOOTSTRAP_H
#define AB_CAMERA_BOOTSTRAP_H

#include "ab/camera_limits.h"
#include "ab/lua_data.h"

/* CameraSystem.findTrueSnapPoints interprets authored snaps in 1024x768 space,
 * recomputes their horizontal coverage from both rounded edges, then preserves
 * the authored bottom edge when fitting the actual logical pixel dimensions.
 * Do not cancel the edge subtraction or replace this with direct zoom scaling.
 * Used values/intermediates must be finite and divisions nonzero; no allocation.
 * Failure preserves output, which may alias authored. */
bool ab_camera_snap_fit(const AbCameraSnap *authored, AbVec2 pixel_size,
                        AbCameraSnap *output);

/* Museum setup selects birdCameraData.ipad and castleCameraData.ipad on the
 * pinned Android build, then applies the fit above in that order. Read px/py/sx
 * from the statically recovered level globals; no Lua, device-profile guessing,
 * limits, movement, sequence or native camera setter runs. Output is two caller-
 * owned snaps and remains unchanged on failure. Source tables are borrowed
 * during the call only. */
bool ab_camera_museum_snaps(const AbDataTable *level, AbVec2 pixel_size,
                            AbCameraSnap output[2], AbError *error);

#endif
