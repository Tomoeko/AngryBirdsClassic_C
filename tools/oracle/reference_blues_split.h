#ifndef AB_REFERENCE_BLUES_SPLIT_H
#define AB_REFERENCE_BLUES_SPLIT_H

#include "reference_blues_child.h"

enum { BS_PROFILES = 16, BS_MAX_STAGES = 40, BS_SUMMARY_WORDS = 24 };

static const char *const bs_parent_names[BS_PROFILES] = {
    "source_blue00", "source_blue01", "source_blue02", "source_blue03",
    "source_blue04", "source_blue05", "source_blue06", "source_blue07",
    "source_blue08", "source_blue09", "source_blue10", "source_blue11",
    "source_blue12", "source_blue13", "source_blue14", "source_blue15"};

/* Prepared parent fields only. Original utils10029 and source125163 determine
 * normalization/offsets, and real native circle/transform/class code determines
 * each resulting child. Threshold neighbors and signed zeros remain bit exact. */
static void bs_case(unsigned profile, BcValue parent[BC_PARENT_FIELDS]) {
    uint32_t unused_offset[2];
    char unused_suffix[4];
    bc_case(profile, 0, parent, unused_offset, unused_suffix);
    static const uint32_t velocities[BS_PROFILES][2] = {
        {0x00000000, 0x80000000}, {0x80000000, 0x00000000}, {0x358637bc, 0xb58637bc},
        {0x358637bd, 0x00000000}, {0x00000000, 0xb58637bd}, {0x358637be, 0x358637bd},
        {0x40400000, 0x40800000}, {0xc0400000, 0x40800000}, {0x40400000, 0xc0800000},
        {0xc0400000, 0xc0800000}, {0x4cbebc20, 0xcc3ebc20}, {0x3eaaaaab, 0x3e4ccccd},
        {0x3f800001, 0x3f7fffff}, {0x00000000, 0x3f800000}, {0xbf800000, 0x80000000},
        {0x3a83126f, 0xc4fa0000}};
    parent[18] = (BcValue){3, velocities[profile][0]};
    parent[19] = (BcValue){3, velocities[profile][1]};
}

#endif
