/* Isolated Android reference tool. No original library code is linked into the
 * first-party runtime. Native source sets own their real maps; composite payloads
 * are controlled identity/reference-count boundaries, without drawing. */
#include "android_io.h"
#include "../../tests/sprite_registry_cases.h"

extern void *dlopen(const char *path, int mode);
extern void *dlsym(void *library, const char *name);

void __aeabi_memclr8(void *destination, size_t size) {
    volatile uint8_t *bytes = destination;
    for (size_t index = 0; index < size; ++index) {
        bytes[index] = 0;
    }
}

typedef struct {
    uint32_t length, capacity, references;
    char bytes[32];
} NativeText;

static NativeText names[5], owners[4];
static const char *name_pointers[5], *owner_pointers[4];

static void print_words(const uint32_t *words, size_t count) {
    static const char digits[] = "0123456789abcdef";
    char line[24 * 9];
    if (count > 24) {
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

static void prepare_text(NativeText *text, const char *input) {
    size_t length = 0;
    while (input[length]) {
        if (length + 1 == sizeof(text->bytes)) {
            system_exit(4);
        }
        text->bytes[length] = input[length];
        ++length;
    }
    text->bytes[length] = 0;
    text->length = text->capacity = (uint32_t)length;
    text->references = 1000000;
}

static uint32_t owner_id(const char *text) {
    uint32_t length = ((const uint32_t *)(const void *)text)[-3];
    for (uint32_t owner = 0; owner < 4; ++owner) {
        if (length != owners[owner].length) {
            continue;
        }
        size_t byte = 0;
        while (byte < length && text[byte] == owners[owner].bytes[byte]) {
            ++byte;
        }
        if (byte == length) {
            return owner + 1;
        }
    }
    system_exit(5);
}

#define LOAD(function, symbol)                                                         \
    do {                                                                               \
        *(void **)&function = dlsym(library, symbol);                                  \
        if (!function) {                                                               \
            system_exit(2);                                                            \
        }                                                                              \
    } while (0)

void android_sprite_registry_main(void) {
    void *library = dlopen("/data/local/tmp/ab520/libAngryBirdsClassic.so", 2);
    if (!library) {
        system_exit(2);
    }
    void (*resources_open)(void *, void *), (*resources_close)(void *);
    void (*sheet_open)(void *), (*sheet_close)(void *);
    void *(*sprite_create)(void *, const void *, int, int, int, int, int, int, int);
    void *(*sprite_get)(void *, const void *);
    void (*composites_open)(void *), (*composites_close)(void *);
    void (*composite_add)(void *, const void *, void *);
    void (*register_sheet)(void *, void *, const void *);
    void (*register_composites)(void *, void *, const void *);
    void (*remove_sheet)(void *, void *), (*remove_composites)(void *, void *);
    const uint32_t *(*find)(void *, const void *, uint32_t);
#if defined(AB_NATIVE_RESOURCE_OWNERSHIP)
    void *(*allocate)(size_t);
    void (*claim)(void *), (*release)(void *);
    void (*add_sheet)(void *, const void *, void *);
    void (*add_composites)(void *, const void *, void *);
    void (*release_sheet)(void *, const void *, int);
    void (*release_composites)(void *, const void *);
    void *(*get_sheet)(void *, const void *);
    void *(*get_composites)(void *, const void *);
    LOAD(allocate, "_Znwj");
    LOAD(claim, "_ZN4lang6Object5claimEv");
    LOAD(release, "_ZN4lang6Object7releaseEv");
    LOAD(add_sheet, "_ZN4game9Resources14addSpriteSheetERKSsPNS_11SpriteSheetE");
    LOAD(add_composites,
         "_ZN4game9Resources17addCompoSpriteSetERKSsPNS_14CompoSpriteSetE");
    LOAD(release_sheet, "_ZN4game9Resources18releaseSpriteSheetERKSsb");
    LOAD(release_composites, "_ZN4game9Resources21releaseCompoSpriteSetERKSs");
    LOAD(get_sheet, "_ZNK4game9Resources14getSpriteSheetERKSs");
    LOAD(get_composites, "_ZNK4game9Resources17getCompoSpriteSetERKSs");
#endif
#if defined(AB_NATIVE_RESOURCE_CACHE)
    void *(*create_sheet)(void *, const void *, int, int);
    void *(*create_composites)(void *, const void *, int);
    LOAD(create_sheet, "_ZN4game9Resources17createSpriteSheetERKSsbb");
    LOAD(create_composites, "_ZN4game9Resources20createCompoSpriteSetERKSsb");
    NativeText cached_paths[2][2];
    static const char *const path_names[2][2] = {
        {"missing/base.dat", "missing/base.extra.dat"},
        {"missing/theme.dat", "missing/theme.extra.dat"}};
    for (size_t key = 0; key < 2; ++key) {
        for (size_t variant = 0; variant < 2; ++variant) {
            prepare_text(&cached_paths[key][variant], path_names[key][variant]);
        }
    }
#endif
    LOAD(resources_open, "_ZN4game9ResourcesC2EPN2gr7ContextE");
    LOAD(resources_close, "_ZN4game9ResourcesD2Ev");
    LOAD(sheet_open, "_ZN4game11SpriteSheetC2Ev");
    LOAD(sheet_close, "_ZN4game11SpriteSheetD2Ev");
    LOAD(sprite_create,
         "_ZN4game11SpriteSheet12createSpriteERKSsiiiiiiNS_6Sprite14SourceRotationE");
    LOAD(sprite_get, "_ZNK4game11SpriteSheet9getSpriteERKSs");
    LOAD(composites_open, "_ZN4game14CompoSpriteSetC2Ev");
    LOAD(composites_close, "_ZN4game14CompoSpriteSetD2Ev");
    LOAD(composite_add, "_ZN4game14CompoSpriteSet3addERKSsPNS_11CompoSpriteE");
    LOAD(register_sheet,
         "_ZN4game9Resources20addSpritesToRegistryEPNS_11SpriteSheetERKSs");
    LOAD(register_composites,
         "_ZN4game9Resources20addSpritesToRegistryEPNS_14CompoSpriteSetERKSs");
    LOAD(remove_sheet,
         "_ZN4game9Resources25removeSpritesFromRegistryEPNS_11SpriteSheetE");
    LOAD(remove_composites,
         "_ZN4game9Resources25removeSpritesFromRegistryEPNS_14CompoSpriteSetE");
    LOAD(find, "_ZNK4game9Resources10findSpriteERKSsNS0_11SpriteEntry10SpriteTypeE");
    void *composite_vtable = dlsym(library, "_ZTVN4game11CompoSpriteE");
    if (!composite_vtable) {
        system_exit(2);
    }
#if defined(AB_NATIVE_RESOURCE_OWNERSHIP)
    void *object_vtable = dlsym(library, "_ZTVN4lang6ObjectE");
    if (!object_vtable) {
        system_exit(2);
    }
#endif
    for (size_t index = 0; index < 5; ++index) {
        prepare_text(names + index, registry_names[index]);
        name_pointers[index] = names[index].bytes;
    }
    for (size_t index = 0; index < 4; ++index) {
        prepare_text(owners + index, registry_owners[index]);
        owner_pointers[index] = owners[index].bytes;
    }
#if defined(AB_NATIVE_RESOURCE_CACHE)
    static const char header[] = "AB_ANDROID_RESOURCE_CACHE_V1 4096\n";
#elif defined(AB_NATIVE_RESOURCE_OWNERSHIP)
    static const char header[] = "AB_ANDROID_RESOURCE_OWNERS_V1 4096\n";
#else
    static const char header[] = "AB_ANDROID_SPRITE_REGISTRY_V1 4096\n";
#endif
    write_bytes(header, sizeof(header) - 1);
    for (size_t test = 0; test < 4096; ++test) {
        uint64_t resources[192] = {0};
        void *sets[4];
#if defined(AB_NATIVE_RESOURCE_OWNERSHIP)
        uint32_t images[2][2] = {{0}}, image_references[2][2] = {{0}};
#endif
#if !defined(AB_NATIVE_RESOURCE_OWNERSHIP)
        uint64_t set_storage[4][16] = {{0}};
#endif
        uint32_t composites[2][3][2] = {{{0}}}, references[2][3][2] = {{{0}}};
        uint32_t input[24];
        void *payloads[4][3] = {{0}};
        registry_case(test, input);
        resources_open(resources, NULL);
        for (size_t set = 0; set < 4; ++set) {
#if defined(AB_NATIVE_RESOURCE_OWNERSHIP)
            sets[set] = allocate(128);
            if (!sets[set]) {
                system_exit(7);
            }
#else
            sets[set] = set_storage[set];
#endif
            if (set < 2) {
                sheet_open(sets[set]);
            } else {
                composites_open(sets[set]);
            }
#if defined(AB_NATIVE_RESOURCE_OWNERSHIP)
            claim(sets[set]);
#endif
            for (size_t item = 0; item < 3; ++item) {
                if (!(input[set] & (1u << item))) {
                    continue;
                }
                const char **name = name_pointers + input[4 + set * 3 + item];
                if (set < 2) {
                    (void)sprite_create(sets[set], name, 0, 0, 16, 16, 0, 0, 0);
                } else {
                    uint32_t *payload = composites[set - 2][item];
                    payload[0] = (uint32_t)(uintptr_t)composite_vtable + 8;
                    uint32_t *counts = references[set - 2][item];
                    counts[0] = counts[1] = 1000000;
                    payload[1] = (uint32_t)(uintptr_t)counts;
                    composite_add(sets[set], name, payload);
                    payloads[set][item] = payload;
                }
            }
            if (set < 2) {
                for (size_t item = 0; item < 3; ++item) {
                    if (input[set] & (1u << item)) {
                        payloads[set][item] = sprite_get(
                            sets[set], name_pointers + input[4 + set * 3 + item]);
                    }
                }
#if defined(AB_NATIVE_RESOURCE_OWNERSHIP)
                /* Image state is a retained Object identity, assigned after
                 * sprite construction. No image dimensions or drawing occur. */
                image_references[set][0] = image_references[set][1] = 1000000;
                images[set][0] = (uint32_t)(uintptr_t)object_vtable + 8;
                images[set][1] = (uint32_t)(uintptr_t)image_references[set];
                ((uint32_t *)sets[set])[3] = (uint32_t)(uintptr_t)images[set];
#endif
            }
        }
        print_words(input, 24);
        for (size_t step = 0; step < 8; ++step) {
            uint32_t command = input[16 + step], set = command & 3u;
#if defined(AB_NATIVE_RESOURCE_OWNERSHIP)
            const void *owner = owner_pointers + ((command >> 3) & 3u);
            if (command & 4u) {
                if (set < 2) {
                    release_sheet(resources, owner, (int)((command >> 5) & 1u));
                } else {
                    release_composites(resources, owner);
                }
            } else {
                (set < 2 ? add_sheet : add_composites)(resources, owner, sets[set]);
            }
            uint32_t owned[10] = {0};
            for (size_t key = 0; key < 4; ++key) {
                void *sheet = get_sheet(resources, owner_pointers + key);
                void *composites_set = get_composites(resources, owner_pointers + key);
                for (size_t source = 0; source < 4; ++source) {
                    if (sheet == sets[source]) {
                        owned[key * 2] = (uint32_t)source + 1;
                    }
                    if (composites_set == sets[source]) {
                        owned[key * 2 + 1] = (uint32_t)source + 1;
                    }
                }
            }
            owned[8] = ((const uint32_t *)sets[0])[3] != 0;
            owned[9] = ((const uint32_t *)sets[1])[3] != 0;
            print_words(owned, 10);
#if defined(AB_NATIVE_RESOURCE_CACHE)
            uint32_t cached[6] = {0};
            for (size_t key = 0; key < 2; ++key) {
                const char *path = cached_paths[key][test & 1u].bytes;
                for (size_t kind = 0; kind < 2; ++kind) {
                    uint32_t identity = owned[(key + 1) * 2 + kind];
                    if (!identity) {
                        continue;
                    }
                    /* Only cache hits are executed: the path does not exist and
                     * this Resources object has no file/context provider. */
                    void *result = kind ? create_composites(resources, &path, 0)
                                        : create_sheet(resources, &path, 0, 1);
                    if (result != sets[identity - 1]) {
                        system_exit(8);
                    }
                    cached[key * 2 + kind] = identity;
                }
            }
            cached[4] = ((const uint32_t *)sets[0])[3] != 0;
            cached[5] = ((const uint32_t *)sets[1])[3] != 0;
            print_words(cached, 6);
#endif
#else
            if (command & 4u) {
                (set < 2 ? remove_sheet : remove_composites)(resources, sets[set]);
            } else {
                (set < 2 ? register_sheet : register_composites)(
                    resources, sets[set], owner_pointers + ((command >> 3) & 3u));
            }
#endif
            for (size_t name = 0; name < 5; ++name) {
                uint32_t output[9] = {0};
                for (uint32_t kind = 0; kind < 3; ++kind) {
                    const uint32_t *entry = find(resources, name_pointers + name, kind);
                    if (!entry) {
                        continue;
                    }
                    output[kind * 3] = entry[0];
                    output[kind * 3 + 1] = owner_id((const char *)(uintptr_t)entry[1]);
                    uint32_t id = 0;
                    for (size_t source = 0; source < 4; ++source) {
                        for (size_t item = 0; item < 3; ++item) {
                            if (payloads[source][item] &&
                                entry[2] ==
                                    (uint32_t)(uintptr_t)payloads[source][item]) {
                                id = (uint32_t)(source * 3 + item + 1);
                            }
                        }
                    }
                    if (!id) {
                        system_exit(6);
                    }
                    output[kind * 3 + 2] = id;
                }
                print_words(output, 9);
            }
        }
        resources_close(resources);
        for (size_t set = 0; set < 4; ++set) {
#if defined(AB_NATIVE_RESOURCE_OWNERSHIP)
            release(sets[set]);
#else
            (set < 2 ? sheet_close : composites_close)(sets[set]);
#endif
        }
    }
#if defined(AB_NATIVE_RESOURCE_CACHE)
    static const char complete[] = "AB resource cache reference complete\n";
#elif defined(AB_NATIVE_RESOURCE_OWNERSHIP)
    static const char complete[] = "AB resource owners reference complete\n";
#else
    static const char complete[] = "AB sprite registry reference complete\n";
#endif
    write_bytes(complete, sizeof(complete) - 1);
    system_exit(0);
}

#undef LOAD
