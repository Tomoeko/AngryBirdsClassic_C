#ifndef AB_ORACLE_MP3_SIDE_H
#define AB_ORACLE_MP3_SIDE_H

#include <stdbool.h>

static int (*mp3_original_mono)(void *, void *);
static int (*mp3_original_stereo)(void *, void *, void *);
static uint32_t *mp3_side_decoder;
static bool mp3_side_captured;
static uint32_t mp3_side_header[2], mp3_side_channels[2][16];
#if defined(AB_ORACLE_MP3_PRISTINE)
__attribute__((used)) static volatile uint32_t mp3_pristine_header;

__attribute__((used, noinline)) static void mp3_capture_ready(void *decoder) {
    __asm__ volatile("nop" : : "r"(decoder) : "memory");
}
#endif
#if defined(AB_ORACLE_MP3_HUFFMAN) || defined(AB_ORACLE_MP3_SPECTRUM) ||               \
    defined(AB_ORACLE_MP3_REQUANTIZE)
#include "mp3_huffman_probe.h"
#endif
#if defined(AB_ORACLE_MP3_SCALEFACTORS)
static uint32_t mp3_scalefactor_info[2][2], mp3_scalefactor_values[2][39];
#endif
#if defined(AB_ORACLE_MP3_RESERVOIR)
static uint32_t mp3_reservoir_bytes, mp3_reservoir_words[256];
#endif

static void capture_mp3_side(uint32_t *decoder, const uint32_t *caller_stack) {
    if (mp3_side_captured) {
        return;
    }
    if (decoder != mp3_side_decoder || decoder[0x12f5] != 1 || decoder[0x12f0] < 1 ||
        decoder[0x12f0] > 2) {
        fail("Unexpected original MPEG-2 side-information context\n");
    }
    /* Original do_layer3's fixed stack allocation, verified at its synthesis
     * callback sites. Read only fields initialized by the single LSF granule;
     * retain original synthesis by forwarding each callback exactly once. */
    const uint32_t *side = caller_stack + 0x200 / 4;
#if defined(AB_ORACLE_MP3_HUFFMAN) || defined(AB_ORACLE_MP3_SPECTRUM) ||               \
    defined(AB_ORACLE_MP3_REQUANTIZE)
    capture_mp3_huffman(decoder, side);
#endif
    mp3_side_header[0] = side[0];
    mp3_side_header[1] = side[1];
    static const size_t fields[] = {3, 4, 5, 6, 7, 8, 9, 10, 16, 17, 19, 20};
    for (size_t channel = 0; channel < decoder[0x12f0]; ++channel) {
        const uint32_t *source = side + channel * 46;
        uint32_t *output = mp3_side_channels[channel];
        for (size_t index = 0; index < 12; ++index) {
            output[index] = source[fields[index]];
        }
        output[12] = source[24] - (uint32_t)(uintptr_t)decoder;
        for (size_t window = 0; window < 3; ++window) {
            output[13 + window] =
                source[6] ? source[21 + window] - source[24] : UINT32_MAX;
        }
#if defined(AB_ORACLE_MP3_SCALEFACTORS)
        size_t count = source[6] != 2 ? 22u : source[7] ? 38u : 39u;
        mp3_scalefactor_info[channel][0] = (uint32_t)count;
        mp3_scalefactor_info[channel][1] = source[18];
        const uint32_t *scales = caller_stack + (0xc8 + channel * 0x9c) / 4;
        for (size_t index = 0; index < count; ++index) {
            mp3_scalefactor_values[channel][index] = scales[index];
        }
#endif
    }
#if defined(AB_ORACLE_MP3_RESERVOIR)
    size_t frame_bytes = decoder[0x4c18 / 4];
    size_t side_bytes = decoder[0x4c98 / 4];
    size_t backstep = side[0];
    if (frame_bytes < side_bytes || frame_bytes > 717 || backstep > 255 ||
        frame_bytes - side_bytes + backstep > 1024) {
        fail("Original MPEG-2 reservoir window exceeds bounds\n");
    }
    const uint8_t *main_data =
        (const uint8_t *)(uintptr_t)decoder[0x6ba0 / 4] + side_bytes;
    const uint8_t *window = main_data - backstep;
    mp3_reservoir_bytes = (uint32_t)(frame_bytes - side_bytes + backstep);
    for (size_t index = 0; index < 256; ++index) {
        mp3_reservoir_words[index] = 0;
    }
    for (size_t index = 0; index < mp3_reservoir_bytes; ++index) {
        mp3_reservoir_words[index / 4] |= (uint32_t)window[index] << ((index % 4) * 8);
    }
#endif
    mp3_side_captured = true;
}

