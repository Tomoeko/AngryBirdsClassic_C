#include "android_io.h"

extern void *dlopen(const char *path, int mode);
extern void *dlsym(void *library, const char *name);

static uint8_t input[512 * 1024], pixels[8 * 1024 * 1024];
static size_t width, height, written_rows;

static uint32_t word(const uint8_t *bytes) {
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) | ((uint32_t)bytes[2] << 16) |
           ((uint32_t)bytes[3] << 24);
}

static uint32_t hash(const uint8_t *bytes, size_t size) {
    uint32_t value = UINT32_C(2166136261);
    for (size_t index = 0; index < size; ++index) {
        value = (value ^ bytes[index]) * UINT32_C(16777619);
    }
    return value;
}

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

static const uint8_t *find_frame(size_t size, size_t *frame_size) {
    if (size < 12 || word(input) != UINT32_C(0x46464952) ||
        word(input + 8) != UINT32_C(0x50424557) || word(input + 4) != size - 8) {
        system_exit(5);
    }
    size_t offset = 12;
    while (size - offset >= 8) {
        size_t count = word(input + offset + 4);
        if (count > size - offset - 8) {
            system_exit(5);
        }
        if (word(input + offset) == UINT32_C(0x20385056)) {
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

static void save_pixels(const char *path, size_t size) {
    int file = system_call(5, (int)(uintptr_t)path, (const void *)(uintptr_t)577, 0600);
    if (file < 0) {
        system_exit(6);
    }
    size_t offset = 0;
    while (offset < size) {
        int count = system_call(4, file, pixels + offset, size - offset);
        if (count <= 0 || (size_t)count > size - offset) {
            system_exit(6);
        }
        offset += (size_t)count;
    }
    if (system_call(6, file, NULL, 0)) {
        system_exit(6);
    }
}

static int copy_rows(const uint8_t *io) {
    size_t row = word(io + 8), row_width = word(io + 12), count = word(io + 16);
    size_t y_stride = word(io + 32), uv_stride = word(io + 36);
    if (row != written_rows || row_width != width || row > height || !count ||
        count > height - row || row % 2 || y_stride < width ||
        uv_stride < (width + 1) / 2) {
        system_exit(7);
    }
    const uint8_t *y = (const uint8_t *)(uintptr_t)word(io + 20);
    const uint8_t *u = (const uint8_t *)(uintptr_t)word(io + 24);
    const uint8_t *v = (const uint8_t *)(uintptr_t)word(io + 28);
    if (!y || !u || !v) {
        system_exit(7);
    }
    for (size_t line = 0; line < count; ++line) {
        for (size_t column = 0; column < width; ++column) {
            pixels[(row + line) * width + column] = y[line * y_stride + column];
        }
    }
    size_t chroma_width = (width + 1) / 2;
    size_t y_size = width * height;
    size_t chroma_size = chroma_width * ((height + 1) / 2);
    for (size_t line = 0; line < (count + 1) / 2; ++line) {
        for (size_t column = 0; column < chroma_width; ++column) {
            size_t offset = (row / 2 + line) * chroma_width + column;
            pixels[y_size + offset] = u[line * uv_stride + column];
            pixels[y_size + chroma_size + offset] = v[line * uv_stride + column];
        }
    }
    written_rows += count;
    return 1;
}

static void print_words(const uint32_t words[7]) {
    static const char digits[] = "0123456789abcdef";
    char line[7 * 9];
    for (size_t index = 0; index < 7; ++index) {
        for (unsigned digit = 0; digit < 8; ++digit) {
            line[index * 9 + digit] = digits[(words[index] >> (28 - digit * 4)) & 15];
        }
        line[index * 9 + 8] = index == 6 ? '\n' : ' ';
    }
    write_bytes(line, sizeof(line));
}

void android_vp8_planes_main(void) {
    void *library = dlopen("/data/local/tmp/ab520/libAngryBirdsClassic.so", 2);
    if (!library) {
        system_exit(2);
    }
    void *(*create)(void);
    void (*destroy)(void *);
    int (*initialize_io)(void *, int);
    int (*headers)(void *, void *);
    int (*decode)(void *, void *);
    *(void **)&create = dlsym(library, "VP8New");
    *(void **)&destroy = dlsym(library, "VP8Delete");
    *(void **)&initialize_io = dlsym(library, "VP8InitIoInternal");
    *(void **)&headers = dlsym(library, "VP8GetHeaders");
    *(void **)&decode = dlsym(library, "VP8Decode");
    if (!create || !destroy || !initialize_io || !headers || !decode) {
        system_exit(2);
    }
    static const char header[] = "AB_ANDROID_VP8_UNFILTERED_V1 7\n";
    write_bytes(header, sizeof(header) - 1);
    for (uint32_t image = 0; image < 7; ++image) {
        char path[] = "/data/local/tmp/ab520/webp-0.webp";
        char output[] = "/data/local/tmp/ab520/webp-0.unfiltered.yuv";
        path[sizeof("/data/local/tmp/ab520/webp-") - 1] = (char)('0' + image);
        output[sizeof("/data/local/tmp/ab520/webp-") - 1] = (char)('0' + image);
        size_t frame_size;
        const uint8_t *frame = find_frame(read_input(path), &frame_size);
        uint64_t io[16];
        if (!initialize_io(io, 0x0200)) {
            system_exit(8);
        }
        uint32_t *fields = (uint32_t *)io;
        fields[15] = (uint32_t)frame_size;
        fields[16] = (uint32_t)(uintptr_t)frame;
        uint8_t *decoder = create();
        if (!decoder || !headers(decoder, io)) {
            system_exit(8);
        }
        width = word((const uint8_t *)io);
        height = word((const uint8_t *)io + 4);
        size_t bytes = width * height + 2 * ((width + 1) / 2) * ((height + 1) / 2);
        if (!width || width > 2048 || !height || height > 2048 ||
            bytes > sizeof(pixels)) {
            system_exit(8);
        }
        fields[11] = (uint32_t)(uintptr_t)&copy_rows;
        fields[17] = 1;
        fields[19] = fields[21] = 0;
        fields[20] = (uint32_t)width;
        fields[22] = (uint32_t)height;
        written_rows = 0;
        if (!decode(decoder, io) || word(decoder) || written_rows != height) {
            system_exit(8);
        }
        save_pixels(output, bytes);
        uint32_t summary[7] = {image,
                               (uint32_t)frame_size,
                               hash(frame, frame_size),
                               (uint32_t)width,
                               (uint32_t)height,
                               (uint32_t)bytes,
                               hash(pixels, bytes)};
        print_words(summary);
        destroy(decoder);
    }
    static const char complete[] = "AB VP8 unfiltered reference complete\n";
    write_bytes(complete, sizeof(complete) - 1);
    system_exit(0);
}
