#ifndef AB_CLASSIC_GAME_RUNTIME_H
#define AB_CLASSIC_GAME_RUNTIME_H

#include "ab/classic_game_update.h"
#include "ab/classic_world_runtime.h"

typedef struct AbClassicGameRuntime AbClassicGameRuntime;

typedef struct {
    AbClassicPlayfield *playfield;
    AbClassicWorldRuntime *world;
    AbClassicParticles *particles;
    AbAudioBank *audio;
    AbSettingsStatistics *settings;
    AbEventManager *events;
    AbRandom48 *random;
    AbDataValue *flying;
    /* Borrow actual initialized source state, selected classes and remaining
     * service callbacks through these configs. Common owner pointers and shared
     * phase bindings are selected by this constructor. Their byte limits remain
     * caller inputs; caller-owned state is preserved at open. */
    AbClassicBirdConfig birds;
    AbClassicCharacterServiceConfig conditions;
    AbClassicGameEffectConfig effects;
    AbClassicGamePreludeConfig prelude;
    AbClassicGameStartConfig start;
    AbClassicLevelFinishConfig finish;
    AbClassicLevelEndingConfig ending;
    AbClassicGameProgressConfig progress;
    AbClassicGamePowerupConfig powerups;
    AbClassicGameLevelParticleConfig level_particles;
    AbClassicGameInputConfig input;
    AbClassicBirdSpecialtyConfig specialty;
    AbClassicControllerConfig controller;
    size_t characters_bytes, touches_bytes, touches, storage_bytes;
} AbClassicGameRuntimeConfig;

typedef struct {
    AbClassicBirds *birds;
    AbClassicCharacters *characters;
    AbClassicCharacterServices *conditions;
    AbClassicBirdSpecialty *specialty;
    AbClassicController *controller;
    AbClassicGameInput *input;
    AbClassicGamePowerups *powerups;
    AbClassicGameEffects *effects;
    AbClassicGameUpdate *update;
    AbClassicLevelFinish *finish;
    AbClassicLevelEnding *ending;
    AbClassicGameProgress *progress;
} AbClassicGameRuntimeView;

/* Construct the ordinary GameUpdate pipeline over one already prepared fresh
 * playfield/world. Share canonical flying, input, completion, physics and
 * activity fields across every phase. Bind real bird retirement, character
 * services, win/loss/ending, specialties, touches and the controller; retain
 * caller-provided source state and reached external services. Borrow actual
 * GUI notifications, menu input, update/powerup/effect registries and draw service.
 *
 * Components retain their supported domains and failure policies. Construction
 * chooses no source defaults and performs no frame tick, resource load, GUI
 * setup or original level-load event sequence. The application must complete
 * its draw/root bindings before invoking the exposed update. Drain borrowing
 * events before close; close this owner before world/playfield/resources. A
 * failed source application requires discarding the borrowing session. */
bool ab_classic_game_runtime_open(AbClassicGameRuntimeConfig config,
                                  AbClassicGameRuntime **output, AbError *error);
void ab_classic_game_runtime_close(AbClassicGameRuntime *runtime);
AbClassicGameRuntimeView ab_classic_game_runtime_view(AbClassicGameRuntime *runtime);

#endif
