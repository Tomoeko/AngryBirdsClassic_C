#ifndef AB_MP3_H
#define AB_MP3_H

#include "ab/pcm.h"

typedef struct {
    AbSpan frames;
    AbSpan leading_tag;
    AbSpan trailing_tag;
    size_t frame_count;
    uint32_t sample_rate;
    uint16_t channels;
    bool has_xing;
    bool has_lame;
    uint32_t encoder_delay;
    uint32_t encoder_padding;
    uint64_t sample_frames;
} AbMp3;

/* Bounded structural reader for the target's MPEG-2 Layer III streams. Walk
 * complete frames with fixed rate/channel count, allowing varying bitrate and
 * frame padding. Skip a bounded leading ID3v2.2/2.3/2.4 tag and optional trailing
 * ID3v1 tag. Parse first-frame Xing/Info fields and LAME delay/padding when
 * present, checking their sizes/counts against the complete frame walk.
 * sample_frames excludes the Xing frame and LAME delay/padding when present;
 * otherwise it counts complete MPEG frames at 576 samples each. ID3 contents,
 * MPEG/LAME CRCs, side information and compressed audio data are not decoded or
 * validated. Other MPEG versions/layers, free format, changing format, leading
 * junk and unknown trailing data reject explicitly. Input stays borrowed; no
 * allocation or audio output occurs. Failure preserves output. */
bool ab_mp3_read(AbSpan input, AbMp3 *output, AbError *error);

typedef struct {
    uint16_t part23_length, big_values, scalefactor_compress;
    uint8_t global_gain;
    bool window_switch;
    uint8_t block_type;
    bool mixed_block;
    uint8_t table_select[3], subblock_gain[3], region_count[2];
    bool scalefactor_scale, count1_table;
} AbMp3Granule;

typedef struct {
    uint16_t main_data_begin, channels;
    uint8_t private_bits;
    AbMp3Granule channel[2];
} AbMp3SideInfo;

/* Read the MPEG-2 Layer III single-granule side information: exactly nine mono
 * or seventeen stereo bytes, after any frame CRC. Preserve encoded table/region
 * fields; switched blocks have two tables and three subblock gains, long blocks
 * have three tables and two region counts. Absent branch fields start zero.
 * Reject oversized big_values and switched block type zero. This does not
 * resolve scalefactors, region boundaries, bit reservoir, Huffman data or PCM.
 * No allocation; invalid input preserves output. */
bool ab_mp3_side_info(AbSpan input, uint16_t channels, AbMp3SideInfo *output,
                      AbError *error);

typedef struct {
    AbSpan frame, side_information, main_data;
    uint32_t header, sample_rate;
    uint16_t channels, crc;
    bool has_crc;
    AbMp3SideInfo side;
} AbMp3FrameInfo;

/* Read one complete MPEG-2 Layer III frame at the input start. Later bytes may
 * follow; advance by frame.size. Return borrowed frame/side/main-data spans,
 * parsed side fields and any stored CRC word. Do not validate CRC or decode the
 * reservoir/Huffman/scalefactor/sample data. Tags must be removed first, using
 * AbMp3.frames; callers still recognize/skip its Xing metadata frame. No
 * allocation; invalid input preserves output. */
bool ab_mp3_frame_info(AbSpan input, AbMp3FrameInfo *output, AbError *error);

typedef struct {
    uint8_t bytes[255];
    size_t size;
} AbMp3Reservoir;

/* Begin with a zero-initialized reservoir. Join the requested previous tail
 * with the frame's complete main-data payload, then retain the newest 255
 * payload bytes for later frames. Push Xing metadata frames too, even when
 * skipping their audio decode. Missing previous bytes reject; native damaged-
 * frame repair is outside this valid-stream boundary. Frame info must come from
 * the bounded reader above. Output, payload and reservoir storage are disjoint.
 * No allocation. Check all bounds before writes; failure preserves everything. */
bool ab_mp3_reservoir_push(AbMp3Reservoir *reservoir, const AbMp3FrameInfo *frame,
                           uint8_t *output, size_t capacity, AbSpan *window,
                           AbError *error);

typedef struct {
    uint8_t values[39];
    uint8_t count, data_count;
    uint16_t bit_count;
    bool preflag;
} AbMp3Scalefactors;

/* Decode one MPEG-2 LSF channel's four scalefactor partitions from an already
 * joined main-data window. bit_offset is the channel's part2_3 start; stereo
 * channel one follows channel zero's full part23_length. The caller selects
 * intensity stereo only for its actual second-channel route. Return original
 * long/short/mixed ordering, trailing zero slots and derived preflag. Blocks one
 * and three use long partitions. No Huffman, requantization or sample decoding.
 * Valid bounded input only: <=1024 window bytes, compression <=511, block <=3
 * and sufficient part23_length. No allocation; failure preserves output. */
bool ab_mp3_scalefactors(const AbMp3Granule *granule, bool intensity, AbSpan main_data,
                         size_t bit_offset, AbMp3Scalefactors *output, AbError *error);

