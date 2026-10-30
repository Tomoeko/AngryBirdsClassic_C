#ifndef AB_AUDIO_BANK_H
#define AB_AUDIO_BANK_H

#include "ab/audio_catalog.h"
#include "ab/audio_play.h"
#include "ab/audio_resources.h"
#include "ab/random.h"

typedef struct AbAudioBank AbAudioBank;

typedef struct {
    AbAudioMixer *mixer;
    AbRandom48 *random;
    size_t storage_bytes, entry_bytes, frames_per_clip;
} AbAudioBankConfig;

typedef struct {
    AbSpan name, path;
    uint32_t id, sample_rate;
    AbPcm16View pcm;
    bool stream;
} AbAudioBankClip;

/* Own the pinned audio catalog, named metadata and decoded PCM for all original
 * registrations. Borrow the archive/key during open only; store no key. Borrow
 * the real mixer/shared script RNG for later calls. Apply the pinned
 * loadAudioAssets channel-volume/count-limit policy after loading. No RNG draw,
 * mixer start, native device opening or playback occurs during open.
 *
 * Retained allocations share storage_bytes; encoded/decoder workspaces have
 * separate per-entry bounds. A conservative four-byte/frame admission bound
 * reserves room before decoding either supported channel count. This is not a
 * peak-memory limit. Streaming registrations are decoded ahead of playback;
 * native stream refill timing is not reproduced. The default native research
 * format keeps the recovered equal-rate channel mapping. Configured 48-kHz
 * desktop output preserves source channels and supports both 16/24-kHz assets
 * through the separately tested presentation resampler.
 *
 * Later operations allocate/decode nothing. Serialize mixer/RNG access. Stop
 * and drain or destroy every borrowing mixer voice before close; close itself
 * never dereferences the mixer. Views and returned group strings expire at close.
 * Failed open releases owned resources; a failure while applying channel policy
 * can retain earlier mixer settings. Output starts NULL. */
bool ab_audio_bank_open(const AbZip *archive, AbSpan key, AbAudioBankConfig config,
                        AbAudioBank **output, AbError *error);
void ab_audio_bank_close(AbAudioBank *bank);
size_t ab_audio_bank_bytes(const AbAudioBank *bank);
size_t ab_audio_bank_count(const AbAudioBank *bank);
/* Optional desktop presentation policy: merge identical non-looping clip/gain/
 * track requests inside one application frame. Group RNG and game callbacks
 * still run normally. Later frames and stopped voices remain independent. */
void ab_audio_bank_coalesce_frame_effects(AbAudioBank *bank, bool enabled);
void ab_audio_bank_begin_frame(AbAudioBank *bank);
bool ab_audio_bank_clip(const AbAudioBank *bank, AbSpan name, AbAudioBankClip *output);
/* Sorted name order, with the same stable borrowed view as a name lookup. */
bool ab_audio_bank_at(const AbAudioBank *bank, size_t index, AbAudioBankClip *output);
/* Borrow the actual audioGroups constructor for source menu operations that
 * traverse a whole group rather than selecting one random member. */
const AbDataTable *ab_audio_bank_groups(const AbAudioBank *bank);

/* getAudioName is a separate ordered RNG boundary. A nil input/result remains
 * NULL; other names are NUL-terminated strings. Unknown names pass through as
 * the borrowed caller pointer. Existing groups return stable catalog strings.
 * No implicit group selection occurs inside playAudio. */
bool ab_audio_bank_name(AbAudioBank *bank, const char *requested, const char **output,
                        AbError *error);
/* Typed getAudioName boundary for script values and unterminated spans. Preserve
 * the selected value, including nil, without string conversion. Unknown names
 * borrow the input; group results borrow the catalog. No allocation. */
bool ab_audio_bank_name_value(AbAudioBank *bank, AbDataValue requested,
                              AbDataValue *output, AbError *error);

/* Apply the real playAudio metadata/channel overrides, then submit the resolved
 * resource to the borrowed mixer at unit speed with no fades/delays. Nil/empty
 * requests suppress after metadata resolution. Missing registered resources
 * retain the native no-op handle UINT32_MAX. Finite numeric gain, Boolean loop
 * and integral channels 0..7 are supported; other native coercions reject.
 * Earlier RNG/admission effects are not rolled back on a later failure. */
bool ab_audio_bank_play(AbAudioBank *bank, AbAudioPlay request, uint32_t *handle,
                        AbError *error);
/* Direct Resources playback after group selection. Preserve the explicit
 * native gain/loop/track without applying script metadata/channel volumes.
 * The same rate, lifetime and missing-resource rules apply. */
bool ab_audio_bank_play_resource(AbAudioBank *bank, AbSpan name, float gain, bool loop,
                                 uint32_t track, uint32_t *handle, AbError *error);
/* Default Slingshot.getLaunchSound selects bird_shot through getAudioName;
 * SlingshotSystem requests gain one/non-looping playback. The controller sets
 * its launch cue only after success. Custom slingshot classes remain separate. */
bool ab_audio_bank_default_sling_launch(AbAudioBank *bank, uint32_t *handle,
                                        AbError *error);
bool ab_audio_bank_is_playing(const AbAudioBank *bank, AbSpan name, bool *output,
                              AbError *error);
bool ab_audio_bank_stop(AbAudioBank *bank, AbSpan name, AbError *error);
bool ab_audio_bank_gain(AbAudioBank *bank, AbSpan name, float gain, AbError *error);

#endif
