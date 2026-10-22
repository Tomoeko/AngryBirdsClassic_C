#ifndef AB_ORACLE_RUBBERBAND_H
#define AB_ORACLE_RUBBERBAND_H

static uint32_t rubberband_calls, rubberband_points[12], rubberband_uv[8];

static void rubberband_image(void *image, void *context, const uint32_t *points,
                             const uint32_t *uv, const void *color, void *shader) {
    (void)image;
    (void)context;
    if (++rubberband_calls != 1 || color || shader) {
        fail("Unexpected native rubberband image boundary\n");
    }
    for (size_t index = 0; index < 12; ++index) {
        rubberband_points[index] = points[index];
    }
    for (size_t index = 0; index < 8; ++index) {
        rubberband_uv[index] = uv[index];
    }
}

typedef void (*RubberbandDraw)(void *, float, float, float, float, float, const void *);

typedef struct {
    uint32_t length, capacity, references;
    char bytes[32];
} RubberbandName;

typedef struct {
    RubberbandDraw draw;
    RubberbandName sprite_name, sheet_name;
    uint32_t name;
    uint32_t resources[0x4e0 / 4], game[21], context[1];
    uint32_t sheet[12], sprite[16], image[5], image_vtable[9];
    uint32_t cache_node[7], cache_entry[3], sheet_node[6], sprite_node[6];
} RubberbandFixture;

/* The tree pointers borrow this fixture's storage: never move it after setup. */
static void rubberband_prepare(RubberbandFixture *fixture) {
    *fixture = (RubberbandFixture){0};
    fixture->draw =
        (RubberbandDraw)find_function("_ZN7GameLua14drawRubberbandEfffffSs");
    void (*construct)(void *) = (void (*)(void *))find_function("_ZN4lang6ObjectC1Ev");
    void (*claim)(void *) = (void (*)(void *))find_function("_ZN4lang6Object5claimEv");
    fixture->sprite_name =
        (RubberbandName){sizeof("SLINGSHOT_PIXEL") - 1, sizeof("SLINGSHOT_PIXEL") - 1,
                         1000, "SLINGSHOT_PIXEL"};
    fixture->sheet_name = (RubberbandName){
        sizeof("band-sheet") - 1, sizeof("band-sheet") - 1, 1000, "band-sheet"};
    fixture->name = (uint32_t)(uintptr_t)fixture->sprite_name.bytes;
    construct(fixture->sheet);
    claim(fixture->sheet);
    construct(fixture->sprite);
    claim(fixture->sprite);
    construct(fixture->image);
    claim(fixture->image);
    fixture->image[0] = (uint32_t)(uintptr_t)fixture->image_vtable;
    fixture->image_vtable[8] = (uint32_t)(uintptr_t)rubberband_image;
    fixture->sheet[3] = (uint32_t)(uintptr_t)fixture->image;
    fixture->sheet[6] = (uint32_t)(uintptr_t)fixture->sprite_node;
    fixture->sprite[2] = (uint32_t)(uintptr_t)fixture->sheet;
    fixture->sprite[5] = UINT32_C(0x00100010);
    const uint32_t uv[] = {0, 0, 0x3f800000, 0, 0, 0x3f800000, 0x3f800000, 0x3f800000};
    for (size_t index = 0; index < 8; ++index) {
        fixture->sprite[8 + index] = uv[index];
    }
    fixture->cache_node[4] = fixture->name;
    fixture->cache_node[5] = (uint32_t)(uintptr_t)fixture->cache_entry;
    fixture->cache_node[6] = (uint32_t)(uintptr_t)(fixture->cache_entry + 3);
    fixture->cache_entry[0] = 1;
    fixture->cache_entry[1] = (uint32_t)(uintptr_t)fixture->sheet_name.bytes;
    fixture->cache_entry[2] = (uint32_t)(uintptr_t)fixture->sprite;
    fixture->resources[0x4cc / 4] = (uint32_t)(uintptr_t)fixture->cache_node;
    fixture->resources[0x46c / 4] = (uint32_t)(uintptr_t)fixture->sheet_node;
    fixture->sheet_node[4] = (uint32_t)(uintptr_t)fixture->sheet_name.bytes;
    fixture->sheet_node[5] = (uint32_t)(uintptr_t)fixture->sheet;
    fixture->sprite_node[4] = fixture->name;
    fixture->sprite_node[5] = (uint32_t)(uintptr_t)fixture->sprite;
    fixture->game[0x44 / 4] = (uint32_t)(uintptr_t)fixture->context;
    fixture->game[0x50 / 4] = (uint32_t)(uintptr_t)fixture->resources;
}

#if defined(AB_ORACLE_RUBBERBAND_ONLY)
static void probe_rubberband(void) {
    RubberbandFixture fixture;
    rubberband_prepare(&fixture);
    uint32_t random = UINT32_C(0x520ba7d0);
    semihost(4, "AB_ANDROID_RUBBERBAND_V1 4096\n");
    for (size_t item = 0; item < 4096; ++item) {
        FloatBits input[5];
        uint32_t words[5];
        for (size_t index = 0; index < 4; ++index) {
            input[index].number =
                (float)((int32_t)(random_word(&random) % 131073) - 65536) / 16.0f;
        }
        input[4].number =
            (float)((int32_t)(random_word(&random) % 65537) - 32768) / 128.0f;
        if (item % 13 == 0) {
            input[4].bits &= UINT32_C(0x80000000);
        }
        if (item % 17 == 0) {
            input[2] = input[0];
            input[3] = input[1];
            const uint32_t unused[] = {0x7fc12345, 0x7f800000, 0xff800000, 0x7fa00001,
                                       0x7f7fffff, 0x80000000, 0};
            input[4].bits = unused[(item / 17) % 7];
        } else if (item % 29 == 0) {
            input[0].bits = input[1].bits = 0;
            input[2].number = 0x1p-80f;
            input[3].number = -0x1p-80f;
            input[4].bits = random_word(&random);
        } else if (item % 31 == 0) {
            input[0].number = input[1].number = 0x1p100f;
            input[2].number = input[3].number = 0x1p101f;
        } else if (item % 43 == 0) {
            input[0].bits = input[1].bits = 0;
            input[2].number = 0x1p-70f;
            input[3].number = -0x1p-71f;
        }
        if (item % 37 == 0) {
            for (size_t index = 0; index < 4; ++index) {
                input[index].bits &= UINT32_C(0x80000000);
            }
        }
        for (size_t index = 0; index < 5; ++index) {
            words[index] = input[index].bits;
        }
        rubberband_calls = 0;
        fixture.draw(fixture.game, input[0].number, input[1].number, input[2].number,
                     input[3].number, input[4].number, &fixture.name);
        print_words(words, 5);
        print_words(&rubberband_calls, 1);
        if (rubberband_calls) {
            print_words(rubberband_points, 12);
            print_words(rubberband_uv, 8);
        }
    }
    semihost(4, "AB rubberband reference complete\n");
}
#endif

#endif
