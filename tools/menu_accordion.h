#ifndef AB_MENU_ACCORDION_H
#define AB_MENU_ACCORDION_H

#include "ab/ui_toggle_button.h"

typedef struct {
    AccordionScene *owner;
    AbUiWidget widget;
    char name[24], up[32], down[32];
    size_t index;
} AccordionButton;

struct AccordionScene {
    PreviewMinigame base;
    AbTargetData definition;
    AbUiWidget ripped, bellows, left, handle;
    AbUiToggleButton pig;
    AbDataTable pig_states;
    AbDataEntry pig_images[2];
    AccordionButton buttons[5];
    AbDataValue note;
    float bellows_x, width, maximum_width, handle_x;
    float start_x, last_x, direction, rip_timer, rip_x;
    float volume, volume_timer, previous_x;
    enum { ACCORDION_IDLE, ACCORDION_DRAGGING, ACCORDION_RIPPING } state;
    bool dragging, direction_present, direction_changed, sound_muted;
};

static bool accordion_stop(AccordionScene *owner, AbError *error) {
    return ab_audio_mixer_all_action(owner->base.menu->mixer, AB_AUDIO_STOP, error);
}

static bool accordion_sound(AccordionScene *owner, const char *group, float volume,
                            AbError *error) {
    AbAudioBank *audio = owner->base.menu->audio;
    AbDataValue selected;
    uint32_t handle;
    return ab_audio_bank_name_value(audio, menu_data(group), &selected, error) &&
           selected.kind == AB_DATA_STRING &&
           ab_audio_bank_play_resource(audio, selected.string, volume, false, 0,
                                       &handle, error);
}

static bool accordion_tracks(AccordionScene *owner, AbError *error) {
    if (owner->note.kind != AB_DATA_STRING)
        return ab_fail(error, "Accordion requires its selected note");
    const char *const suffixes[] = {"_left", "_right"};
    for (size_t index = 0; index < 2; ++index) {
        char name[64];
        AbSpan note = owner->note.string;
        size_t suffix = strlen(suffixes[index]);
        if (note.size >= sizeof(name) - suffix)
            return ab_fail(error, "Accordion note identifier exceeds storage");
        memcpy(name, note.data, note.size);
        memcpy(name + note.size, suffixes[index], suffix + 1);
        uint32_t handle;
        if (!ab_audio_bank_play_resource(owner->base.menu->audio, menu_span(name), 1,
                                         true, (uint32_t)index + 5, &handle, error))
            return false;
    }
    return true;
}

static bool accordion_volume(AccordionScene *owner, uint32_t track, float volume,
                             AbError *error) {
    AbAudioMixer *mixer = owner->base.menu->mixer;
    float music = ab_audio_mixer_view(mixer).track_gains[7];
    return ab_audio_mixer_track_gain(mixer, track, fminf(volume, music), error);
}

static bool accordion_button_pointer(void *context, AbUiWidget *widget, AbSpan event,
                                     AbVec2 point, AbUiPointerReply *reply,
                                     AbError *error) {
    AccordionButton *button = context;
    *reply = (AbUiPointerReply){0};
    if (!menu_equal((AbUiPointerValue){.kind = AB_UI_POINTER_TEXT, .text = event},
                    "LPRESS"))
        return true;
    bool hit;
    if (!ab_ui_widget_image_hit(widget, point, &hit, error))
        return false;
    if (!hit)
        return true;
    if (!minigame_set_image(&button->owner->base, widget, button->down, error))
        return false;
    reply->values[0] = menu_text("PLAY");
    reply->values[1] = menu_number((float)button->index);
    reply->values[2] =
        (AbUiPointerValue){.kind = AB_UI_POINTER_OBJECT, .object = (uintptr_t)widget};
    return true;
}

