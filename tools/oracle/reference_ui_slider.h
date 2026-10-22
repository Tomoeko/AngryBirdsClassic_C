#ifndef AB_REFERENCE_UI_SLIDER_H
#define AB_REFERENCE_UI_SLIDER_H

/* Independently selected inputs for one original Slider.update invocation.
 * This header generates data only; the original method supplies the oracle. */
enum { REFERENCE_UI_SLIDER_CASES = 512 };

typedef struct {
    unsigned state, curve, direction, count;
    float padding_x, padding_y, gap, child_size;
    float extend, adjusted, disappear, screen_height, scale, scale_x, scale_y;
    float seconds;
    float offsets[3][2], overrides[3];
    unsigned override_kinds[3];
} ReferenceUiSlider;

static ReferenceUiSlider reference_ui_slider(unsigned index) {
    ReferenceUiSlider value = {
        .state = index % 6,
        .curve = index / 6 % 4,
        .direction = index / 24 % 4,
        .count = index % 3 + 1,
        .padding_x = 8.25f,
        .padding_y = 12.5f,
        .gap = (float)(index % 4) * 0.25f,
        .child_size = 20.5f,
        .extend = index % 7 ? (float)(index * 13 % 180) + 0.125f : 0,
        .adjusted = (float)(index % 11) * 0.5f,
        .disappear = (float)(index % 9 + 1) * 0.125f,
        .screen_height = index % 2 ? 768 : 800,
        .scale = index % 3 ? 1.5f : 0.75f,
        .scale_x = index % 2 ? 1.25f : 0.5f,
        .scale_y = index % 3 ? 0.75f : 2,
    };
    const float seconds[] = {0, 0.015625f, 0.25f, -0.03125f, 0.0625f};
    value.seconds = seconds[index % 5];
    for (unsigned child = 0; child < 3; ++child) {
        value.offsets[child][0] = (float)((index + child) % 7) * 0.25f;
        value.offsets[child][1] = -(float)((index + child) % 5) * 0.125f;
        value.override_kinds[child] = (index + child) % 3;
        value.overrides[child] = index % 2 ? 0 : 7.75f;
    }
    return value;
}

#endif
