#include "ab/scene_membership.h"
#include "ab/scene_id.h"

#include <string.h>

static bool valid_scene(const AbSceneMembership *scene) {
    return scene && scene->count <= scene->capacity &&
           scene->capacity <= SIZE_MAX / sizeof(*scene->items) &&
           (!scene->capacity || scene->items);
}

bool ab_scene_membership_insert(AbSceneMembership *scene, AbSceneMember member,
                                AbError *error) {
    if (!valid_scene(scene) || !member.object || scene->count == scene->capacity)
        return ab_fail(error, "Invalid or exhausted scene membership storage");
    size_t index = scene->count;
    while (index && scene->items[index - 1].layer > member.layer) {
        scene->items[index] = scene->items[index - 1];
        --index;
    }
    scene->items[index] = member;
    ++scene->count;
    return true;
}

static bool remove_member(AbSceneMembership *scene, size_t index,
                          AbSceneMember *removed, bool *found) {
    *found = index < scene->count;
    if (!*found)
        return true;
    *removed = scene->items[index];
    --scene->count;
    memmove(scene->items + index, scene->items + index + 1,
            (scene->count - index) * sizeof(*scene->items));
    return true;
}

bool ab_scene_membership_remove_id(AbSceneMembership *scene, int32_t id,
                                   AbSceneMember *removed, bool *found,
                                   AbError *error) {
    if (!valid_scene(scene) || !removed || !found)
        return ab_fail(error, "Invalid scene identifier removal storage");
    size_t index = 0;
    while (index < scene->count && scene->items[index].id != id)
        ++index;
    return remove_member(scene, index, removed, found);
}

bool ab_scene_membership_remove_object(AbSceneMembership *scene, void *object,
                                       int32_t layer, AbSceneMember *removed,
                                       bool *found, AbError *error) {
    if (!valid_scene(scene) || !object || !removed || !found)
        return ab_fail(error, "Invalid scene object removal storage");
    size_t index = 0;
    while (index < scene->count &&
           (scene->items[index].layer != layer || scene->items[index].object != object))
        ++index;
    return remove_member(scene, index, removed, found);
}

bool ab_scene_membership_dispose_number(AbSceneMembership *scene, float id,
                                        AbSceneMember *removed, bool *found,
                                        AbError *error) {
    return ab_scene_membership_remove_id(scene, ab_scene_id_integer(id), removed, found,
                                         error);
}