static bool accordion_handle_pointer(void *context, AbUiWidget *widget, AbSpan event,
                                     AbVec2 point, AbUiPointerReply *reply,
                                     AbError *error) {
    AccordionScene *owner = context;
    AbUiPointerValue name = {.kind = AB_UI_POINTER_TEXT, .text = event};
    *reply = (AbUiPointerReply){0};
    if (menu_equal(name, "LPRESS") && owner->state == ACCORDION_IDLE) {
        bool hit;
        if (!ab_ui_widget_image_hit(widget, point, &hit, error))
            return false;
        if (hit) {
            owner->state = ACCORDION_DRAGGING;
            owner->start_x = owner->last_x = point.x;
            owner->dragging = true;
            owner->rip_timer = 0;
            owner->direction_present = false;
        }
    } else if (menu_equal(name, "LHOLD") && owner->state == ACCORDION_DRAGGING) {
        AbUiFrameFields *frame = &widget->frame->fields;
        float left = menu_real(frame->x.number_bits) -
                     menu_real(widget->state.image.resource_pivot_x.number_bits) *
                         menu_real(frame->scale_x.number_bits);
        if (point.x >= left) {
            owner->dragging = true;
            owner->last_x = point.x;
        }
    }
    return true;
}

static bool accordion_handle_update(void *context, AbUiWidget *widget, float seconds,
                                    float time, AbError *error) {
    AccordionScene *owner = context;
    AbUiFrameFields *handle = &widget->frame->fields;
    AbUiFrameFields *bellows = &owner->bellows.frame->fields;
    float scale;
    if (!ab_ui_widget_update_base(widget, seconds, time, error) ||
        !ab_ui_scaling_get(&owner->base.menu->scaling, &scale, error))
        return false;
    owner->direction_changed = false;
    if (!owner->dragging && owner->state == ACCORDION_DRAGGING)
        owner->state = ACCORDION_IDLE;
    if (owner->state == ACCORDION_IDLE) {
        float x = menu_real(handle->x.number_bits);
        float displacement = x - owner->handle_x;
        float movement = displacement > 0   ? (seconds * -owner->width) * 3
                         : displacement < 0 ? (seconds * owner->width) * 3
                                            : 0;
        x += movement;
        if ((movement > 0 && x > owner->handle_x) ||
            (movement < 0 && x < owner->handle_x))
            x = owner->handle_x;
        handle->x = menu_number(x);
        bellows->scale_x = menu_number(
            (((owner->width + (x - owner->handle_x)) / owner->width) * .65f) * scale);
    } else if (owner->state == ACCORDION_DRAGGING) {
        float offset = fmaxf(
            fminf(owner->last_x - owner->start_x, owner->maximum_width - owner->width),
            owner->width * -.8f);
        handle->x = menu_number(owner->handle_x + offset);
        if (fabsf(offset) > 0) {
            owner->direction_changed =
                !owner->direction_present || owner->direction * offset < 0;
            owner->direction = offset;
            owner->direction_present = true;
        }
        if (owner->last_x - owner->handle_x > owner->width * 1.55f)
            owner->rip_timer += seconds;
        else
            owner->rip_timer = 0;
        bellows->scale_x =
            menu_number((((owner->width + offset) / owner->width) * .65f) * scale);
        if (owner->rip_timer >= 3) {
            owner->dragging = false;
            owner->state = ACCORDION_RIPPING;
            owner->rip_x = 0;
            owner->ripped.frame->fields.visible =
                (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN, .boolean = true};
            bellows->scale_x = menu_number(scale);
            if (!accordion_stop(owner, error) ||
                !accordion_sound(owner, "accordion_break", 1, error))
                return false;
        }
    } else {
        owner->rip_x +=
            ((owner->base.menu->screen.x * .5f + owner->rip_x) * seconds) * 2;
        bellows->x = menu_number(owner->bellows_x + owner->rip_x);
        handle->x = menu_number(
            ((owner->handle_x + owner->maximum_width) - owner->width) + owner->rip_x);
    }
    owner->dragging = false;
    return true;
}

