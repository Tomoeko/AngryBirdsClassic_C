#ifndef AB_CLASSIC_START_LEVEL_H
#define AB_CLASSIC_START_LEVEL_H

#include "ab/camera_shake.h"
#include "ab/camera_touch.h"
#include "ab/classic_camera.h"
#include "ab/classic_loader.h"

typedef struct AbClassicStartLevel AbClassicStartLevel;

typedef struct {
    AbClassicCamera *camera;
    AbCameraTouchHandler *input;
    AbCameraTouchShared *shared_input;
    AbCameraShake *shake;
    AbEventManager *events;
    const AbDataTable *event_ids, *level_scripts, *promo_campaigns;
    const AbDataValue *touch_gestures, *started_from_editor, *level_name;
    size_t starts, storage_bytes;
} AbClassicStartLevelConfig;

/* commongamelogic.startLevel24424 and CameraSystem.onStartLevel6389 for the
 * target's default TouchCameraInputHandler. Supply real camera/sequence sources,
 * shared handler/shake/global fields, events and actual promo campaigns table.
 * The APK's PromoCampaignSubsystem initializes it from an empty private class
 * registry; that selected empty domain has no onStartLevel work. Nonempty
 * campaign classes reject when reached. The ordinary
 * level-script table is actual bootstrap state, including an explicitly empty
 * table when no hooks are registered. Truthy onStartLevel hooks are owned C
 * AbClassicLoaderCall records stored as AB_DATA_OBJECT, never script bytecode.
 * Reached PC/custom input handlers and foreign callable values reject.
 *
 * Clear the privately owned captured flag first; construct input, reset both movement
 * curves, remove explosion shake, notify cameras initialized, start the selected
 * default/restart sequence, notify loading done and conditionally goto game,
 * then visit promo campaigns and the freshly looked-up level hook. Keep events and
 * their records alive until all observers finish. Fixed storage allocates only at open.
 * Calls serialize; late failure retains effects and stops this owner. This does not
 * construct the loaded world, GameHud, promo subsystem or root.
 */
bool ab_classic_start_level_open(AbClassicStartLevelConfig config,
                                 AbClassicStartLevel **output, AbError *error);
void ab_classic_start_level_close(AbClassicStartLevel *start);
bool ab_classic_start_level(AbClassicStartLevel *start, AbDataValue restart,
                            AbError *error);
/* Direct completion_call binding for prepareLevel's argument-free Loader call. */
bool ab_classic_start_level_call(void *context, AbError *error);

#endif