typedef struct {
    AbSpan input;
    size_t position, limit;
} AbMp3HuffmanBits;

/* Decode one MSB-first spectral pair or count1 quad inside a <=1024-byte joined
 * window. limit is an explicit bit budget, at most input.size*8. Pair tables
 * include native zero-book aliases 4/14; tables >=32 reject. Quad selector is
 * the transmitted count1 flag. Consume linbits and nonzero signs in native
 * order, returning signed quantized integers. No requantization or reordering.
 * Caller storage is disjoint. No allocation; failed prefix/extension/sign reads
 * preserve both cursor and output. */
bool ab_mp3_huffman_pair(AbMp3HuffmanBits *bits, uint32_t table, int16_t output[2],
                         AbError *error);
bool ab_mp3_huffman_quad(AbMp3HuffmanBits *bits, bool second, int16_t output[4],
                         AbError *error);

/* Resolve pair-region endpoints for valid MPEG-2 granules at 16/22.05/24 kHz.
 * Returned endpoints count pairs, capped at 288. Short switched blocks split
 * at 18 pairs; switched start/stop blocks at 27. Long blocks use encoded region
 * counts and the sample rate's long-band boundaries. Failure preserves output. */
bool ab_mp3_huffman_regions(const AbMp3Granule *granule, uint32_t sample_rate,
                            uint16_t pair_ends[2], AbError *error);

/* Read all big-value pairs and count1 data after the caller's scalefactor bits.
 * Return 576 signed quantized values in frequency/window order, including
 * short reordering and zero tail. Mixed short blocks currently reject. The
 * native mixed mapping/overflow route remains a separate recovery task.
 * A count1 code/sign prefix at the budget
 * end follows the native partial-prefix behavior; big-value truncation rejects.
 * main_data is a <=1024-byte joined window, bit_offset starts this channel's
 * complete part23 span. No requantization, stereo transform or PCM. Caller
 * storage is disjoint. No allocation; failure preserves output. */
bool ab_mp3_quantized_spectrum(const AbMp3Granule *granule, uint32_t sample_rate,
                               AbSpan main_data, size_t bit_offset,
                               uint16_t scalefactor_bits, int16_t output[576],
                               AbError *error);

/* Reconstruct the bounded unsigned magnitude^(4/3) factor as binary32 using
 * integer comparisons, without host pow/cbrt or a decoder lookup dependency.
 * Accept magnitudes 0..8206. No scale/gain application or allocation. Failure
 * preserves output. */
bool ab_mp3_magnitude_power(uint16_t magnitude, float *output, AbError *error);

/* Return the binary32 factor 2^(-quarter_steps/4) over the original gain table's
 * complete -46..331 range. Use four fractional powers and exact exponent
 * adjustment; no host math or allocation. Failure preserves output. */
bool ab_mp3_gain_power(int quarter_steps, float *output, AbError *error);

/* Apply global gain, scalefactors, long pre-emphasis and short-window gains to
 * frequency-ordered quantized values. mid_side selects the native shared
 * inverse-sqrt-two gain adjustment; this does not perform a stereo transform.
 * Accept the long/start/stop and non-mixed short layouts supported above,
 * canonical 22/39 scalefactor slots and magnitudes <=8206. Input/output storage
 * is disjoint. No allocation; failure preserves output. */
bool ab_mp3_requantize_spectrum(const AbMp3Granule *granule,
                                const AbMp3Scalefactors *scalefactors,
                                uint32_t sample_rate, bool mid_side,
                                const int16_t quantized[576], float output[576],
                                AbError *error);

typedef struct {
    uint8_t long_band;
    uint8_t short_bands[3];
    uint8_t active_subbands;
} AbMp3SpectralLimits;

/* Derive last-nonzero scalefactor bands and the native full-rate subband bound
 * from frequency-ordered quantized values. Supported layouts/rates match the
 * spectrum decoder. No allocation; failure preserves output. */
bool ab_mp3_spectral_limits(const AbMp3Granule *granule, uint32_t sample_rate,
                            const int16_t quantized[576], AbMp3SpectralLimits *output,
                            AbError *error);

/* Recover left/right from already normalized mid/side spectra. The preceding
 * requantization stage supplies the shared inverse-sqrt-two gain. Arrays are
 * disjoint. active_subbands is the decoded shared bound 1..32; preserve values
 * beyond that bound. Reject nonfinite inputs/results atomically. */
bool ab_mp3_mid_side_spectrum(uint8_t active_subbands, float left[576],
                              float right[576], AbError *error);

/* Apply the eight alias rotations at adjacent long-block subband boundaries.
 * Pure short blocks pass through; mixed short support remains unrecovered.
 * active_subbands supplies the decoded channel bound 1..32, including the last
 * active subband. Compute fixed coefficients with first-party arithmetic. No heap
 * allocation. Invalid metadata or nonfinite data/results preserve the input. */
bool ab_mp3_alias_spectrum(const AbMp3Granule *granule, uint8_t active_subbands,
                           float frequency[576], AbError *error);