static bool accordion_layout(AccordionScene *owner, AbError *error) {
    PreviewMenu *menu = owner->base.menu;
    AbVec2 left, bellows, pivot, handle, handle_pivot, pig;
    if (!minigame_layout(&owner->base, error) ||
        !ab_ui_scaling_sprite(&menu->scaling, &menu->context.images,
                              menu_span("ACCO_LEFT"), true, &left, error) ||
        !ab_ui_scaling_sprite(&menu->scaling, &menu->context.images,
                              menu_span("ACCO_MID"), false, &bellows, error) ||
        !ab_ui_scaling_sprite(&menu->scaling, &menu->context.images,
                              menu_span("ACCO_MID"), true, &pivot, error) ||
        !ab_ui_scaling_sprite(&menu->scaling, &menu->context.images,
                              menu_span("ACCO_RIGHT"), false, &handle, error) ||
        !ab_ui_scaling_sprite(&menu->scaling, &menu->context.images,
                              menu_span("ACCO_RIGHT"), true, &handle_pivot, error) ||
        !ab_ui_scaling_sprite(&menu->scaling, &menu->context.images,
                              menu_span("GE_GRANDPA_01"), false, &pig, error))
        return false;
    float width = ((left.x + bellows.x) - pivot.x) + handle.x - handle_pivot.x;
    owner->bellows_x = (menu->screen.x * .5f - .5f * width) + left.x;
    owner->maximum_width = bellows.x - pivot.x;
    owner->width = owner->maximum_width * .65f;
    if (owner->width <= 0)
        return ab_fail(error, "Invalid actual accordion bellows width");
    owner->handle_x = owner->bellows_x + owner->width;
    AbUiWidget *parts[] = {&owner->left, &owner->ripped, &owner->bellows,
                           &owner->handle};
    for (size_t index = 0; index < 4; ++index) {
        AbUiFrameFields *frame = &parts[index]->frame->fields;
        frame->x = menu_number(index == 3 ? owner->handle_x : owner->bellows_x);
        frame->y = menu_number(menu->screen.y * .5f);
        if (!ab_ui_scaling_set_to(&menu->scaling, frame, error))
            return false;
    }
    /* Source layout5321 overwrites the scaled bellows X with this literal. */
    owner->bellows.frame->fields.scale_x = menu_number(.65f);
    for (size_t index = 0; index < 5; ++index) {
        AbUiFrameFields *frame = &owner->buttons[index].widget.frame->fields;
        frame->x = menu_number(owner->bellows_x);
        frame->y = menu_number(menu->screen.y * .5f);
        if (!ab_ui_scaling_set_to(&menu->scaling, frame, error))
            return false;
    }
    AbUiFrameFields *frame = &owner->pig.widget.frame->fields;
    frame->x = menu_number(menu->screen.x - pig.x * .55f -
                           menu_real(menu_route_field(&menu->margin, "x").bits));
    frame->y = menu_number(menu->screen.y - pig.y * .55f -
                           menu_real(menu_route_field(&menu->margin, "y").bits));
    return ab_ui_scaling_set_to(&menu->scaling, frame, error);
}

static bool accordion_lifecycle(void *context, AbUiWidget *widget,
                                AbUiFrameMethod method, AbError *error) {
    AccordionScene *owner = context;
    if (!minigame_lifecycle(&owner->base, widget, method, error))
        return false;
    if (method == AB_UI_FRAME_LAYOUT)
        return accordion_layout(owner, error);
    if (method == AB_UI_FRAME_ENTRY) {
        owner->volume = owner->volume_timer = 0;
        owner->note = menu_data("empty_accordion");
        owner->previous_x = menu_real(owner->handle.frame->fields.x.number_bits);
        return accordion_volume(owner, 5, 0, error) &&
               accordion_volume(owner, 6, 0, error);
    }
    if (method == AB_UI_FRAME_EXIT) {
        float music = ab_audio_mixer_view(owner->base.menu->mixer).track_gains[7];
        return accordion_volume(owner, 5, music, error) &&
               accordion_volume(owner, 6, music, error) && accordion_stop(owner, error);
    }
    return true;
}

static bool accordion_pointer(void *context, AbUiWidget *widget, AbSpan event,
                              AbVec2 point, AbUiPointerReply *reply, AbError *error) {
    AccordionScene *owner = context;
    bool handled;
    if (!ab_ui_widget_pointer_base(widget, event, point, reply, error) ||
        !minigame_controls(&owner->base, reply, &handled, error))
        return false;
    if (handled || !menu_equal(reply->values[0], "PLAY"))
        return true;
    float selected = menu_real(reply->values[1].number_bits);
    for (size_t index = 0; index < 5; ++index)
        if ((float)(index + 1) != selected &&
            !minigame_set_image(&owner->base, &owner->buttons[index].widget,
                                owner->buttons[index].up, error))
            return false;
    if (owner->state == ACCORDION_RIPPING)
        return true;
    owner->note = minigame_at(owner->definition.data.returned.table, (size_t)selected);
    return accordion_stop(owner, error) && accordion_tracks(owner, error);
}

