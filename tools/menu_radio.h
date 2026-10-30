#ifndef AB_MENU_RADIO_H
#define AB_MENU_RADIO_H

#include "ab/rotation.h"
#include "ab/script_math.h"

typedef struct {
    RadioScene *owner;
    AbUiWidget widget;
    char name[32], up[40], down_image[40];
    size_t index;
    bool down, hold, pressed;
} RadioButton;

struct RadioScene {
    PreviewMinigame base;
    AbTargetData definition;
    AbDataValue menu_song;
    AbUiWidget radio, wheel, bird, shadow, indicator;
    RadioButton buttons[4];
    float position, channel, target, direction, last_angle;
    float current_channel, last_button, bird_phase, bird_y, indicator_x[2];
    enum { RADIO_IDLE, RADIO_DRAGGING, RADIO_AUTOSPINNING } state;
    bool channel_present, current_present, bird_active;
};

static const float radio_pi = 0x1.921fb6p+1f;

static bool radio_song(RadioScene *owner, float channel, AbDataValue *output,
                       AbError *error) {
    const float keys[] = {1, 2, 3, 3.5f, 4};
    const char *const songs[] = {NULL, "ambient_theme1", "ambient_theme2",
                                 "funky_theme", "ambient_theme3"};
    for (size_t index = 0; index < 5; ++index) {
        if (channel != keys[index])
            continue;
        *output = index ? menu_data(songs[index]) : owner->menu_song;
        return output->kind == AB_DATA_STRING ||
               ab_fail(error, "Radio requires its actual main-menu song");
    }
    return ab_fail(error, "Unknown radio channel");
}

static bool radio_sound(RadioScene *owner, AbDataValue song, float volume, bool loop,
                        uint32_t track, AbError *error) {
    uint32_t handle;
    if (song.kind != AB_DATA_STRING)
        return ab_fail(error, "Radio audio requires a resource identifier");
    return ab_audio_bank_play_resource(owner->base.menu->audio, song.string, volume,
                                       loop, track, &handle, error);
}

static void radio_spin(RadioScene *owner, float target) {
    owner->target = target;
    owner->direction = target >= owner->position ? 1.0f : -1.0f;
    owner->state = RADIO_AUTOSPINNING;
}

static void radio_release(RadioScene *owner) {
    /* Original five-key constructor traversal, measured in the target runtime.
     * Strict comparison makes this order observable at equal-distance ties. */
    const float order[] = {2, 3, 1, 4, 3.5f};
    float best = INFINITY, target = 1;
    for (size_t index = 0; index < 5; ++index) {
        float distance = fabsf(order[index] - owner->position);
        if (distance < best) {
            best = distance;
            target = order[index];
        }
    }
    radio_spin(owner, target);
}

static bool radio_wheel_geometry(RadioScene *owner, AbVec2 *center, float *width,
                                 AbError *error) {
    PreviewMenu *menu = owner->base.menu;
    AbVec2 bounds, pivot;
    if (!ab_ui_scaling_sprite(&menu->scaling, &menu->context.images,
                              menu_span("SOUNDBOARD_2_WHEEL"), false, &bounds, error) ||
        !menu->context.images.pivot(menu->context.images.context,
                                    menu_span("SOUNDBOARD_2_WHEEL"), &pivot, error))
        return false;
    /* The source combines scaled bounds with the unscaled resource pivot. */
    *center = (AbVec2){
        menu_real(owner->wheel.frame->fields.x.number_bits) - pivot.x + bounds.x * .5f,
        menu_real(owner->wheel.frame->fields.y.number_bits) - pivot.y + bounds.y * .5f};
    *width = bounds.x;
    return true;
}

