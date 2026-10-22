#ifndef AB_ORACLE_SCENE_MEMBERSHIP_H
#define AB_ORACLE_SCENE_MEMBERSHIP_H

#include "../../tests/scene_membership_cases.h"

static uint32_t membership_snapshot(const uint32_t *node, uint32_t objects[9][19],
                                    uint32_t *output, uint32_t count) {
    if (!node)
        return count;
    count = membership_snapshot((const uint32_t *)(uintptr_t)node[2], objects, output,
                                count);
    const uint32_t *begin = (const uint32_t *)(uintptr_t)node[5];
    const uint32_t *end = (const uint32_t *)(uintptr_t)node[6];
    while (begin != end) {
        if (count == 48)
            fail("Native scene membership observer capacity exceeded\n");
        uint32_t index = 0;
        while (index < 9 && *begin != (uint32_t)(uintptr_t)objects[index])
            ++index;
        if (index == 9)
            fail("Unknown native scene member\n");
        output[count++] = index;
        ++begin;
    }
    return membership_snapshot((const uint32_t *)(uintptr_t)node[3], objects, output,
                               count);
}

static void probe_scene_membership(void) {
    void (*construct)(void *, void *, void *) =
        (void (*)(void *, void *, void *))find_function(
            "_ZN3gfx5SceneC1ERN4game9ResourcesERN2gr7ContextE");
    void *(*layer_vector)(void *, const int32_t *) =
        (void *(*)(void *, const int32_t *))find_function(
            "_ZNSt3mapIiSt6vectorIN4lang3PtrIN3gfx16RenderableObjectEEESaIS5_"
            "EESt4lessIiESaISt4pairIKiS7_EEEixERSB_");
    void (*append)(void *, const void *) =
        (void (*)(void *, const void *))find_function(
            "_ZNSt6vectorIN4lang3PtrIN3gfx16RenderableObjectEEESaIS4_EE19_M_emplace_"
            "back_auxIJRKS4_EEEvDpOT_");
    uint32_t (*remove_id)(void *, int32_t) =
        (uint32_t (*)(void *, int32_t))find_function("_ZN3gfx5Scene6removeEi");
    uint32_t (*remove_object)(void *, const void *) =
        (uint32_t (*)(void *, const void *))find_function(
            "_ZN3gfx5Scene6removeERKNS_16RenderableObjectE");
    void (*dispose)(void *, float) = (void (*)(void *, float))find_function(
        "_ZN7GameLua23disposeLuaAssetRendererEf");
    void (*clear)(void *) = (void (*)(void *))find_function("_ZN3gfx5Scene5clearEv");
    semihost(4, "AB_ANDROID_SCENE_MEMBERSHIP_V1 12288\n");
    for (uint32_t sample = 0; sample < 256; ++sample) {
        uint32_t scene[8] = {0}, game[1637] = {0}, objects[9][19] = {{0}};
        uint32_t references[9][2];
        construct(scene, 0, 0);
        game[1636] = (uint32_t)(uintptr_t)scene;
        /* Controlled renderer identities have pinned references. Real map/vector
         * and Scene methods execute; renderer constructors/destructors do not. */
        for (uint32_t index = 0; index < 9; ++index) {
            references[index][0] = references[index][1] = 1;
            objects[index][1] = (uint32_t)(uintptr_t)references[index];
            objects[index][17] = (uint32_t)scene_membership_layer(sample, index);
            objects[index][18] = (uint32_t)scene_membership_id(sample, index);
        }
        uint32_t previous_count = 0;
        for (uint32_t step = 0; step < 48; ++step) {
            SceneMembershipOperation operation =
                scene_membership_operation(sample, step);
            uint32_t result = 0;
            if (operation.kind == 0) {
                int32_t layer = scene_membership_layer(sample, operation.object);
                void *vector = layer_vector(scene + 2, &layer);
                const void *object = objects[operation.object];
                append(vector, &object);
                result = 1;
            } else if (operation.kind == 1) {
                result = remove_id(scene, operation.id);
            } else if (operation.kind == 2) {
                result = remove_object(scene, objects[operation.object]);
            } else {
                dispose(game, operation.number);
            }
            uint32_t members[48];
            uint32_t count = membership_snapshot((const uint32_t *)(uintptr_t)scene[4],
                                                 objects, members, 0);
            if (operation.kind == 3)
                result = count < previous_count;
            FloatBits number = {.number = operation.number};
            uint32_t input[] = {operation.kind, operation.object,
                                (uint32_t)operation.id, number.bits};
            uint32_t output[] = {result, count};
            print_words(input, 4);
            print_words(output, 2);
            print_words(members, count);
            for (uint32_t index = 0; index < 9; ++index)
                print_words(references[index], 1);
            previous_count = count;
        }
        clear(scene);
        for (uint32_t index = 0; index < 9; ++index)
            if (references[index][0] != 1)
                fail("Native scene clear retained an unexpected reference\n");
    }
    semihost(4, "AB scene membership reference complete\n");
}

#endif
