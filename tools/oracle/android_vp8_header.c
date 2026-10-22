#include "android_io.h"

extern void *dlopen(const char *path, int mode);
extern void *dlsym(void *library, const char *name);

static uint8_t input[512 * 1024];

static size_t read_input(const char *path) {
    int file = system_call(5, (int)(uintptr_t)path, NULL, 0);
    if (file < 0) {
        system_exit(4);
    }
    size_t size = 0;
    for (;;) {
        if (size == sizeof(input)) {
            system_exit(4);
        }
        int count = system_call(3, file, input + size, sizeof(input) - size);
        if (count < 0 || (size_t)count > sizeof(input) - size) {
            system_exit(4);
        }
        if (!count) {
            break;
        }
        size += (size_t)count;
    }
    if (system_call(6, file, NULL, 0)) {
        system_exit(4);
    }
    return size;
}

static uint32_t word(const uint8_t *bytes) {
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) | ((uint32_t)bytes[2] << 16) |
           ((uint32_t)bytes[3] << 24);
}

static uint32_t hash(const uint8_t *bytes, size_t size) {
    uint32_t result = UINT32_C(2166136261);
    for (size_t index = 0; index < size; ++index) {
        result = (result ^ bytes[index]) * UINT32_C(16777619);
    }
    return result;
}

static int tag(const uint8_t *bytes, const char *expected) {
    for (size_t index = 0; index < 4; ++index) {
        if (bytes[index] != (uint8_t)expected[index]) {
            return 0;
        }
    }
    return 1;
}

static const uint8_t *find_frame(size_t size, size_t *frame_size) {
    if (size < 12 || !tag(input, "RIFF") || !tag(input + 8, "WEBP") ||
        word(input + 4) != size - 8) {
        system_exit(5);
    }
    size_t offset = 12;
    while (size - offset >= 8) {
        size_t count = word(input + offset + 4);
        if (count > size - offset - 8) {
            system_exit(5);
        }
        if (tag(input + offset, "VP8 ")) {
            *frame_size = count;
            return input + offset + 8;
        }
        if (count + (count & 1) > size - offset - 8) {
            system_exit(5);
        }
        offset += 8 + count + (count & 1);
    }
    system_exit(5);
}

static void print_words(const uint32_t *words, size_t count) {
    static const char digits[] = "0123456789abcdef";
    char line[264 * 9];
    if (!count || count > 264) {
        system_exit(3);
    }
    for (size_t index = 0; index < count; ++index) {
        for (unsigned digit = 0; digit < 8; ++digit) {
            line[index * 9 + digit] = digits[(words[index] >> (28 - digit * 4)) & 15];
        }
        line[index * 9 + 8] = index + 1 == count ? '\n' : ' ';
    }
    write_bytes(line, count * 9);
}

#if defined(AB_VP8_MODES)
static uint8_t top_modes[4 * 128];
static uint32_t macroblocks[200 * 128];

#if !defined(AB_VP8_RESIDUALS)
static void capture_modes(void *library, uint8_t *decoder, const uint8_t *frame) {
    int (*parse_row)(void *, void *);
    *(void **)&parse_row = dlsym(library, "VP8ParseIntraModeRow");
    uint32_t width = word(decoder + 284), height = word(decoder + 288);
    if (!parse_row || !width || width > 128 || !height || height > 128) {
        system_exit(10);
    }
    uint32_t dimensions[4] = {(uint32_t)decoder[44] | ((uint32_t)decoder[45] << 8),
                              (uint32_t)decoder[46] | ((uint32_t)decoder[47] << 8),
                              width, height};
    print_words(dimensions, 4);
    for (size_t index = 0; index < sizeof(top_modes); ++index) {
        top_modes[index] = 0;
    }
    ((uint32_t *)decoder)[1936 / 4] = (uint32_t)(uintptr_t)top_modes;
    ((uint32_t *)decoder)[1996 / 4] = (uint32_t)(uintptr_t)macroblocks;
    for (uint32_t row = 0; row < height; ++row) {
        ((uint32_t *)decoder)[1940 / 4] = 0;
        for (size_t index = 0; index < width * 200; ++index) {
            macroblocks[index] = 0;
        }
        if (!parse_row(decoder + 12, decoder)) {
            system_exit(11);
        }
        uintptr_t cursor = word(decoder + 24);
        int32_t pending = (int32_t)word(decoder + 20);
        int64_t consumed = (int64_t)(cursor - (uintptr_t)frame - 10) * 8 - pending - 8;
        uint32_t first_size = word(decoder + 40);
        if (cursor < (uintptr_t)frame + 10 ||
            cursor > (uintptr_t)frame + 10 + first_size || consumed < 0 ||
            (uint64_t)consumed > (uint64_t)first_size * 8) {
            system_exit(12);
        }
        uint32_t state[4] = {row, word(decoder + 16) + 1, (uint32_t)consumed,
                             word(decoder + 32)};
        print_words(state, 4);
        for (uint32_t column = 0; column < width; ++column) {
            const uint8_t *block = (const uint8_t *)(macroblocks + column * 200);
            uint32_t values[8] = {block[798],        block[797],
                                  block[768],        word(block + 769),
                                  word(block + 773), word(block + 777),
                                  word(block + 781), block[785]};
            print_words(values, 8);
        }
    }
    ((uint32_t *)decoder)[1936 / 4] = 0;
    ((uint32_t *)decoder)[1996 / 4] = 0;
}
#else
static uint8_t nonzero_context[2 * 129];

