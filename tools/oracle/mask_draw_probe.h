#ifndef AB_ORACLE_MASK_DRAW_H
#define AB_ORACLE_MASK_DRAW_H

#include "../../tests/mask_draw_cases.h"

static uint32_t mask_context_state[39];

static void *mask_draw_state(void *context) {
    (void)context;
    return mask_context_state;
}

static int32_t mask_draw_width(const uint32_t *object) {
    return (int32_t)object[1];
}

static int32_t mask_draw_height(const uint32_t *object) {
    return (int32_t)object[2];
}

static void *mask_draw_texture(const uint32_t *image) {
    return (void *)(uintptr_t)image[3];
}

static void probe_mask_draw(void) {
    void (*add)(void *, float, float, void *, void *) =
        (void (*)(void *, float, float, void *, void *))find_function(
            "_ZN11MaskedImage3addEffPN2gr5ImageEPN4game6SpriteE");
    uint32_t context_vtable[29] = {0}, image_vtable[17] = {0};
    uint32_t texture_vtable[7] = {0}, context[3] = {0};
    uint32_t image[4] = {0}, mask_image[4] = {0};
    uint32_t texture[3] = {0}, mask_texture[3] = {0}, sheet[4] = {0};
    uint32_t sprite[16] = {0};
    context_vtable[24] = (uint32_t)(uintptr_t)mask_draw_state;
    context_vtable[27] = (uint32_t)(uintptr_t)mask_draw_width;
    context_vtable[28] = (uint32_t)(uintptr_t)mask_draw_height;
    image_vtable[11] = texture_vtable[5] = (uint32_t)(uintptr_t)mask_draw_width;
    image_vtable[12] = texture_vtable[6] = (uint32_t)(uintptr_t)mask_draw_height;
    image_vtable[16] = (uint32_t)(uintptr_t)mask_draw_texture;
    context[0] = (uint32_t)(uintptr_t)context_vtable;
    image[0] = mask_image[0] = (uint32_t)(uintptr_t)image_vtable;
    texture[0] = mask_texture[0] = (uint32_t)(uintptr_t)texture_vtable;
    image[3] = (uint32_t)(uintptr_t)texture;
    mask_image[3] = (uint32_t)(uintptr_t)mask_texture;
    sheet[3] = (uint32_t)(uintptr_t)mask_image;
    sprite[2] = (uint32_t)(uintptr_t)sheet;
    semihost(4, "AB_ANDROID_MASK_DRAW_V1 2048\n");
    for (uint32_t sample = 0; sample < AB_MASK_DRAW_CASES; ++sample) {
        uint32_t input[AB_MASK_DRAW_INPUT_WORDS];
        mask_draw_case(sample, input);
        context[1] = input[0];
        context[2] = input[1];
        image[1] = texture[1] = input[2];
        image[2] = texture[2] = input[3];
        mask_image[1] = mask_texture[1] = input[4];
        mask_image[2] = mask_texture[2] = input[5];
        sprite[4] = input[6] | input[7] << 16;
        sprite[5] = input[8] | input[9] << 16;
        sprite[6] = input[10];
        sprite[7] = input[11];
        for (size_t word = 0; word < 39; ++word)
            mask_context_state[word] = 0;
        mask_context_state[4] = mask_context_state[7] = input[18];
        mask_context_state[5] = input[19];
        mask_context_state[6] = input[19] ^ UINT32_C(0x80000000);
        for (size_t word = 0; word < 4; ++word)
            mask_context_state[8 + word] = input[14 + word];
        mask_context_state[12] = input[21];
        mask_context_state[13] = input[22];
        mask_context_state[16] = input[23];
        uint32_t owner[15] = {0}, positions[24] = {0};
        uint32_t terrain_uv[24] = {0}, mask_uv[24] = {0};
        owner[2] = (uint32_t)(uintptr_t)context;
        uint32_t *arrays[] = {positions, terrain_uv, mask_uv};
        for (size_t array = 0; array < 3; ++array) {
            owner[6 + array * 3] = owner[7 + array * 3] =
                (uint32_t)(uintptr_t)arrays[array];
            owner[8 + array * 3] = (uint32_t)(uintptr_t)(arrays[array] + 24);
        }
        FloatBits x = {.bits = input[12]}, y = {.bits = input[13]};
        add(owner, x.number, y.number, image, sprite);
        print_words(input, AB_MASK_DRAW_INPUT_WORDS);
        uint32_t count = (owner[7] - owner[6]) / 16;
        if (count != 0 && count != 6)
            fail("Unexpected native mask vertex count\n");
        print_words(&count, 1);
        if (count) {
            print_words(positions, 24);
            print_words(terrain_uv, 24);
            print_words(mask_uv, 24);
        }
    }
    semihost(4, "AB mask draw reference complete\n");
}

#endif