static bool radio_wheel_pointer(void *context, AbUiWidget *widget, AbSpan event,
                                AbVec2 point, AbUiPointerReply *reply, AbError *error) {
    (void)widget;
    RadioScene *owner = context;
    *reply = (AbUiPointerReply){0};
    AbUiPointerValue name = {.kind = AB_UI_POINTER_TEXT, .text = event};
    bool press = menu_equal(name, "LPRESS") && owner->state == RADIO_IDLE;
    bool drag = menu_equal(name, "LHOLD") && owner->state == RADIO_DRAGGING;
    if (press || drag) {
        AbVec2 center;
        float width = 0;
        if (!radio_wheel_geometry(owner, &center, &width, error))
            return false;
        float dx = point.x - center.x, dy = point.y - center.y;
        float squared = dx * dx + dy * dy;
        float distance = (float)sqrt((double)squared);
        bool inside = distance >= width * .5f && distance <= width * 2.5f;
        float angle = 0;
        if ((drag || (press && inside)) &&
            !ab_script_angle_from_components(dy, dx, &angle))
            return ab_fail(error, "Invalid radio wheel pointer angle");
        if (press && inside) {
            owner->state = RADIO_DRAGGING;
            owner->last_angle = angle;
        }
        if (drag) {
            if (distance >= width * .5f) {
                float difference = angle - owner->last_angle;
                if (difference >= radio_pi)
                    difference -= radio_pi * 2;
                else if (difference <= -radio_pi)
                    difference += radio_pi * 2;
                owner->position += difference / (radio_pi * 2);
                if (owner->position < 1)
                    owner->position = 1;
                else if (owner->position > 4)
                    owner->position = 4;
            }
            owner->last_angle = angle;
        }
    }
    if (menu_equal(name, "LRELEASE") && owner->state == RADIO_DRAGGING)
        radio_release(owner);
    return true;
}

static bool radio_wheel_update(void *context, AbUiWidget *widget, float seconds,
                               float time, AbError *error) {
    RadioScene *owner = context;
    if (!ab_ui_widget_update_base(widget, seconds, time, error))
        return false;
    if (owner->state == RADIO_AUTOSPINNING) {
        owner->position += (seconds * owner->direction) * 4;
        owner->position = owner->direction < 0 ? fmaxf(owner->position, owner->target)
                                               : fminf(owner->position, owner->target);
        if (owner->position == owner->target) {
            owner->state = RADIO_IDLE;
            owner->channel = owner->position;
            owner->channel_present = true;
        }
    }
    widget->frame->fields.angle = menu_number((owner->position * radio_pi) * 2);
    return true;
}

static bool radio_wheel_lifecycle(void *context, AbUiWidget *widget,
                                  AbUiFrameMethod method, AbError *error) {
    RadioScene *owner = context;
    if (!ab_ui_widget_lifecycle_base(widget, method, error))
        return false;
    if (method == AB_UI_FRAME_ENTRY) {
        owner->state = RADIO_IDLE;
        owner->position = 1;
        owner->channel_present = false;
        widget->frame->fields.angle = menu_number(0);
    }
    return true;
}

static bool radio_button_pointer(void *context, AbUiWidget *widget, AbSpan event,
                                 AbVec2 point, AbUiPointerReply *reply,
                                 AbError *error) {
    RadioButton *button = context;
    *reply = (AbUiPointerReply){0};
    AbUiPointerValue name = {.kind = AB_UI_POINTER_TEXT, .text = event};
    if (!menu_equal(name, "LRELEASE"))
        return true;
    bool hit;
    if (!ab_ui_widget_image_hit(widget, point, &hit, error))
        return false;
    if (hit) {
        button->hold = button->pressed = true;
        button->down = !button->down;
        reply->values[0] = menu_text("CHANNEL_CLICKED");
        reply->values[1] = menu_number((float)button->index);
        reply->values[2] = (AbUiPointerValue){.kind = AB_UI_POINTER_OBJECT,
                                              .object = (uintptr_t)widget};
    }
    return true;
}

static bool radio_button_update(void *context, AbUiWidget *widget, float seconds,
                                float time, AbError *error) {
    (void)seconds;
    (void)time;
    RadioButton *button = context;
    PreviewMenu *menu = button->owner->base.menu;
    if (menu->context.left_button_held) {
        bool hit;
        if (!ab_ui_widget_image_hit(widget, menu->context.cursor, &hit, error))
            return false;
        button->hold |= hit;
    }
    return minigame_set_image(&button->owner->base, widget,
                              button->down ? button->down_image : button->up, error);
}

