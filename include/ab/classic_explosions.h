#ifndef AB_CLASSIC_EXPLOSIONS_H
#define AB_CLASSIC_EXPLOSIONS_H

#include "ab/classic_particles.h"
#include "ab/classic_visuals.h"
#include "ab/classic_game_globals.h"
#include "ab/audio_bank.h"
#include "ab/camera_shake.h"

typedef struct {
    AbClassicLevel *level;
    AbClassicVisuals *visuals;
    AbClassicParticles *particles;
    AbAudioBank *audio;
    AbClassicGameGlobals *globals;
    AbCameraShake *shake;
    AbStringMap *dead_blocks;
    const AbDataValue *physical_values;
    size_t physical_count;
    AbStringOrderWorkspace *workspace;
    float physics_scale;
    /* The application must resolve the actual LevelEffect class. This owner
     * implements LevelEffect.explosion464, which returns sound unchanged. */
    bool base_effect;
    void *context;
    AbClassicJointBreakRequest break_joint;
    bool (*destroy_connected)(void *context, AbSpan object, AbError *error);
} AbClassicExplosions;

/* gamelogic.makeExplosion152242 followed by MuseumMode.createExplosion19029
 * and ObjectFunctions.museumCreateExplosion166. Emit the two actual legacy
 * particle effects before the selected level effect; then audio, shake, source
 * world iteration, native joint/impulse operations, pig popping and damage.
 * Source scale affects shake only. Preserve the empty addExplosionScore152497
 * method and inflictDamage151911's writes to the shared score scratch field.
 * Borrow canonical objects, actual preloaded effects and stable dead-map value
 * rows. No allocation or physics step. Missing reached services reject; earlier
 * effects survive failure and the caller must discard the session. */
bool ab_classic_explosions_make(AbClassicExplosions *owner, size_t object,
                                AbDataValue definition, AbDataValue sound,
                                AbDataValue scale, AbError *error);

#endif
