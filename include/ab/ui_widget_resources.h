#ifndef AB_UI_WIDGET_RESOURCES_H
#define AB_UI_WIDGET_RESOURCES_H

#include "ab/audio_bank.h"
#include "ab/sprite_provider.h"
#include "ab/ui_localization.h"
#include "ab/ui_widget.h"

typedef struct {
    const AbSpriteProvider *sprites;
    const AbUiLocalizationSource *strings;
    AbAudioBank *audio;
    bool initialized;
} AbUiWidgetResources;

/* Bind concrete widgets to active sprite/texture resources, the actual
 * getLocalizationString group provider, and playAudio's owned bank/mixer.
 * Keep this adapter at a stable address while its shared context is borrowed.
 * Resources, strings and audio outlive the widgets; serialize group mutations
 * and refresh affected widget image state after loading/unloading resources.
 * No decoding, allocation, RNG or playback occurs here. Existing viewport,
 * rendering policy, draw storage, input and lifecycle fields remain caller-owned.
 * Axis-aligned composites draw their exact cached KA3D/RVIO part selections.
 * Bounds and pivot queries retain native zero for an absent sprite. Absent
 * sprite draws append nothing, as Resources::drawSprite does. Present bindings
 * require active pixels; unsupported composite transforms fail explicitly.
 */
bool ab_ui_widget_resources_initialize(const AbSpriteProvider *sprites,
                                       const AbUiLocalizationSource *strings,
                                       AbAudioBank *audio, AbUiWidgetResources *adapter,
                                       AbUiWidgetContext *context, AbError *error);

#endif
