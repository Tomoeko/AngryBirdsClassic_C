#ifndef AB_CLASSIC_GAME_UPDATE_H
#define AB_CLASSIC_GAME_UPDATE_H

#include "ab/classic_controller.h"
#include "ab/classic_game_input.h"
#include "ab/classic_game_level_particles.h"
#include "ab/classic_game_powerups.h"
#include "ab/classic_game_prelude.h"
#include "ab/classic_game_progress.h"
#include "ab/classic_game_start.h"

typedef struct {
    AbClassicGamePrelude *prelude;
    AbClassicGameStart *start;
    AbClassicGameProgress *progress;
    AbClassicGamePowerups *powerups;
    AbClassicGameLevelParticles *particles;
    AbClassicGameInput *input;
    AbClassicController *controller;
} AbClassicGameUpdateConfig;

typedef struct {
    AbClassicGameUpdateConfig config;
    bool initialized, updating, failed;
} AbClassicGameUpdate;

/* Compose the owned ordinary GameUpdate phases in source order, retaining each
 * component's supported domain and error policy. All components and their
 * canonical globals/registries remain borrowed. Cursor, cheat/touch/skip flags,
 * sling, shots and physics-enable references must be shared, not copied.
 * Initialization chooses no state, resources, hooks or bootstrap defaults.
 *
 * This does not supply the root wrapper, native physics/audio/particle schedule,
 * MenuManager, platform input, GUI or renderer. In particular the controller's
 * no-grenades/no-flying-Yellow-special-case tail domain still applies. A caller
 * must establish that actual selected domain; this is not an all-level fallback.
 */
bool ab_classic_game_update_initialize(AbClassicGameUpdateConfig config,
                                       AbClassicGameUpdate *output, AbError *error);
/* Run only at the recovered root updateGame boundary, with original unscaled
 * seconds and numeric time. Camera input is already resolved for the supported
 * controller domain; this call supplies the actual newly updated shake offset.
 * No clocks, physics steps, callbacks or failed phases are implicitly skipped.
 * No allocation. Reentry rejects; late failure stops this composed owner and
 * retains every preceding component's effects.
 */
bool ab_classic_game_update(AbClassicGameUpdate *game, AbClassicGameTouchInput touches,
                            float seconds, AbDataValue time, AbCameraSystemInput camera,
                            AbError *error);

#endif
