#ifndef AB_CLASSIC_EGGS_H
#define AB_CLASSIC_EGGS_H

#include "ab/bird_collision_projectiles.h"
#include "ab/classic_birds.h"
#include "ab/classic_explosions.h"
#include "ab/classic_playfield.h"

typedef struct AbClassicEggs AbClassicEggs;

typedef struct {
    AbClassicPlayfieldView playfield;
    AbClassicBirds *birds;
    AbClassicObjectRemoval *physical;
    AbClassicParticles *particles;
    AbClassicExplosions *explosions;
    AbAudioBank *audio;
    AbSettingsStatistics *settings;
    const AbSpriteProvider *sprites;
    AbBirdCollisionGrenades *grenades;
    float default_force;
    size_t storage_bytes;
    void *context;
    /* Register world/controller/character adapters after the actual object,
     * script, shot/camera and native physical bindings have been created. */
    bool (*created)(void *context, size_t object, AbError *error);
} AbClassicEggConfig;

/* triggerSpecialty125163's GRENADE branch and GameUpdate110400's backward
 * flyingGrenades explosion loop. Borrow the actual playfield and source list;
 * own stable projectile records and parent-name-plus-a bytes until close.
 * Reserve one egg per initial queued bird. The source timer=5 is retained, not
 * interpreted as an invented fuse: this loop checks only literal explode=true.
 * Creation does not add the projectile to the Lua birds/launch queue. Physical
 * registration does put it in the real controllable-body vector.
 * Serialized calls allocate no frame storage beyond reserved world growth.
 * Failure retains preceding source effects and stops the owner. */
bool ab_classic_eggs_open(AbClassicEggConfig config, AbClassicEggs **output,
                          AbError *error);
void ab_classic_eggs_close(AbClassicEggs *eggs);
size_t ab_classic_eggs_bytes(const AbClassicEggs *eggs);
bool ab_classic_eggs_trigger(AbClassicEggs *eggs, size_t parent,
                             const AbDataTable *definition, AbError *error);
bool ab_classic_eggs_update(AbClassicEggs *eggs, AbError *error);

#endif