typedef struct {
    float cosine[9];
    float half_cosine[9];
} AbMp3TransformCoefficients;

typedef struct {
    float values[2][4][36]; /* Subband parity, block type, coefficient. */
} AbMp3HybridWindows;

/* Generate all factored hybrid windows, including the odd-subband sign change.
 * Short windows use twelve coefficients followed by positive zero padding.
 * No host trigonometry, allocation or global mutable state. */
bool ab_mp3_hybrid_windows_init(AbMp3HybridWindows *output, AbError *error);

/* Generate factored hybrid-transform coefficients with bounded binary64
 * arithmetic. Initialize once and borrow the immutable table across subbands.
 * No host trigonometric function, allocation or global mutable state. */
bool ab_mp3_transform_init(AbMp3TransformCoefficients *output, AbError *error);

/* Transform one 18-value subband, combine its preceding overlap and retain the
 * next overlap. window is the original decoder's 36-value factored synthesis
 * window, supplied explicitly; window construction is a separate stage.
 * Arrays are disjoint; outputs are contiguous, not synthesis-matrix strides.
 * Reject nonfinite inputs/results atomically. No allocation. */
bool ab_mp3_long_transform(const AbMp3TransformCoefficients *transform,
                           const float frequency[18], const float previous[18],
                           const float window[36], float samples[18], float next[18],
                           AbError *error);

/* Transform three interleaved six-value short windows. Combine the first
 * portion with preceding overlap and retain the next twelve values, followed
 * by six positive zeros. window supplies twelve factored coefficients for
 * this subband parity. Ownership/failure rules match the long transform. */
bool ab_mp3_short_transform(const AbMp3TransformCoefficients *transform,
                            const float frequency[18], const float previous[18],
                            const float window[12], float samples[18], float next[18],
                            AbError *error);

typedef struct {
    float values[576];
} AbMp3HybridOverlap;

/* Combine one channel's spectrum with its retained overlap into an 18-by-32
 * time-slot matrix. Zero-initialize overlap at stream start. Use generated
 * immutable coefficients/windows; supply the decoded/shared subband bound.
 * Native pair processing rounds the bound upward to an even subband count.
 * Mixed short blocks remain unsupported. Caller storage is disjoint. No heap
 * allocation; failure preserves both overlap and output. */
bool ab_mp3_hybrid_channel(const AbMp3TransformCoefficients *transform,
                           const AbMp3HybridWindows *windows,
                           const AbMp3Granule *granule, uint8_t active_subbands,
                           const float frequency[576], AbMp3HybridOverlap *overlap,
                           float output[576], AbError *error);

typedef struct {
    float factors[31];
} AbMp3SynthesisTransform;

/* Generate the five factored synthesis-transform coefficient groups. */
bool ab_mp3_synthesis_transform_init(AbMp3SynthesisTransform *output, AbError *error);

/* Convert a 32-subband time slot to contiguous cosine coefficients. The native
 * synthesis storage uses these in two strided halves with a repeated center;
 * this primitive does not apply synthesis windows, retain history or emit PCM.
 * Inputs/output are disjoint. Reject nonfinite inputs/results atomically. */
bool ab_mp3_synthesis_transform(const AbMp3SynthesisTransform *transform,
                                const float frequency[32], float output[32],
                                AbError *error);

typedef struct {
    float values[544];
} AbMp3SynthesisWindow;

typedef struct {
    float values[2][272];
    uint8_t phase;
} AbMp3SynthesisHistory;

/* Expand the target's quantized synthesis-window parameters with an explicit
 * nonnegative gain. The native default is 1.0. Initialize immutable window and
 * transform tables once; retain a separate history for each channel. */
bool ab_mp3_synthesis_window_init(double gain, AbMp3SynthesisWindow *output,
                                  AbError *error);
bool ab_mp3_synthesis_history_init(AbMp3SynthesisHistory *output, AbError *error);

/* Synthesize one channel's 32-sample time slot and advance its history. Apply
 * binary32 window accumulation, clamp to signed PCM16, and truncate toward
 * zero. clipped returns the number of clamped samples, 0..32. Caller storage
 * is disjoint. No allocation; failure preserves history, output and clipped. */
bool ab_mp3_synthesize(const AbMp3SynthesisTransform *transform,
                       const AbMp3SynthesisWindow *window, const float frequency[32],
                       AbMp3SynthesisHistory *history, int16_t output[32],
                       uint8_t *clipped, AbError *error);

/* Decode a complete supported MPEG-2 stream to owned interleaved signed PCM16
 * samples in host byte order, with native default gain and LAME trimming.
 * max_frames bounds allocation. Intensity stereo, mixed short blocks and LAME
 * padding below the observed 529-sample synthesis delay currently reject.
 * Input stays borrowed only during this call. Failure preserves output;
 * release any preceding owned output before a successful replacement. */
bool ab_mp3_decode(AbSpan input, size_t max_frames, AbPcm16 *output, AbError *error);

#endif
