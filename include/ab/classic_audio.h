#ifndef AB_CLASSIC_AUDIO_H
#define AB_CLASSIC_AUDIO_H

#include "ab/audio_resources.h"
#include "ab/audio_bank.h"
#include "ab/classic_post_physics.h"

/* Direct rolling resource host. Context is a live AbAudioResources binding.
 * Play requests loop on track two with unit speed and no fades/delays. Resolve
 * names directly, without Lua group selection or additional random draws. This
 * connects already decoded PCM assets to the mixer; APK audio loading, MP3,
 * resource-group lifetime and native device playback remain separate. */
bool ab_classic_audio_rolling(void *context, AbRollingAudioCall *call, AbError *error);
/* Same direct resource boundary with the shared APK-owned audio bank. */
bool ab_classic_audio_bank_rolling(void *context, AbRollingAudioCall *call,
                                   AbError *error);

#endif