__attribute__((used)) static int mp3_side_mono(void *bands, uint32_t *decoder,
                                               const uint32_t *caller_stack) {
    capture_mp3_side(decoder, caller_stack);
    return mp3_original_mono(bands, decoder);
}

__attribute__((used)) static int mp3_side_stereo(void *left, void *right,
                                                 uint32_t *decoder,
                                                 const uint32_t *caller_stack) {
    capture_mp3_side(decoder, caller_stack);
    return mp3_original_stereo(left, right, decoder);
}

__attribute__((naked)) static int mp3_side_mono_entry(void *bands
                                                      __attribute__((unused)),
                                                      void *decoder
                                                      __attribute__((unused))) {
    __asm__ volatile("mov r2, sp\n"
                     "b mp3_side_mono\n");
}

__attribute__((naked)) static int
mp3_side_stereo_entry(void *left __attribute__((unused)),
                      void *right __attribute__((unused)),
                      void *decoder __attribute__((unused))) {
    __asm__ volatile("mov r3, sp\n"
                     "b mp3_side_stereo\n");
}

static void probe_mp3_side(void) {
    int (*initialize)(void) = (int (*)(void))find_function("mpg123_init");
    void *(*create)(const char *, int *) =
        (void *(*)(const char *, int *))find_function("mpg123_new");
    int (*open)(void *, const char *) =
        (int (*)(void *, const char *))find_function("mpg123_open");
    int (*next)(void *) = (int (*)(void *))find_function("mpg123_framebyframe_next");
    int (*data)(void *, uint32_t *, uint8_t **, size_t *) = (int (*)(
        void *, uint32_t *, uint8_t **, size_t *))find_function("mpg123_framedata");
    int (*decode)(void *, int32_t *, uint8_t **, size_t *) =
        (int (*)(void *, int32_t *, uint8_t **, size_t *))find_function(
            "mpg123_framebyframe_decode");
    void (*destroy)(void *) = (void (*)(void *))find_function("mpg123_delete");
    void (*deinitialize)(void) = (void (*)(void))find_function("mpg123_exit");
    int error = 0;
    if (initialize()) {
        fail("Cannot initialize original MPEG-2 side-information probe\n");
    }
    uint32_t *decoder = create(NULL, &error);
    if (!decoder || error) {
        fail("Cannot create original side-information decoder\n");
    }
#if defined(AB_ORACLE_MP3_PRISTINE)
    mp3_capture_ready(decoder);
#endif
    if (open(decoder, "/data/local/tmp/ab520/mp3-input.mp3")) {
        fail("Cannot open original side-information input\n");
    }
#if defined(AB_ORACLE_MP3_REQUANTIZE)
    initialize_mp3_huffman();
    semihost(4, "AB_ANDROID_MP3_REQUANTIZE_V1\n");
#elif defined(AB_ORACLE_MP3_SPECTRUM)
    initialize_mp3_huffman();
    semihost(4, "AB_ANDROID_MP3_SPECTRUM_V1\n");
#elif defined(AB_ORACLE_MP3_HUFFMAN)
    initialize_mp3_huffman();
    semihost(4, "AB_ANDROID_MP3_HUFFMAN_V1\n");
#elif defined(AB_ORACLE_MP3_SCALEFACTORS)
    semihost(4, "AB_ANDROID_MP3_SCALEFACTORS_V1\n");
#elif defined(AB_ORACLE_MP3_RESERVOIR)
    semihost(4, "AB_ANDROID_MP3_RESERVOIR_V1\n");
#else
    semihost(4, "AB_ANDROID_MP3_SIDE_V1\n");
#endif
    size_t frames = 0;
    for (size_t attempt = 0; attempt < 16384; ++attempt) {
#if defined(AB_ORACLE_MP3_PRISTINE)
        if (frames == 32) {
            break;
        }
#endif
        int status = next(decoder);
        if (status == -10 || status == -12) {
            break;
        }
        if (status && status != -11) {
            fail("Original side-information next-frame failed\n");
        }
        uint32_t header;
        uint8_t *body = NULL;
        size_t bytes = 0;
        if (data(decoder, &header, &body, &bytes) || !body ||
            ((header >> 19) & 3) != 2 || ((header >> 17) & 3) != 1) {
            fail("Unexpected original side-information frame\n");
        }
        uint32_t channels = ((header >> 6) & 3) == 3 ? 1u : 2u;
        size_t offset = (header & UINT32_C(0x10000)) ? 0u : 2u;
        size_t side_bytes = channels == 1 ? 9u : 17u;
        if (bytes < offset + side_bytes) {
            fail("Truncated original side-information frame\n");
        }
        uint32_t raw[17];
        for (size_t index = 0; index < side_bytes; ++index) {
            raw[index] = body[offset + index];
        }
#if defined(AB_ORACLE_MP3_PRISTINE)
        mp3_pristine_header = header;
#endif
#if defined(AB_ORACLE_MP3_SPECTRUM) || defined(AB_ORACLE_MP3_REQUANTIZE)
        if ((raw[5] & 1u) && (raw[6] >> 6) == 2 && (raw[6] & 32u)) {
            fail("Native mixed-short spectral mapping requires separate recovery\n");
        }
#endif
        mp3_side_captured = false;
        mp3_side_decoder = decoder;
        mp3_original_mono = (int (*)(void *, void *))(uintptr_t)decoder[0x12ee];
        mp3_original_stereo =
            (int (*)(void *, void *, void *))(uintptr_t)decoder[0x12ed];
        decoder[0x12ee] = (uint32_t)(uintptr_t)mp3_side_mono_entry;
        decoder[0x12ed] = (uint32_t)(uintptr_t)mp3_side_stereo_entry;
        int32_t number = -1;
        uint8_t *audio = NULL;
        size_t audio_bytes = 0;
        status = decode(decoder, &number, &audio, &audio_bytes);
        decoder[0x12ee] = (uint32_t)(uintptr_t)mp3_original_mono;
        decoder[0x12ed] = (uint32_t)(uintptr_t)mp3_original_stereo;
        if (status || !mp3_side_captured) {
            uint32_t failure[] = {(uint32_t)status, mp3_side_captured ? 1u : 0u,
                                  (uint32_t)number, header, (uint32_t)frames};
            print_words(failure, 5);
            fail("Original side-information synthesis observation failed\n");
        }
        uint32_t frame[] = {header, (uint32_t)bytes, channels, (uint32_t)side_bytes,
                            (uint32_t)number};
        print_words(frame, 5);
        print_words(raw, side_bytes);
        print_words(mp3_side_header, 2);
#if defined(AB_ORACLE_MP3_REQUANTIZE)
        print_words(mp3_requantize_control, 41);
        print_words(mp3_huffman_observation, 3);
        for (size_t index = 3; index < 579; index += 192) {
            print_words(mp3_huffman_observation + index, 192);
        }
#elif defined(AB_ORACLE_MP3_SPECTRUM)
        print_words(mp3_huffman_observation, 3);
        for (size_t index = 3; index < MP3_SPECTRAL_WORDS; index += 192) {
            print_words(mp3_huffman_observation + index, 192);
        }
#elif defined(AB_ORACLE_MP3_HUFFMAN)
        print_words(mp3_huffman_observation, MP3_SPECTRAL_WORDS);
#endif
        for (size_t channel = 0; channel < channels; ++channel) {
            print_words(mp3_side_channels[channel], 16);
#if defined(AB_ORACLE_MP3_SCALEFACTORS)
            print_words(mp3_scalefactor_info[channel], 2);
            print_words(mp3_scalefactor_values[channel],
                        mp3_scalefactor_info[channel][0]);
#endif
        }
#if defined(AB_ORACLE_MP3_RESERVOIR)
        print_words(&mp3_reservoir_bytes, 1);
        print_words(mp3_reservoir_words, (mp3_reservoir_bytes + 3u) / 4u);
#endif
        ++frames;
    }
    destroy(decoder);
    deinitialize();
    if (!frames || frames >= 16384) {
        fail("Original side-information capture count exceeded bounds\n");
    }
    uint32_t count = (uint32_t)frames;
    print_words(&count, 1);
#if defined(AB_ORACLE_MP3_REQUANTIZE)
    semihost(4, "AB MP3 requantization reference complete\n");
#elif defined(AB_ORACLE_MP3_SPECTRUM)
    semihost(4, "AB MP3 spectrum reference complete\n");
#elif defined(AB_ORACLE_MP3_HUFFMAN)
    semihost(4, "AB MP3 Huffman reference complete\n");
#elif defined(AB_ORACLE_MP3_SCALEFACTORS)
    semihost(4, "AB MP3 scalefactor reference complete\n");
#elif defined(AB_ORACLE_MP3_RESERVOIR)
    semihost(4, "AB MP3 reservoir reference complete\n");
#else
    semihost(4, "AB MP3 side-information reference complete\n");
#endif
}

#endif