static bool radio_layout(RadioScene *owner, AbError *error) {
    PreviewMenu *menu = owner->base.menu;
    AbVec2 size, pivot, bird, shadow, wheel;
    if (!minigame_layout(&owner->base, error) ||
        !ab_ui_scaling_sprite(&menu->scaling, &menu->context.images,
                              menu_span("SOUNDBOARD_2_RADIO"), false, &size, error) ||
        !ab_ui_scaling_sprite(&menu->scaling, &menu->context.images,
                              menu_span("SOUNDBOARD_2_RADIO"), true, &pivot, error) ||
        !ab_ui_scaling_sprite(&menu->scaling, &menu->context.images,
                              menu_span("SOUNDBOARD_2_BIRD"), false, &bird, error) ||
        !ab_ui_scaling_sprite(&menu->scaling, &menu->context.images,
                              menu_span("SOUNDBOARD_2_BIRD_SHADOW"), false, &shadow,
                              error) ||
        !menu->context.images.bounds(menu->context.images.context,
                                     menu_span("SOUNDBOARD_2_WHEEL"), &wheel, error))
        return false;
    float x = menu->screen.x * .5f;
    float bottom = size.y - pivot.y;
    float y = menu->screen.y >= menu->screen.y * .5f + bottom * 1.2f
                  ? menu->screen.y * .5f
                  : menu->screen.y - bottom * 1.2f;
    AbUiWidget *aligned[] = {&owner->radio,
                             &owner->wheel,
                             &owner->buttons[0].widget,
                             &owner->buttons[1].widget,
                             &owner->buttons[2].widget,
                             &owner->buttons[3].widget};
    for (size_t index = 0; index < 6; ++index) {
        AbUiFrameFields *frame = &aligned[index]->frame->fields;
        frame->x = menu_number(x);
        frame->y = menu_number(y);
        if (!ab_ui_scaling_set_to(&menu->scaling, frame, error))
            return false;
    }
    owner->wheel.state.image.rotation_pivot_x = menu_number(wheel.x * .5f);
    owner->wheel.state.image.rotation_pivot_y = menu_number(wheel.y * .5f);
    owner->bird.frame->fields.x = menu_number(x + (size.x - pivot.x) - bird.x * .4f);
    owner->bird_y = y + bottom - bird.y * .4f;
    owner->bird.frame->fields.y = menu_number(owner->bird_y);
    owner->shadow.frame->fields.x = owner->bird.frame->fields.x;
    owner->shadow.frame->fields.y = menu_number(y + bottom + shadow.y * .5f);
    owner->indicator.frame->fields.y = menu_number((y - pivot.y) + size.y * .42f);
    const AbDataTable *positions = owner->definition.data.returned.table;
    float start = 0, end = 0;
    if (!minigame_number(positions, "x0", &start, error) ||
        !minigame_number(positions, "x1", &end, error))
        return false;
    owner->indicator_x[0] = (x - pivot.x) + size.x * start;
    owner->indicator_x[1] = (x - pivot.x) + size.x * end;
    owner->indicator.frame->fields.x = menu_number(owner->indicator_x[0]);
    return ab_ui_scaling_set_to(&menu->scaling, &owner->bird.frame->fields, error) &&
           ab_ui_scaling_set_to(&menu->scaling, &owner->shadow.frame->fields, error) &&
           ab_ui_scaling_set_to(&menu->scaling, &owner->indicator.frame->fields, error);
}

static bool radio_lifecycle(void *context, AbUiWidget *widget, AbUiFrameMethod method,
                            AbError *error) {
    RadioScene *owner = context;
    if (!minigame_lifecycle(&owner->base, widget, method, error))
        return false;
    if (method == AB_UI_FRAME_LAYOUT)
        return radio_layout(owner, error);
    if (method == AB_UI_FRAME_ENTRY) {
        owner->current_present = owner->bird_active = false;
        return true;
    }
    if (method != AB_UI_FRAME_EXIT || !owner->current_present ||
        owner->current_channel == 1)
        return true;
    AbDataValue song;
    bool playing;
    return radio_song(owner, owner->current_channel, &song, error) &&
           ab_audio_bank_is_playing(owner->base.menu->audio, song.string, &playing,
                                    error) &&
           (!playing ||
            ab_audio_bank_stop(owner->base.menu->audio, song.string, error));
}

