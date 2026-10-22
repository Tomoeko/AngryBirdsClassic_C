#ifndef AB_BIRD_REMOVE_H
#define AB_BIRD_REMOVE_H

#include "ab/audio_play.h"

typedef struct {
    AbDataValue objects, birds, other_birds, hats;
    AbDataValue camera, level_effect;
    AbDataValue current_bird_name, flying_bird, specialty_available;
} AbBirdRemoveState;

typedef struct {
    void *context;
    /* Resolve/call current methods on the supplied receiver. destroy receives
     * only the script; particles receives the script and fixed key particles.
     * Missing/non-callable methods must fail at their call boundary. */
    bool (*destroy)(void *context, AbDataValue script, AbError *error);
    bool (*definition)(void *context, AbDataValue name, AbDataValue *output,
                       AbError *error);
    bool (*hook)(void *context, AbDataValue current_key, AbDataValue object,
                 AbError *error);
    bool (*particles)(void *context, AbDataValue script, AbDataValue *output,
                      AbError *error);
    bool (*emit)(void *context, AbDataValue captured_name, AbDataValue descriptor,
                 bool first, bool second, AbError *error);
    bool (*camera)(void *context, AbDataValue receiver, AbDataValue object,
                   AbError *error);
    bool (*volume)(void *context, AbDataValue receiver, AbDataValue object,
                   AbDataValue *output, AbError *error);
    bool (*audio_name)(void *context, AbDataValue sound, AbDataValue *output,
                       AbError *error);
    bool (*audio)(void *context, AbAudioPlay request, AbError *error);
    bool (*remove)(void *context, AbDataValue captured_name, AbError *error);
} AbBirdRemoveHost;

/* Complete gamelogic.removeBird, prototype 148346, with stable plain tables and
 * string object/hat names. Nil play_sound defaults true; other values use normal
 * truthiness. The original third parameter has no observable use here.
 *
 * Optional script destruction precedes fresh definition/named-hook dispatch.
 * Zero a present hat's defence/strength, cache the current bird name, optionally
 * emit script removal particles, then notify the current camera. Clear matching
 * current/flying bird bindings and specialty availability, choose bubble/default
 * sound, optionally query effect volume/play audio, remove the physical object,
 * then clear current world/dynamic/birds/otherBirds tables in that order.
 * Cache the audio function/context before resolving its name. Callback changes
 * to later data/bindings stay live; all script returns are ignored except the
 * explicit definition/particle/volume/audio-name results. No score is awarded.
 *
 * Hosts implement concrete effects and actual removal; this function owns no
 * registry, renderer, audio, script VM or memory. Preserve the cached bird/name
 * even after removal. Each mutable table needs capacity for nil writes. No
 * metatables, coercion, allocation, mixed-key iteration or reentry. Environment
 * lookup uses ordinary pure getfenv; replacing that binding is outside this
 * interface. Missing
 * required services fail when reached; earlier mutations survive failure.
 */
bool ab_bird_remove(AbDataValue object, AbDataValue play_sound,
                    AbBirdRemoveState *state, AbBirdRemoveHost *host, AbError *error);

#endif
