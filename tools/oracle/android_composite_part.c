#include "android_io.h"
#include "../../tests/composite_part_cases.h"

extern void *dlopen(const char *path, int mode);
extern void *dlsym(void *library, const char *name);

void __aeabi_memclr8(void *destination, size_t size) {
    volatile uint8_t *bytes = destination;
    for (size_t index = 0; index < size; ++index) {
        bytes[index] = 0;
    }
}

void __aeabi_memclr4(void *destination, size_t size) {
    __aeabi_memclr8(destination, size);
}

typedef struct {
    uint32_t length, capacity, references;
    char bytes[32];
} NativeText;

static void prepare_text(NativeText *output, const char *input) {
    size_t length = 0;
    while (input[length]) {
        output->bytes[length] = input[length];
        ++length;
    }
    output->bytes[length] = 0;
    output->length = output->capacity = (uint32_t)length;
    output->references = 1000000;
}

static void print_words(const uint32_t *words, size_t count) {
    static const char digits[] = "0123456789abcdef";
    char line[65 * 9];
    if (!count || count > 65) {
        system_exit(3);
    }
    for (size_t word = 0; word < count; ++word) {
        for (unsigned digit = 0; digit < 8; ++digit) {
            line[word * 9 + digit] = digits[(words[word] >> (28 - digit * 4)) & 15];
        }
        line[word * 9 + 8] = word + 1 < count ? ' ' : '\n';
    }
    write_bytes(line, count * 9);
}

#define LOAD(function, symbol)                                                         \
    do {                                                                               \
        *(void **)&function = dlsym(library, symbol);                                  \
        if (!function) {                                                               \
            system_exit(2);                                                            \
        }                                                                              \
    } while (0)

void android_composite_part_main(void) {
    void *library = dlopen("/data/local/tmp/ab520/libAngryBirdsClassic.so", 2);
    if (!library) {
        system_exit(2);
    }
    void *(*allocate)(size_t);
    void (*claim)(void *), (*release)(void *);
    void (*resources_open)(void *, void *), (*resources_close)(void *);
    void (*sheet_open)(void *);
    void *(*sprite_create)(void *, const void *, int, int, int, int, int, int, int);
    void *(*sprite_get)(void *, const void *);
    void (*add_sheet)(void *, const void *, void *);
    void (*stream_open)(void *, const void *, unsigned), (*stream_close)(void *);
    void *(*load_composites)(void *, void *, const void *);
    void *(*get_composite)(void *, const void *);
    LOAD(allocate, "_Znwj");
    LOAD(claim, "_ZN4lang6Object5claimEv");
    LOAD(release, "_ZN4lang6Object7releaseEv");
    LOAD(resources_open, "_ZN4game9ResourcesC2EPN2gr7ContextE");
    LOAD(resources_close, "_ZN4game9ResourcesD2Ev");
    LOAD(sheet_open, "_ZN4game11SpriteSheetC2Ev");
    LOAD(sprite_create,
         "_ZN4game11SpriteSheet12createSpriteERKSsiiiiiiNS_6Sprite14SourceRotationE");
    LOAD(sprite_get, "_ZNK4game11SpriteSheet9getSpriteERKSs");
    LOAD(add_sheet, "_ZN4game9Resources14addSpriteSheetERKSsPNS_11SpriteSheetE");
    LOAD(stream_open, "_ZN2io20ByteArrayInputStreamC2EPKvj");
    LOAD(stream_close, "_ZN2io20ByteArrayInputStreamD2Ev");
    LOAD(load_composites,
         "_ZN4game14SheetLoaderDAT16loadCompositeSetERN2io11InputStreamERKSt3mapISsN4la"
         "ng3PtrINS_11SpriteSheetEEESt4lessISsESaISt4pairIKSsS8_EEE");
    LOAD(get_composite, "_ZNK4game14CompoSpriteSet14getCompoSpriteERKSs");
    NativeText texts[7];
    const char *names[7];
    for (size_t index = 0; index < 7; ++index) {
        prepare_text(texts + index, index < 4    ? composite_part_sprites[index]
                                    : index == 4 ? "alpha"
                                    : index == 5 ? "zulu"
                                                 : "composite");
        names[index] = texts[index].bytes;
    }
    static const char header[] = "AB_ANDROID_COMPOSITE_PART_V1 4096\n";
    write_bytes(header, sizeof(header) - 1);
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        uint32_t input[15];
        composite_part_case(sample, input);
        print_words(input, 15);
        uint8_t data[256];
        size_t size = composite_part_data(input, data);
        uint64_t resources[192] = {0}, stream[160] = {0};
        void *sheets[2], *sprites[2];
        resources_open(resources, NULL);
        for (size_t index = 0; index < 2; ++index) {
            sheets[index] = allocate(128);
            if (!sheets[index]) {
                system_exit(4);
            }
            sheet_open(sheets[index]);
            claim(sheets[index]);
            (void)sprite_create(sheets[index], names + input[2], 0, 0,
                                (int)input[4] + (index ? 17 : 0),
                                (int)input[5] + (index ? 31 : 0),
                                (int)(int32_t)input[6] + (index ? 3 : 0),
                                (int)(int32_t)input[7] - (index ? 4 : 0), 0);
            sprites[index] = sprite_get(sheets[index], names + input[2]);
        }
        for (size_t order = 0; order < 2; ++order) {
            size_t index = order ^ ((input[1] >> 1) & 1u);
            size_t key = 4 + (index ^ (input[1] & 1u));
            add_sheet(resources, names + key, sheets[index]);
        }
        stream_open(stream, data, (unsigned)size);
        uint32_t *set =
            load_composites(NULL, stream, (const uint8_t *)resources + 1124);
        if (!set) {
            system_exit(5);
        }
        if (!set[1]) {
            claim(set);
        } else if (((const uint32_t *)(uintptr_t)set[1])[0] != 1) {
            system_exit(5);
        }
        uint32_t *composite = get_composite(set, names + 6);
        if (!composite || composite[3] - composite[2] != 4) {
            system_exit(6);
        }
        const uint32_t *entry = *(const uint32_t *const *)(uintptr_t)composite[2];
        uint32_t output[13] = {0};
        for (uint32_t index = 0; index < 2; ++index) {
            if (entry[3] == (uint32_t)(uintptr_t)sprites[index]) {
                output[0] = index + 1;
            }
        }
        if (!output[0]) {
            system_exit(6);
        }
        for (size_t field = 0; field < 7; ++field) {
            output[field + 1] = entry[field + 4];
        }
        output[8] = ((const uint8_t *)entry)[44];
        for (size_t field = 0; field < 4; ++field) {
            output[field + 9] = composite[field + 11];
        }
        print_words(output, 13);
        const char *name = (const char *)(uintptr_t)entry[2];
        uint32_t length = ((const uint32_t *)(const void *)name)[-3];
        uint32_t name_words[65] = {length};
        if (length > 64) {
            system_exit(7);
        }
        for (size_t byte = 0; byte < length; ++byte) {
            name_words[byte + 1] = (uint8_t)name[byte];
        }
        print_words(name_words, length + 1);
        release(set);
        stream_close(stream);
        resources_close(resources);
        for (size_t index = 0; index < 2; ++index) {
            release(sheets[index]);
        }
    }
    static const char complete[] = "AB composite part reference complete\n";
    write_bytes(complete, sizeof(complete) - 1);
    system_exit(0);
}

#undef LOAD
