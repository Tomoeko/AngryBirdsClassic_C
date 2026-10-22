#ifndef AB_TEST_OBJECT_MISSING_CASES_H
#define AB_TEST_OBJECT_MISSING_CASES_H

#include <stdint.h>

static const char *const missing_definition_names[] = {"BP_ROCKET_2",
                                                       "DecorationSiegeLongHorizontal",
                                                       "EmptyPlaceholderBird",
                                                       "DecorationFaceLift01Medium",
                                                       "DecorationFaceLift02Small",
                                                       "missing",
                                                       "unknown_fixture",
                                                       "MissingDefinition"};

static void object_missing_case(uint32_t sample, uint32_t input[3]) {
    input[0] = sample;
    input[1] = sample % 8;
    input[2] = (sample / 8) % 6;
}

#endif
