#ifndef AB_REFERENCE_BLUES_CHILD_H
#define AB_REFERENCE_BLUES_CHILD_H

#include <stdint.h>

enum {
    BC_PROFILES = 16,
    BC_CHILDREN = 3,
    BC_PARENT_FIELDS = 26,
    BC_MAX_EVENTS = 8,
    BC_ARGUMENT_WORDS = 22,
    BC_CORE_WORDS = 17,
    BC_LINK_WORDS = 12
};

typedef struct {
    uint32_t kind, bits;
} BcValue;

static const char *const bc_parent_fields[BC_PARENT_FIELDS] = {
    "x",
    "y",
    "radius",
    "density",
    "friction",
    "restitution",
    "z_order",
    "definition",
    "controllable",
    "strength",
    "defence",
    "materialName",
    "levelGoal",
    "damageFactors",
    "spritePivotX",
    "spritePivotY",
    "damageSprite",
    "useLegacyCollisionPath",
    "xVel",
    "yVel",
    "angle",
    "collider",
    "velocityMultiplierModifier",
    "powerup_scale",
    "sprite",
    "name"};

/* String roles, not asset replacement: BIRD_BLUE is actual SmallBlueBird art;
 * the unused circle argument intentionally differs from damageSprite. */
static const char *const bc_strings[] = {"",
                                         "source_blue",
                                         "BIRD_BLUE",
                                         "unused_source_sprite",
                                         "SmallBlueBird",
                                         "light",
                                         "",
                                         "MuseumBluesTrajectoryRenderer",
                                         "circle"};

static const char *const bc_fields[] = {"name",
                                        "sprite",
                                        "type",
                                        "x",
                                        "y",
                                        "width",
                                        "height",
                                        "radius",
                                        "angle",
                                        "density",
                                        "friction",
                                        "restitution",
                                        "mass",
                                        "z_order",
                                        "definition",
                                        "controllable",
                                        "strength",
                                        "defence",
                                        "materialName",
                                        "levelGoal",
                                        "damageFactors",
                                        "spritePivotX",
                                        "spritePivotY",
                                        "damageSprite",
                                        "useLegacyCollisionPath",
                                        "shot",
                                        "sleeping",
                                        "hasCollided",
                                        "parentName",
                                        "xVel",
                                        "yVel",
                                        "collider",
                                        "velocityMultiplierModifier",
                                        "powerup_scale",
                                        "launchVelocity"};

#define BC_FIELD_COUNT (sizeof(bc_fields) / sizeof(*bc_fields))
#define BC_SNAPSHOT_WORDS (1 + BC_FIELD_COUNT * 2 + BC_CORE_WORDS + BC_LINK_WORDS)
#define BC_EVENT_WORDS (BC_ARGUMENT_WORDS + BC_SNAPSHOT_WORDS)

static BcValue bc_number(float value) {
    union {
        float value;
        uint32_t bits;
    } word = {.value = value};
    return (BcValue){3, word.bits};
}

/* Prepared parent/offset inputs only. Native and C implementations determine
 * all clone fields, body effects, callback order and constructor results. */
static void bc_case(unsigned profile, unsigned child, BcValue parent[BC_PARENT_FIELDS],
                    uint32_t offset[2], char suffix[4]) {
    for (unsigned index = 0; index < BC_PARENT_FIELDS; ++index)
        parent[index] = (BcValue){0};
    parent[0] = bc_number((float)((int)(profile * 7 % 31) - 15) / 4.0f);
    parent[1] = bc_number((float)((int)(profile * 11 % 31) - 15) / 4.0f);
    parent[2] = bc_number((float)(profile % 7 + 1) / 8.0f);
    parent[3] = bc_number(profile % 5 ? (float)(profile % 9 + 1) / 8.0f : 0);
    parent[4] = bc_number((float)(profile % 7) / 16.0f);
    parent[5] = bc_number((float)(profile % 5) / 8.0f);
    parent[6] = bc_number((float)((int)profile - 7) / 4.0f);
    parent[7] = (BcValue){4, 4};
    parent[8] = profile % 4 == 0   ? (BcValue){1, 1}
                : profile % 4 == 1 ? bc_number(0)
                : profile % 4 == 2 ? (BcValue){1, 0}
                                   : (BcValue){0};
    parent[9] = bc_number(10.0f + (float)profile / 4.0f);
    parent[10] = bc_number(2.0f + (float)profile / 8.0f);
    parent[11] = (BcValue){4, 5};
    parent[12] = profile % 3 == 0   ? (BcValue){0}
                 : profile % 3 == 1 ? (BcValue){1, 0}
                                    : bc_number(0);
    parent[13] = (BcValue){5, 1};
    parent[14] = bc_number((float)((int)profile - 8) / 16.0f);
    parent[15] = bc_number(profile & 1 ? -0.0f : 0.25f);
    parent[16] = (BcValue){4, 2};
    parent[17] = profile & 1 ? (BcValue){1, 1} : (BcValue){1, 0};
    parent[18] = bc_number((float)((int)(profile * 3 % 31) - 15) / 8.0f);
    parent[19] = bc_number((float)((int)(profile * 5 % 31) - 15) / 8.0f);
    parent[20] = bc_number((float)((int)profile - 8) / 4.0f);
    parent[21] = bc_number(3);
    parent[22] = profile & 1 ? bc_number((float)profile / 8.0f) : (BcValue){0};
    parent[23] = profile % 3 == 0   ? (BcValue){0}
                 : profile % 3 == 1 ? (BcValue){1, 0}
                                    : bc_number(1.25f);
    parent[24] = (BcValue){4, 3};
    parent[25] = (BcValue){4, 1};
    if (!profile) {
        parent[0] = bc_number(-0.0f);
        parent[1] = bc_number(0);
        parent[18] = bc_number(0);
        parent[19] = bc_number(-0.0f);
        parent[20] = bc_number(-0.0f);
    }
    const float directions[][2] = {{1, 0}, {0.6f, 0.8f}, {0, 1}, {-0.8f, 0.6f}};
    float sign = child == 0 ? -1.0f : child == 1 ? 0 : 1.0f;
    offset[0] = bc_number(child == 1 ? 0 : directions[profile % 4][0] * sign).bits;
    offset[1] = bc_number(child == 1 ? 0 : directions[profile % 4][1] * sign).bits;
    suffix[0] = (char)('a' + child);
    suffix[1] = (char)('0' + profile / 10);
    suffix[2] = (char)('0' + profile % 10);
    suffix[3] = 0;
}

#endif
