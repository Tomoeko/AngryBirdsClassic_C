#ifndef AB_CLASSIC_BIRD_SPECIALTY_H
#define AB_CLASSIC_BIRD_SPECIALTY_H

#include "ab/classic_shots.h"
#include "ab/audio_bank.h"
#include "ab/event_manager.h"
#include "ab/ui_frame_tree.h"
#include "ab/classic_blues_split.h"
#include "ab/classic_bird_boost.h"

typedef struct AbClassicBirdSpecialty AbClassicBirdSpecialty;

typedef struct {
    AbClassicShots *shots;
    AbClassicBehaviors *behaviors;
    AbAudioBank *audio;
    AbEventManager *events;
    AbUiFrameNode *notifications;
    const AbDataValue *flying, *red_available;
    size_t notifications_capacity, storage_bytes;
    AbSettingsStatistics *settings;
    AbClassicBluesSplit *blues;
    AbClassicBirdBoost boost;
} AbClassicBirdSpecialtyConfig;

/* Concrete ordinary triggerSpecialty125163 and triggerBirdSpecialty125065.
 * Borrow canonical globals/objects, actual behavior groups, decoded bank,
 * EventManager and real notifications Frame tree until close. Bootstrap supplies
 * the resolved red feature flag and populates that GUI tree through its owner.
 * No inferred empty GUI or missing hook, effects, RNG or default flags at open.
 * Retain event payloads until borrowing listeners finish; calls allocate nothing.
 * Plain immutable definitions and canonical live physical objects only. An
 * optional real Blues branch borrows these same shot/behavior/flying owners.
 */
bool ab_classic_bird_specialty_open(AbClassicBirdSpecialtyConfig config,
                                    AbClassicBirdSpecialty **output, AbError *error);
void ab_classic_bird_specialty_close(AbClassicBirdSpecialty *specialty);
size_t ab_classic_bird_specialty_bytes(const AbClassicBirdSpecialty *specialty);

/* Clear availability before even the nil-bird gate. Publish the definition's
 * specialty and physical flags before optional hooks. The ordinary red callback
 * performs the actual redBirdFingerTutorial lookup before its feature gates,
 * then real fallback sound. Notify before fresh GameObject/behavior dispatch.
 * Bind actual settings for extra tutorials. CLUSTER_BOMB uses the optional real
 * Blues branch, then common hook/event/parent behavior dispatch before final
 * parent removeBird. BOOST uses the actual impulse/particle/audio/visual/puff
 * bindings before the common tail. Active red tutorials/forced targeting, other
 * ordinary abilities and unresolved named hooks reject when reached. Reentry rejects; a
 * late error retains prior effects and stops this owner. This does not choose the
 * enclosing input gate or execute a complete frame.
 */
bool ab_classic_bird_specialty_trigger(AbClassicBirdSpecialty *specialty,
                                       AbDataValue bird, float seconds, AbError *error);
bool ab_classic_bird_specialty_trigger_flying(AbClassicBirdSpecialty *specialty,
                                              float seconds, AbError *error);

#endif
