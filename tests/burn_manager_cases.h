#ifndef AB_TEST_BURN_MANAGER_CASES_H
#define AB_TEST_BURN_MANAGER_CASES_H

#include <stdint.h>

enum { AB_BURN_MANAGER_CASES = 24 };

/* operation: add=0, update=1, reset=2, query=3. Second word is duration/time;
 * third is the update's unused seconds. One continuous original module state. */
static const uint32_t burn_manager_cases[AB_BURN_MANAGER_CASES][3] = {
    {3, 0x7fc12345, 0x7fc12345}, {2, 0x7fc12345, 0x7fc12345},
    {0, 0x40200000, 0x7fc12345}, {0, 0x3f800000, 0x7fc12345},
    {0, 0x40800000, 0x7fc12345}, {3, 0x7fc12345, 0x7fc12345},
    {1, 0x41200000, 0x7fc12345}, {1, 0x41600000, 0x7f800000},
    {3, 0x7fc12345, 0x7fc12345}, {1, 0x41600001, 0x00000000},
    {0, 0xbf800000, 0x7fc12345}, {1, 0xc0a00000, 0x80000000},
    {0, 0x80000000, 0x7fc12345}, {0, 0x3e000000, 0x7fc12345},
    {1, 0xbf800000, 0x3f800000}, {0, 0x3e800000, 0x7fc12345},
    {1, 0xbf600000, 0x00000000}, {0, 0x3e000000, 0x7fc12345},
    {0, 0x3e000000, 0x7fc12345}, {2, 0x7fc12345, 0x7fc12345},
    {0, 0x00000000, 0x7fc12345}, {1, 0x7fc12345, 0x7f800000},
    {3, 0x7fc12345, 0x7fc12345}, {2, 0x7fc12345, 0x7fc12345},
};

#endif