static bool radio_pointer(void *context, AbUiWidget *widget, AbSpan event, AbVec2 point,
                          AbUiPointerReply *reply, AbError *error) {
    RadioScene *owner = context;
    bool handled;
    if (!ab_ui_widget_pointer_base(widget, event, point, reply, error) ||
        !minigame_controls(&owner->base, reply, &handled, error))
        return false;
    if (handled || !menu_equal(reply->values[0], "CHANNEL_CLICKED"))
        return true;
    if (reply->values[1].kind != AB_UI_POINTER_NUMBER)
        return ab_fail(error, "Radio button lost its channel metadata");
    float channel = menu_real(reply->values[1].number_bits);
    if (channel < 1 || channel > 4 || floorf(channel) != channel)
        return ab_fail(error, "Invalid radio button channel");
    radio_spin(owner, channel);
    if (owner->last_button != channel &&
        !radio_sound(owner, menu_data("button_radio"), 1, false, 0, error))
        return false;
    owner->last_button = channel;
    return true;
}

static bool radio_music_update(RadioScene *owner, AbError *error) {
    AbAudioBank *audio = owner->base.menu->audio;
    if (!owner->channel_present)
        return true;
    AbDataValue song;
    bool playing;
    if (owner->current_present && owner->current_channel == owner->channel)
        return radio_song(owner, owner->current_channel, &song, error) &&
               ab_audio_bank_is_playing(audio, song.string, &playing, error) &&
               (playing || radio_sound(owner, song, .8f, true, 7, error));
    if (owner->current_present) {
        if (!radio_song(owner, owner->current_channel, &song, error) ||
            !ab_audio_bank_is_playing(audio, song.string, &playing, error) ||
            (playing && !ab_audio_bank_stop(audio, song.string, error)))
            return false;
    }
    owner->current_present = true;
    owner->current_channel = owner->channel;
    if (!radio_song(owner, owner->current_channel, &song, error) ||
        !radio_sound(owner, song, .8f, true, 7, error))
        return false;
    bool changed = false;
    if (owner->current_channel == 3.5f) {
        size_t count = 0;
        for (size_t index = 0; index < 4; ++index)
            count += owner->buttons[index].down;
        if (count < 2) {
            for (size_t index = 0; index < 4; ++index)
                owner->buttons[index].down = index >= 2;
            changed = true;
        }
    } else {
        for (size_t index = 0; index < 4; ++index) {
            RadioButton *button = owner->buttons + index;
            bool selected = (float)(index + 1) == owner->current_channel;
            changed |= selected && !button->down;
            button->down = selected;
        }
    }
    return !changed ||
           radio_sound(owner, menu_data("button_radio"), 1, false, 0, error);
}

static bool radio_update(void *context, AbUiWidget *widget, float seconds, float time,
                         AbError *error) {
    RadioScene *owner = context;
    if (!ab_ui_widget_update_base(widget, seconds, time, error))
        return false;
    size_t pressed = 0, down = 0;
    for (size_t index = 0; index < 4; ++index)
        pressed += owner->buttons[index].pressed;
    for (size_t index = 0; index < 4; ++index) {
        RadioButton *button = owner->buttons + index;
        if (pressed && !button->hold)
            button->down = false;
        button->hold = button->pressed = false;
        down += button->down;
    }
    if (down == 2) {
        bool playing;
        if (!ab_audio_bank_is_playing(owner->base.menu->audio, menu_span("funky_theme"),
                                      &playing, error))
            return false;
        if (!playing)
            radio_spin(owner, 3.5f);
    }
    owner->indicator.frame->fields.x = menu_number(
        owner->indicator_x[0] +
        (owner->indicator_x[1] - owner->indicator_x[0]) * ((owner->position - 1) / 3));
    bool special = owner->channel_present && owner->channel == 3.5f;
    if (special && !owner->base.star_collected &&
        !minigame_collect(&owner->base, error))
        return false;
    if (special && !owner->bird_active) {
        owner->bird_active = true;
        owner->bird_phase = 0;
    }
    if (owner->bird_active) {
        owner->bird_phase += seconds * 5;
        if (special) {
            float phase = owner->bird_phase;
            owner->bird_phase =
                (float)((double)phase -
                        floor((double)(phase / radio_pi)) * (double)radio_pi);
        } else if (owner->bird_phase >= radio_pi) {
            owner->bird_active = false;
        }
    }
    if (owner->bird_active) {
        AbVec2 bounds;
        float sine;
        if (!owner->base.menu->context.images.bounds(
                owner->base.menu->context.images.context,
                menu_span("SOUNDBOARD_2_BIRD"), &bounds, error) ||
            !ab_script_sine(owner->bird_phase, &sine))
            return false;
        owner->bird.frame->fields.angle = menu_number(sine * .25f);
        owner->bird.frame->fields.y =
            menu_number(owner->bird_y - (sine * bounds.y) * .333f);
    } else {
        owner->bird.frame->fields.angle = menu_number(0);
        owner->bird.frame->fields.y = menu_number(owner->bird_y);
    }
    return radio_music_update(owner, error);
}

