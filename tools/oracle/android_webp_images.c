#include "android_io.h"

extern void *dlopen(const char *path, int mode);
extern void *dlsym(void *library, const char *name);

static uint8_t input[512 * 1024];

static uint32_t hash(const uint8_t *bytes, size_t size) {
    uint32_t result = UINT32_C(2166136261);
    for (size_t index = 0; index < size; ++index) {
        result = (result ^ bytes[index]) * UINT32_C(16777619);
    }
    return result;
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
    if (system_call(6, file, NULL, 0) || !size) {
        system_exit(4);
    }
    return size;
}

static void save_pixels(const char *path, const uint8_t *pixels, size_t size) {
    int file = system_call(5, (int)(uintptr_t)path, (const void *)(uintptr_t)577, 0600);
    if (file < 0) {
        system_exit(5);
    }
    while (size) {
        int count = system_call(4, file, pixels, size);
        if (count <= 0 || (size_t)count > size) {
            system_exit(5);
        }
        pixels += count;
        size -= (size_t)count;
    }
    if (system_call(6, file, NULL, 0)) {
        system_exit(5);
    }
}

static void print_words(const uint32_t words[7]) {
    static const char digits[] = "0123456789abcdef";
    char line[7 * 9];
    for (size_t word = 0; word < 7; ++word) {
        for (unsigned digit = 0; digit < 8; ++digit) {
            line[word * 9 + digit] = digits[(words[word] >> (28 - digit * 4)) & 15];
        }
        line[word * 9 + 8] = word == 6 ? '\n' : ' ';
    }
    write_bytes(line, sizeof(line));
}

void android_webp_images_main(void) {
    void *library = dlopen("/data/local/tmp/ab520/libAngryBirdsClassic.so", 2);
    if (!library) {
        system_exit(2);
    }
    int (*info)(const uint8_t *, size_t, int *, int *);
#if defined(AB_WEBP_YUV)
    uint8_t *(*decode)(const uint8_t *, size_t, uint8_t *, size_t, int, uint8_t *,
                       size_t, int, uint8_t *, size_t, int);
#else
    uint8_t *(*decode)(const uint8_t *, size_t, uint8_t *, size_t, int);
#endif
    void *(*allocate)(size_t);
    void (*release)(void *);
    *(void **)&info = dlsym(library, "WebPGetInfo");
#if defined(AB_WEBP_YUV)
    *(void **)&decode = dlsym(library, "WebPDecodeYUVInto");
#else
    *(void **)&decode = dlsym(library, "WebPDecodeRGBAInto");
#endif
    *(void **)&allocate = dlsym(library, "malloc");
    *(void **)&release = dlsym(library, "free");
    if (!info || !decode || !allocate || !release) {
        system_exit(2);
    }
#if defined(AB_WEBP_YUV)
    static const char header[] = "AB_ANDROID_WEBP_YUV_V1 7\n";
#else
    static const char header[] = "AB_ANDROID_WEBP_IMAGES_V1 7\n";
#endif
    write_bytes(header, sizeof(header) - 1);
    for (uint32_t index = 0; index < 7; ++index) {
        char path[] = "/data/local/tmp/ab520/webp-0.webp";
#if defined(AB_WEBP_YUV)
        char output[] = "/data/local/tmp/ab520/webp-0.yuv";
        output[sizeof(output) - 6] = (char)('0' + index);
#else
        char output[] = "/data/local/tmp/ab520/webp-0.rgba";
        output[sizeof(output) - 7] = (char)('0' + index);
#endif
        path[sizeof(path) - 7] = (char)('0' + index);
        size_t size = read_input(path);
        int width, height;
        if (!info(input, size, &width, &height) || width <= 0 || height <= 0 ||
            width > 4096 || height > 4096 ||
            (size_t)width > (16 * 1024 * 1024) / ((size_t)height * 4)) {
            system_exit(6);
        }
#if defined(AB_WEBP_YUV)
        size_t y_size = (size_t)width * (size_t)height;
        int chroma_stride = (width + 1) / 2;
        size_t chroma_size = (size_t)chroma_stride * (size_t)((height + 1) / 2);
        size_t bytes = y_size + chroma_size * 2;
#else
        size_t bytes = (size_t)width * (size_t)height * 4;
#endif
        uint8_t *pixels = allocate(bytes);
        if (!pixels) {
            system_exit(7);
        }
#if defined(AB_WEBP_YUV)
        if (decode(input, size, pixels, y_size, width, pixels + y_size, chroma_size,
                   chroma_stride, pixels + y_size + chroma_size, chroma_size,
                   chroma_stride) != pixels) {
#else
        if (decode(input, size, pixels, bytes, width * 4) != pixels) {
#endif
            system_exit(7);
        }
        save_pixels(output, pixels, bytes);
        uint32_t words[7] = {index,
                             (uint32_t)size,
                             hash(input, size),
                             (uint32_t)width,
                             (uint32_t)height,
                             (uint32_t)bytes,
                             hash(pixels, bytes)};
        print_words(words);
        release(pixels);
    }
#if defined(AB_WEBP_YUV)
    static const char complete[] = "AB WebP YUV reference complete\n";
#else
    static const char complete[] = "AB WebP image reference complete\n";
#endif
    write_bytes(complete, sizeof(complete) - 1);
    system_exit(0);
}