static void save_record(int file, const uint8_t *bytes, size_t size) {
    while (size) {
        int count = system_call(4, file, bytes, size);
        if (count <= 0 || (size_t)count > size) {
            system_exit(13);
        }
        bytes += count;
        size -= (size_t)count;
    }
}

static void capture_residuals(void *library, uint8_t *decoder, const uint8_t *frame,
                              uint32_t image) {
    int (*parse_row)(void *, void *);
    void (*initialize_dsp)(void);
    void (*initialize_row)(void *);
    int (*decode_mb)(void *, void *);
    *(void **)&parse_row = dlsym(library, "VP8ParseIntraModeRow");
    *(void **)&initialize_dsp = dlsym(library, "VP8DspInit");
    *(void **)&initialize_row = dlsym(library, "VP8InitScanline");
    *(void **)&decode_mb = dlsym(library, "VP8DecodeMB");
    uint32_t width = word(decoder + 284), height = word(decoder + 288);
    if (!parse_row || !initialize_dsp || !initialize_row || !decode_mb || !width ||
        width > 128 || !height || height > 128 || word(decoder + 308) != 1) {
        system_exit(10);
    }
    initialize_dsp();
    for (size_t index = 0; index < sizeof(top_modes); ++index) {
        top_modes[index] = 0;
    }
    for (size_t index = 0; index < sizeof(nonzero_context); ++index) {
        nonzero_context[index] = 0;
    }
    ((uint32_t *)decoder)[1936 / 4] = (uint32_t)(uintptr_t)top_modes;
    ((uint32_t *)decoder)[1948 / 4] = (uint32_t)(uintptr_t)(nonzero_context + 2);
    ((uint32_t *)decoder)[1996 / 4] = (uint32_t)(uintptr_t)macroblocks;
    /* Isolate residual decoding from the later pixel filter bookkeeping.
     * VP8GetHeaders sets a nonzero filter type before frame scratch exists. */
    ((uint32_t *)decoder)[2000 / 4] = 0;
    char path[] = "/data/local/tmp/ab520/webp-0.coefficients";
    path[sizeof("/data/local/tmp/ab520/webp-") - 1] = (char)('0' + image);
    int file = system_call(5, (int)(uintptr_t)path, (const void *)(uintptr_t)577, 0600);
    if (file < 0) {
        system_exit(13);
    }
    uint32_t digest = UINT32_C(2166136261), output_size = 0;
    uintptr_t token_start = (uintptr_t)frame + 10 + word(decoder + 40);
    uint8_t *reader = decoder + 312;
    for (uint32_t row = 0; row < height; ++row) {
        initialize_row(decoder);
        for (size_t index = 0; index < width * 200; ++index) {
            macroblocks[index] = 0;
        }
        if (!parse_row(decoder + 12, decoder)) {
            system_exit(11);
        }
        for (uint32_t column = 0; column < width; ++column) {
            ((uint32_t *)decoder)[1988 / 4] = column;
            if (!decode_mb(decoder, reader)) {
                system_exit(14);
            }
            int32_t pending = (int32_t)word(reader + 8);
            if (word(reader + 12) < token_start ||
                word(reader + 12) > word(reader + 16)) {
                system_exit(14);
            }
            int64_t consumed =
                (int64_t)(word(reader + 12) - token_start) * 8 - pending - 8;
            if (consumed < 0 ||
                (uint64_t)consumed > (uint64_t)(word(reader + 16) - token_start) * 8) {
                system_exit(14);
            }
            const uint8_t *block = (const uint8_t *)(macroblocks + column * 200);
            uint32_t record[196];
            for (size_t index = 0; index < 192; ++index) {
                record[index] = word(block + index * 4);
            }
            record[192] = word(reader + 4) + 1;
            record[193] = (uint32_t)consumed;
            record[194] = word(reader + 20);
            record[195] = (uint32_t)nonzero_context[2 + column * 2] |
                          ((uint32_t)nonzero_context[3 + column * 2] << 8) |
                          ((uint32_t)nonzero_context[0] << 16) |
                          ((uint32_t)nonzero_context[1] << 24);
            const uint8_t *bytes = (const uint8_t *)record;
            for (size_t index = 0; index < sizeof(record); ++index) {
                digest = (digest ^ bytes[index]) * UINT32_C(16777619);
            }
            save_record(file, bytes, sizeof(record));
            output_size += sizeof(record);
        }
    }
    if (system_call(6, file, NULL, 0)) {
        system_exit(13);
    }
    uint32_t summary[6] = {image, width, height, width * height, output_size, digest};
    print_words(summary, 6);
    ((uint32_t *)decoder)[1936 / 4] = 0;
    ((uint32_t *)decoder)[1948 / 4] = 0;
    ((uint32_t *)decoder)[1996 / 4] = 0;
}
#endif
#endif

