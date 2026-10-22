#include "ab/screen_clip.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Screen clip failed at %d: %s\n", __LINE__, #condition);   \
            return 1;                                                                  \
        }                                                                              \
    } while (0)

int main(void) {
    AbDrawClip clip;
    CHECK(ab_screen_clip((AbScreenClipRect){-0.5f, -1.5f, 4, 5}, NULL, &clip, NULL));
    CHECK(clip.enabled && clip.left == -1 && clip.top == -2 && clip.right == 4 &&
          clip.bottom == 4);
    AbDrawScissor scissor;
    CHECK(ab_draw_clip_scissor(clip, 3, 3, &scissor, NULL) && scissor.x == 0 &&
          scissor.y == 0 && scissor.width == 3 && scissor.height == 3);
    AbVec2 offset = {0.5f, -0.5f};
    CHECK(ab_screen_clip((AbScreenClipRect){80.25f, 120.25f, 140, 80}, &offset, &clip,
                         NULL));
    CHECK(clip.left == 81 && clip.top == 120 && clip.right == 221 &&
          clip.bottom == 200);
    CHECK(ab_draw_clip_scissor(clip, 480, 320, &scissor, NULL) && scissor.x == 81 &&
          scissor.y == 120 && scissor.width == 140 && scissor.height == 80);
    CHECK(ab_screen_clip((AbScreenClipRect){600, 500, 10, 20}, NULL, &clip, NULL));
    CHECK(ab_draw_clip_scissor(clip, 480, 320, &scissor, NULL) && !scissor.width &&
          !scissor.height && scissor.x == 480 && scissor.y == 320);
    CHECK(ab_screen_clip((AbScreenClipRect){0, 0, 480, 320}, &offset, &clip, NULL) &&
          clip.left == 1 && clip.top == -1 && clip.right == 481 && clip.bottom == 320);
    AbDrawClip saved = clip;
    CHECK(!ab_screen_clip((AbScreenClipRect){NAN, 0, 1, 1}, NULL, &clip, NULL) &&
          !memcmp(&clip, &saved, sizeof(clip)));
    CHECK(!ab_screen_clip((AbScreenClipRect){0x1p31f, 0, 1, 1}, NULL, &clip, NULL) &&
          !memcmp(&clip, &saved, sizeof(clip)));
    CHECK(!ab_screen_clip((AbScreenClipRect){0, 0, -1, 1}, NULL, &clip, NULL));
    CHECK(ab_draw_clip_scissor((AbDrawClip){0}, 480, 320, &scissor, NULL) &&
          scissor.x == 0 && scissor.y == 0 && scissor.width == 480 &&
          scissor.height == 320);
    puts("Screen offset, separately rounded edges and framebuffer intersection passed");
    return 0;
}