static bool accordion_pig_image(AccordionScene *owner, const char *image,
                                AbError *error) {
    return ab_ui_toggle_button_set_state_image(&owner->pig, 1, menu_span(image), error);
}

static bool accordion_update(void *context, AbUiWidget *widget, float seconds,
                             float time, AbError *error) {
    AccordionScene *owner = context;
    PreviewMenu *menu = owner->base.menu;
    /* Native held input is delivered each update. Desktop motion events alone
     * do not keep a stationary held handle alive through its dragging reset. */
    if (menu->context.left_button_held && !owner->dragging) {
        AbUiPointerReply reply;
        if (!accordion_handle_pointer(owner, &owner->handle, menu_span("LHOLD"),
                                      menu->context.cursor, &reply, error))
            return false;
    }
    if (!ab_ui_widget_update_base(widget, seconds, time, error))
        return false;
    bool smiling =
        menu_route_equal(owner->pig_images[0].value, menu_data("GE_GRANDPA_04_SMILE"));
    if (owner->state == ACCORDION_RIPPING && smiling &&
        !accordion_pig_image(owner, "GE_GRANDPA_01", error))
        return false;
    bool collect = false;
    if (!owner->base.star_collected && owner->state == ACCORDION_RIPPING) {
        AbDataValue clip;
        bool playing;
        if (!ab_audio_bank_name_value(menu->audio, menu_data("accordion_break"), &clip,
                                      error) ||
            clip.kind != AB_DATA_STRING ||
            !ab_audio_bank_is_playing(menu->audio, clip.string, &playing, error))
            return false;
        collect = !playing;
    }
    if (collect) {
        if (!minigame_collect(&owner->base, error))
            return false;
    } else if (owner->state == ACCORDION_DRAGGING && owner->direction_changed) {
        if (!accordion_stop(owner, error) || !accordion_tracks(owner, error))
            return false;
        if (menu_real(ab_ui_toggle_button_get_state(&owner->pig).number_bits) == 1 &&
            (!accordion_pig_image(
                 owner, smiling ? "GE_GRANDPA_01" : "GE_GRANDPA_04_SMILE", error) ||
             !accordion_sound(owner, "pig_accordion", .7f, error)))
            return false;
    } else if (owner->state == ACCORDION_IDLE &&
               !accordion_pig_image(owner, "GE_GRANDPA_01", error)) {
        return false;
    }
    owner->volume_timer += seconds;
    if (owner->volume_timer > .016f) {
        float x = menu_real(owner->handle.frame->fields.x.number_bits);
        float movement = fabsf(x - owner->previous_x);
        if (owner->state == ACCORDION_DRAGGING && movement > 0)
            owner->volume = fminf(
                owner->volume + ((owner->volume_timer * 20) * movement) / owner->width,
                1);
        else
            owner->volume = fmaxf(owner->volume - owner->volume_timer * .35f, 0);
        owner->volume_timer = 0;
        owner->previous_x = x;
    }
    float left = owner->volume, right = owner->volume;
    if (owner->direction_present) {
        if (owner->direction < 0)
            right = 0;
        else
            left = 0;
    }
    if (!accordion_volume(owner, 5, left, error) ||
        !accordion_volume(owner, 6, right, error))
        return false;
    float music = ab_audio_mixer_view(menu->mixer).track_gains[7];
    if (music <= 0 && !owner->sound_muted) {
        if (!accordion_stop(owner, error))
            return false;
        owner->sound_muted = true;
    } else if (music > 0 && owner->sound_muted) {
        owner->sound_muted = false;
    }
    return true;
}