void android_vp8_header_main(void) {
    void *library = dlopen("/data/local/tmp/ab520/libAngryBirdsClassic.so", 2);
    if (!library) {
        system_exit(2);
    }
    void *(*create)(void);
    void (*destroy)(void *);
    int (*initialize_io)(void *, int);
    int (*headers)(void *, void *);
    uint32_t (*read_value)(void *, int);
    *(void **)&create = dlsym(library, "VP8New");
    *(void **)&destroy = dlsym(library, "VP8Delete");
    *(void **)&initialize_io = dlsym(library, "VP8InitIoInternal");
    *(void **)&headers = dlsym(library, "VP8GetHeaders");
    *(void **)&read_value = dlsym(library, "VP8GetValue");
    if (!create || !destroy || !initialize_io || !headers || !read_value) {
        system_exit(2);
    }
#if defined(AB_VP8_RESIDUALS)
    static const char header[] = "AB_ANDROID_VP8_RESIDUALS_V1 7\n";
#elif defined(AB_VP8_MODES)
    static const char header[] = "AB_ANDROID_VP8_MODES_V1 7\n";
#else
    static const char header[] = "AB_ANDROID_VP8_HEADER_V2 7\n";
#endif
    write_bytes(header, sizeof(header) - 1);
    for (uint32_t index = 0; index < 7; ++index) {
        char path[] = "/data/local/tmp/ab520/webp-0.webp";
        path[sizeof(path) - 7] = (char)('0' + index);
        size_t size = read_input(path), frame_size;
        const uint8_t *frame = find_frame(size, &frame_size);
        uint32_t input_words[4] = {index, (uint32_t)frame_size, hash(frame, frame_size),
                                   (uint32_t)size};
        print_words(input_words, 4);
        uint64_t io[16];
        if (!initialize_io(io, 0x0200)) {
            system_exit(6);
        }
        ((uint32_t *)io)[15] = (uint32_t)frame_size;
        ((uint32_t *)io)[16] = (uint32_t)(uintptr_t)frame;
        uint8_t *decoder = create();
        if (!decoder || !headers(decoder, io) || word(decoder) ||
            word(decoder + 4) != 1) {
            system_exit(7);
        }
#if defined(AB_VP8_MODES)
#if defined(AB_VP8_RESIDUALS)
        capture_residuals(library, decoder, frame, index);
#else
        capture_modes(library, decoder, frame);
#endif
        destroy(decoder);
        continue;
#endif
        uint32_t fields[88];
        fields[0] = word(decoder);
        fields[1] = word(decoder + 4);
        fields[2] = decoder[36];
        fields[3] = decoder[37];
        fields[4] = decoder[38];
        fields[5] = word(decoder + 40);
        fields[6] = (uint32_t)decoder[44] | ((uint32_t)decoder[45] << 8);
        fields[7] = (uint32_t)decoder[46] | ((uint32_t)decoder[47] << 8);
        for (size_t field = 8; field < 12; ++field) {
            fields[field] = decoder[48 + field - 8];
        }
        for (size_t field = 12; field < 27; ++field) {
            fields[field] = word(decoder + 52 + (field - 12) * 4);
        }
        for (size_t field = 27; field < 35; ++field) {
            fields[field] = (uint32_t)(int32_t)(int8_t)decoder[112 + field - 27];
        }
        for (size_t field = 35; field < 38; ++field) {
            fields[field] = decoder[868 + field - 35];
        }
        fields[38] = word(decoder + 308);
        fields[39] = word(decoder + 284);
        fields[40] = word(decoder + 288);
        fields[41] = word(decoder + 1928);
        fields[42] = decoder[1932];
        fields[43] = word(decoder + 32);
        for (size_t segment = 0; segment < 4; ++segment) {
            for (size_t value = 0; value < 7; ++value) {
                fields[44 + segment * 7 + value] =
                    word(decoder + 740 + segment * 32 + value * 4);
            }
        }
        uint32_t partitions = fields[38];
        if (!partitions || partitions > 8) {
            system_exit(8);
        }
        for (size_t partition = 0; partition < 8; ++partition) {
            uint32_t start = 0, end = 0;
            if (partition < partitions) {
                const uint8_t *reader = decoder + 312 + partition * 24;
                int32_t pending = (int32_t)word(reader + 8);
                if (pending < -8 || pending > 16 || (pending + 8) % 8) {
                    system_exit(8);
                }
                uintptr_t prefetched = (uintptr_t)(pending + 8) / 8;
                uintptr_t begin = word(reader + 12) - prefetched;
                uintptr_t finish = word(reader + 16);
                if (begin < (uintptr_t)frame || finish < begin ||
                    finish - (uintptr_t)frame > frame_size) {
                    system_exit(8);
                }
                start = (uint32_t)(begin - (uintptr_t)frame);
                end = (uint32_t)(finish - (uintptr_t)frame);
            }
            fields[72 + partition * 2] = start;
            fields[73 + partition * 2] = end;
        }
        print_words(fields, 88);
        uint32_t probabilities[264];
        for (size_t probability = 0; probability < 264; ++probability) {
            probabilities[probability] = word(decoder + 871 + probability * 4);
        }
        print_words(probabilities, 264);
        uint32_t continuation[34];
        continuation[0] = word(decoder + 16) + 1;
        int32_t pending = (int32_t)word(decoder + 20);
        uintptr_t cursor = word(decoder + 24);
        if (cursor < (uintptr_t)frame + 10 ||
            cursor > (uintptr_t)frame + 10 + fields[5]) {
            system_exit(9);
        }
        int64_t consumed = (int64_t)(cursor - (uintptr_t)frame - 10) * 8 - pending - 8;
        if (consumed < 0 || (uint64_t)consumed > (uint64_t)fields[5] * 8) {
            system_exit(9);
        }
        continuation[1] = (uint32_t)consumed;
        for (size_t field = 0; field < 32; ++field) {
            unsigned width = (unsigned)(field * 7 + index * 3) % 11;
            continuation[field + 2] = read_value(decoder + 12, (int)width);
        }
        print_words(continuation, 34);
        destroy(decoder);
    }
#if defined(AB_VP8_RESIDUALS)
    static const char complete[] = "AB VP8 residual reference complete\n";
#elif defined(AB_VP8_MODES)
    static const char complete[] = "AB VP8 modes reference complete\n";
#else
    static const char complete[] = "AB VP8 header reference complete\n";
#endif
    write_bytes(complete, sizeof(complete) - 1);
    system_exit(0);
}
