#include "ab/bird_bubbles.h"

#include <math.h>
#include <stdio.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Bubble boundary failed at %d: %s\n", __LINE__,            \
                    #condition);                                                       \
            return 1;                                                                  \
        }                                                                              \
    } while (0)

int main(void) {
    bool inside, cleanup, apply;
    float next;
    AbVec2 force = {123, 456};
    AbBirdBubbleCandidate candidate = {.relative = {7, 0}, .radius = 7};
    CHECK(ab_bird_bubble_candidate(candidate, &inside, NULL) && !inside);
    candidate.relative.x = nextafterf(7, 0);
    CHECK(ab_bird_bubble_candidate(candidate, &inside, NULL) && inside);
    candidate.controllable = true;
    CHECK(ab_bird_bubble_candidate(candidate, &inside, NULL) && !inside);
    candidate.shot = true;
    CHECK(ab_bird_bubble_candidate(candidate, &inside, NULL) && inside);
    candidate.puffer = true;
    CHECK(ab_bird_bubble_candidate(candidate, &inside, NULL) && !inside);
    candidate.puffer = false;
    candidate.flying = true;
    CHECK(ab_bird_bubble_candidate(candidate, &inside, NULL) && !inside);
    candidate.controllable = false;
    CHECK(ab_bird_bubble_candidate(candidate, &inside, NULL) && inside);
    candidate.excluded_material = true;
    candidate.relative.x = NAN;
    CHECK(ab_bird_bubble_candidate(candidate, &inside, NULL) && !inside);
    candidate.excluded_material = false;
    CHECK(!ab_bird_bubble_candidate(candidate, &inside, NULL));
    CHECK(ab_bird_bubble_timer(.01f, .1f, .5f, &next, &cleanup, NULL) && !cleanup &&
          next < 0);
    CHECK(ab_bird_bubble_timer(next, .1f, .5f, &next, &cleanup, NULL) && cleanup);
    CHECK(ab_bird_bubble_timer(0, 0, 0, &next, &cleanup, NULL) && cleanup);
    CHECK(ab_bird_bubble_timer(2, 1, 0, &next, &cleanup, NULL) && next == 2 &&
          !cleanup);
    CHECK(!ab_bird_bubble_timer(2, INFINITY, 1, &next, &cleanup, NULL));
    CHECK(ab_bird_bubble_force((AbVec2){1000, -10}, false, NAN, NAN, NAN, &force,
                               &apply, NULL) &&
          !apply && force.x == 123);
    CHECK(ab_bird_bubble_force((AbVec2){1000, nextafterf(-10, 0)}, false, 2, 3, 20,
                               &force, &apply, NULL) &&
          apply && force.x == 6 && force.y == -108);
    CHECK(ab_bird_bubble_force((AbVec2){6, 8}, true, 2, 3, 20, &force, &apply, NULL) &&
          !apply);
    CHECK(ab_bird_bubble_force((AbVec2){6, nextafterf(8, 0)}, true, 2, 3, 20, &force,
                               &apply, NULL) &&
          !apply);
    CHECK(ab_bird_bubble_force((AbVec2){6, 7.99f}, true, 2, 3, 20, &force, &apply,
                               NULL) &&
          apply);
    CHECK(!ab_bird_bubble_force((AbVec2){0, 0}, false, 0, INFINITY, 20, &force, &apply,
                                NULL));
    return 0;
}