static bool radio_construct(PreviewMinigame *base, AbError *error) {
    RadioScene *owner = (RadioScene *)base;
    AbTargetData *data = &owner->definition;
    if (!minigame_script(
            base, "assets/data/scripts/menus/radio.lua",
            "35b9f73ae548e9212398c77375c43531c1627d556c89937e29bea094f8c176d9", data,
            error) ||
        !ab_lua_data_literal_table_range(&data->chunk, 32, 3, 2, 1024 * 1024,
                                         &data->data, error) ||
        data->data.returned.kind != AB_DATA_TABLE ||
        !minigame_widgets(base, "Radio", error) ||
        !minigame_image(base, &owner->radio, "radio", "SOUNDBOARD_2_RADIO", error) ||
        !minigame_image(base, &owner->wheel, "wheel", "SOUNDBOARD_2_WHEEL", error))
        return false;
    for (size_t index = 0; index < 4; ++index) {
        RadioButton *button = owner->buttons + index;
        button->owner = owner;
        button->index = index + 1;
        int name = snprintf(button->name, sizeof(button->name), "channel_button_%zu",
                            index + 1);
        int up = snprintf(button->up, sizeof(button->up), "SOUNDBOARD_2_BUTTON_UP_%zu",
                          index + 1);
        int down = snprintf(button->down_image, sizeof(button->down_image),
                            "SOUNDBOARD_2_BUTTON_DOWN_%zu", index + 1);
        if (name <= 0 || (size_t)name >= sizeof(button->name) || up <= 0 ||
            (size_t)up >= sizeof(button->up) || down <= 0 ||
            (size_t)down >= sizeof(button->down_image) ||
            !minigame_image(base, &button->widget, button->name, button->up, error))
            return ab_fail(error, "Invalid radio button construction");
        button->widget.methods = (AbUiWidgetMethods){.context = button,
                                                     .pointer = radio_button_pointer,
                                                     .update = radio_button_update};
    }
    if (!minigame_image(base, &owner->shadow, "bird_shadow", "SOUNDBOARD_2_BIRD_SHADOW",
                        error) ||
        !minigame_image(base, &owner->bird, "bird", "SOUNDBOARD_2_BIRD", error) ||
        !minigame_image(base, &owner->indicator, "indicator", "SOUNDBOARD_2_INDICATOR",
                        error) ||
        !minigame_widget_controls(base, error))
        return false;
    owner->wheel.methods = (AbUiWidgetMethods){.context = owner,
                                               .lifecycle = radio_wheel_lifecycle,
                                               .pointer = radio_wheel_pointer,
                                               .update = radio_wheel_update};
    base->widget.methods = (AbUiWidgetMethods){.context = owner,
                                               .lifecycle = radio_lifecycle,
                                               .pointer = radio_pointer,
                                               .update = radio_update};
    return true;
}

static void radio_dispose(PreviewMinigame *base) {
    RadioScene *owner = (RadioScene *)base;
    ab_target_data_close(&owner->definition);
    free(owner);
}

static bool radio_new(PreviewMenu *menu, AbError *error) {
    RadioScene *owner = calloc(1, sizeof(*owner));
    if (!owner)
        return ab_fail(error, "Cannot allocate radio scene");
    owner->base = (PreviewMinigame){.menu = menu,
                                    .name = menu_data("RADIO"),
                                    .location = "radio",
                                    .groups = {"GOLDEN_EGGS", "BACKGROUNDS"},
                                    .group_count = 2,
                                    .construct = radio_construct,
                                    .dispose = radio_dispose};
    owner->menu_song = menu->main_song;
    menu->minigame = &owner->base;
    return true;
}

#endif
