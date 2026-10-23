#ifndef AB_CLASSIC_CHARACTER_SERVICES_H
#define AB_CLASSIC_CHARACTER_SERVICES_H

#include "ab/classic_birds.h"
#include "ab/classic_characters.h"
#include "ab/classic_sling.h"
#include "ab/classic_goals.h"

typedef struct {
    AbClassicLevel *level;
    AbClassicCamera *camera;
    AbClassicSling *sling;
    AbClassicBirds *birds;
    AbAudioBank *audio;
    const AbDataValue *prevent_ending, *level_timeout, *moving_objects;
    const AbDataValue *do_not_wait_for_moving_objects;
    const AbDataValue *bird_sound, *pig_sound;
    void *context;
    bool (*update_hat)(void *context, size_t object, AbError *error);
    /* Used by the separate ordinary win query, when its source gates reach
     * them. Existing character/loss calls do not require unused win bindings. */
    AbClassicGoals *goals;
    const AbDataValue *do_not_timeout, *eagle_launched, *eagle_timer;
} AbClassicCharacterServiceConfig;

typedef struct {
    AbClassicCharacterServiceConfig config;
    AbClassicLevelView level;
    bool initialized;
} AbClassicCharacterServices;

/* Caller-owned stable services for the concrete character controller. Borrow all
 * source owners/shared globals until the character owner closes. Initialize has
 * no effects/RNG/allocation. Globals are live typed values; doNotWait uses the
 * original literal-true comparison while the other gates use truthiness.
 *
 * Museum checkLevelFailed prototype 150733 visits the actual birds table after
 * prevent/timeout/motion gates. A remaining controllable bird returns false
 * before BurnManager. The actual original isBurningObjects method is bound when
 * reached: its distinct unchanged upvalue returns false. Canonical dynamic
 * birds participate alongside the initial queue; foreign physical identities
 * and mismatched name/table pairs reject.
 *
 * Character distance uses the current camera screen position and x/y*20 for a
 * goal, or the actual sling level-start position*20 for a bird. Preserve separate
 * float arithmetic and promoted sqrt (utils vLength prototype 9192). Sound runs
 * typed getAudioName then actual playAudio(gain,false,0); caller supplies the live
 * g_sound_birds/g_sound_pigs values selected by the loader. Ordinary Museum
 * selects bird_misc/piglette at loadLevel 2711 instructions 161..164.
 * Hats require their actual callback only when the existing controller reaches
 * them. No hidden platform output or invented loss/audio/hat result.
 * Calls are serialized; callbacks cannot replace/free this binding or reenter.
 */
bool ab_classic_character_services_initialize(AbClassicCharacterServiceConfig config,
                                              AbClassicCharacterServices *output,
                                              AbError *error);
AbClassicCharacterHost
ab_classic_character_services_host(AbClassicCharacterServices *services);

/* Concrete ordinary checkLevelComplete prototype149936 and checkLevelFailed
 * prototype150733, reusable by game progress/ending as well as character faces.
 * Win checks prevention/eagle literal true first, then actual goal notification,
 * then reads current timeout/motion/wait flags after that callback. No cached
 * snapshots across events, invented win/lose result or implicit clock advance.
 * Reached missing services/unsupported numeric values fail with partial event
 * effects retained. A used eagle timer must be finite; nil blocks completion.
 */
bool ab_classic_character_services_win(AbClassicCharacterServices *services,
                                       bool *output, AbError *error);
bool ab_classic_character_services_lose(AbClassicCharacterServices *services,
                                        bool *output, AbError *error);

#endif
