#ifndef AB_SCENE_MEMBERSHIP_H
#define AB_SCENE_MEMBERSHIP_H

#include "ab/base.h"

typedef struct {
    void *object;
    int32_t id, layer;
} AbSceneMember;

typedef struct {
    AbSceneMember *items;
    size_t count, capacity;
} AbSceneMembership;

/* Bounded membership projection of gfx::Scene's ascending signed layer map and
 * each layer's insertion vector. Caller owns disjoint stable storage and borrowed
 * object identities. Duplicate IDs and duplicate identities are allowed. Insertion
 * appends within its layer. Removing preserves the order of remaining members.
 * No allocation, object retain/release callbacks or native layer-node storage.
 * Empty layers are not represented. Caller releases the removed object's scene
 * reference after success; other owners can still keep that renderer alive.
 * Invalid bindings/capacity preserve state and outputs. No concurrent mutation.
 */
bool ab_scene_membership_insert(AbSceneMembership *scene, AbSceneMember member,
                                AbError *error);

/* Remove the first matching ID in draw order, or first identical object in the
 * supplied current layer. Missing matches set found=false and preserve removed.
 * Successful matches set found=true and publish removed. Output pointers are
 * required and must not alias scene storage. */
bool ab_scene_membership_remove_id(AbSceneMembership *scene, int32_t id,
                                   AbSceneMember *removed, bool *found, AbError *error);
bool ab_scene_membership_remove_object(AbSceneMembership *scene, void *object,
                                       int32_t layer, AbSceneMember *removed,
                                       bool *found, AbError *error);

/* GameLua.disposeLuaAssetRenderer converts its binary32 argument with the native
 * signed ARM VCVT truncation/saturation before Scene.remove(int). NaN maps to
 * zero, positive overflow to INT32_MAX and negative overflow to INT32_MIN.
 * This preserves rounded-ID aliases; it does not recover the original integer.
 */
bool ab_scene_membership_dispose_number(AbSceneMembership *scene, float id,
                                        AbSceneMember *removed, bool *found,
                                        AbError *error);

#endif
