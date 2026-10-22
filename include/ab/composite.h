#ifndef AB_COMPOSITE_H
#define AB_COMPOSITE_H

#include "ab/base.h"

typedef struct {
    AbSpan sprite;
    AbSpan tag; /* Optional RVIO instance tag; KA3D leaves it empty. */
    int16_t x;
    int16_t y;
    uint32_t scale_x_bits;
    uint32_t scale_y_bits;
    uint32_t rotation_bits;
    bool flip_x;
    bool flip_y;
} AbCompositePart;

typedef struct {
    AbSpan name;
    int16_t x;
    int16_t y;
} AbCompositePoint;

typedef struct {
    AbSpan name;
    AbCompositePart *parts;
    size_t part_count;
    /* KA3D version-2 auxiliary records; the native loader discards them. */
    AbCompositePoint *points;
    size_t point_count;
} AbComposite;

typedef struct {
    bool extended; /* RVIO when true, KA3D otherwise. */
    uint16_t version;
    AbComposite *composites;
    size_t count;
} AbCompositeSet;

/* Owns arrays; all names borrow input. Float fields retain binary32 bits. */
bool ab_composite_read(AbSpan input, size_t allocation_limit, AbCompositeSet *output,
                       AbError *error);
void ab_composite_free(AbCompositeSet *set);

#endif
