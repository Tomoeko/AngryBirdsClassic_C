#include "ab/classic_pause_actions.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Pause action check failed at %d: %s\n", __LINE__,         \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static bool check(void) {
    const AbDataValue values[] = {
        {0},
        {.kind = AB_DATA_BOOLEAN},
        {.kind = AB_DATA_BOOLEAN, .bits = 1},
        {.kind = AB_DATA_NUMBER},
        {.kind = AB_DATA_NUMBER, .bits = UINT32_C(0x80000000)},
        {.kind = AB_DATA_NUMBER, .bits = UINT32_C(0x3fea3d71)},
        {.kind = AB_DATA_NUMBER, .bits = UINT32_C(0x7fc12345)}};
    for (size_t index = 0; index < sizeof(values) / sizeof(*values); ++index) {
        AbDataValue restarted = {.kind = AB_DATA_BOOLEAN, .bits = 1};
        AbDataValue zoom = values[index];
        AbDataValue typo = {.kind = AB_DATA_NUMBER, .bits = 1};
        AbClassicPauseActions actions = {0};
        CHECK(ab_classic_pause_actions_init(
                  &actions,
                  (AbClassicPauseActionConfig){.restarted_from = &restarted,
                                               .zoom_level = &zoom,
                                               .sotred_zoom_level = &typo},
                  NULL) &&
              actions.scaling_enabled && actions.stored_zoom.kind == AB_DATA_NIL &&
              typo.bits == 1 && restarted.kind == AB_DATA_BOOLEAN);
        CHECK(ab_classic_pause_scaling(&actions, false, NULL) &&
              !actions.scaling_enabled && actions.stored_zoom.kind == zoom.kind &&
              actions.stored_zoom.bits == zoom.bits && typo.bits == 1);
        zoom = (AbDataValue){.kind = AB_DATA_NUMBER, .bits = UINT32_C(0x40000000)};
        CHECK(ab_classic_pause_scaling(&actions, false, NULL) &&
              actions.stored_zoom.kind == values[index].kind &&
              actions.stored_zoom.bits == values[index].bits &&
              ab_classic_pause_scaling(&actions, true, NULL) &&
              actions.scaling_enabled && zoom.kind == values[index].kind &&
              zoom.bits == values[index].bits && typo.kind == AB_DATA_NIL);
        zoom = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1};
        CHECK(ab_classic_pause_scaling(&actions, true, NULL) &&
              zoom.kind == AB_DATA_BOOLEAN && zoom.bits == 1);
        CHECK(!ab_classic_pause_show(&actions, NULL) && actions.failed &&
              restarted.kind == AB_DATA_STRING && restarted.string.size == 10 &&
              !memcmp(restarted.string.data, "pause menu", 10) &&
              !ab_classic_pause_scaling(&actions, false, NULL));
    }
    return true;
}

int main(void) {
    return check() ? 0 : 1;
}