static bool accordion_construct(PreviewMinigame *base, AbError *error) {
    AccordionScene *owner = (AccordionScene *)base;
    AbTargetData *data = &owner->definition;
    if (!minigame_script(
            base, "assets/data/scripts/menus/accordion.lua",
            "b021ef2018148a983579adc3124a56d26b09e57c9289933e2f0a8ae8df1dbe5e", data,
            error) ||
        !ab_lua_data_literal_table_range(&data->chunk, 12, 7, 1, 1024 * 1024,
                                         &data->data, error) ||
        data->data.returned.kind != AB_DATA_TABLE ||
        !minigame_widgets(base, "Accordion", error) ||
        !minigame_image(base, &owner->ripped, "ripped_bellows", "ACCO_MID_BROKEN",
                        error) ||
        !minigame_image(base, &owner->bellows, "bellows", "ACCO_MID", error) ||
        !minigame_image(base, &owner->left, "accordion", "ACCO_LEFT", error))
        return false;
    owner->ripped.frame->fields.visible =
        (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN, .boolean = false};
    for (size_t index = 0; index < 5; ++index) {
        AccordionButton *button = owner->buttons + index;
        button->owner = owner;
        button->index = index + 1;
        int name = snprintf(button->name, sizeof(button->name), "button%zu", index + 1);
        int up = snprintf(button->up, sizeof(button->up), "ACCO_BTN_UP_%zu", index + 1);
        int down = snprintf(button->down, sizeof(button->down), "ACCO_BTN_DOWN_%zu",
                            index + 1);
        if (name <= 0 || (size_t)name >= sizeof(button->name) || up <= 0 ||
            (size_t)up >= sizeof(button->up) || down <= 0 ||
            (size_t)down >= sizeof(button->down) ||
            !minigame_image(base, &button->widget, button->name, button->up, error))
            return ab_fail(error, "Invalid accordion button construction");
        button->widget.methods =
            (AbUiWidgetMethods){.context = button, .pointer = accordion_button_pointer};
    }
    AbUiImageButtonArguments pig = {.image.frame.name = menu_text("pig"),
                                    .return_value = menu_text("TOGGLE_PIG")};
    owner->pig_images[0] =
        (AbDataEntry){menu_route_number(1), menu_data("GE_GRANDPA_01")};
    owner->pig_images[1] =
        (AbDataEntry){menu_route_number(2), menu_data("GE_GRANDPA_01_BLINK")};
    owner->pig_states = (AbDataTable){owner->pig_images, 2, 2, NULL};
    if (!minigame_image(base, &owner->handle, "handle", "ACCO_RIGHT", error) ||
        !ab_ui_toggle_button_init(&owner->pig, &base->menu->context,
                                  (AbUiWidgetChildren){0}, &pig, error) ||
        !ab_ui_toggle_button_set_images(&owner->pig, &owner->pig_states,
                                        (AbUiPointerValue){0}, error) ||
        !ab_ui_widget_add(&base->widget, &owner->pig.widget, error) ||
        !minigame_widget_controls(base, error))
        return false;
    owner->handle.methods = (AbUiWidgetMethods){.context = owner,
                                                .pointer = accordion_handle_pointer,
                                                .update = accordion_handle_update};
    base->widget.methods = (AbUiWidgetMethods){.context = owner,
                                               .lifecycle = accordion_lifecycle,
                                               .pointer = accordion_pointer,
                                               .update = accordion_update};
    return true;
}

static void accordion_dispose(PreviewMinigame *base) {
    AccordionScene *owner = (AccordionScene *)base;
    ab_target_data_close(&owner->definition);
    free(owner);
}

static bool accordion_new(PreviewMenu *menu, AbError *error) {
    AccordionScene *owner = calloc(1, sizeof(*owner));
    if (!owner)
        return ab_fail(error, "Cannot allocate accordion scene");
    owner->base = (PreviewMinigame){.menu = menu,
                                    .name = menu_data("ACCORDION"),
                                    .location = "accordion",
                                    .groups = {"GOLDEN_EGGS", "BACKGROUNDS"},
                                    .group_count = 2,
                                    .construct = accordion_construct,
                                    .dispose = accordion_dispose};
    menu->minigame = &owner->base;
    return true;
}

#endif
