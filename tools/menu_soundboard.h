#ifndef AB_MENU_SOUNDBOARD_H
#define AB_MENU_SOUNDBOARD_H

#include "ab/hash.h"

typedef struct {
    SoundboardScene *owner;
    AbUiWidget widget;
    float click_time;
    char name[32];
} SoundboardButton;

struct SoundboardScene {
    PreviewMinigame base;
    SoundboardButton buttons[16];
    AbTargetData definition;
    const AbDataTable *descriptors;
    size_t bird;
    enum { SOUNDBOARD_NONE, SOUNDBOARD_STRETCHED, SOUNDBOARD_LAUNCHED } state;
    bool started, specialty;
};

static bool soundboard_stop(SoundboardScene *owner, AbDataValue name, AbError *error) {
    if (name.kind != AB_DATA_STRING)
        return ab_fail(error, "Soundboard clip name is not a string");
    AbAudioBank *audio = owner->base.menu->audio;
    const AbDataValue *group = ab_data_get(ab_audio_bank_groups(audio), name);
    if (!group || !menu_route_truthy(*group)) {
        bool playing;
        return ab_audio_bank_is_playing(audio, name.string, &playing, error) &&
               (!playing || ab_audio_bank_stop(audio, name.string, error));
    }
    if (group->kind != AB_DATA_TABLE)
        return ab_fail(error, "Soundboard audio group is not an array");
    for (size_t index = 1; index <= group->table->count; ++index) {
        AbDataValue clip = minigame_at(group->table, index);
        if (clip.kind == AB_DATA_NIL)
            break;
        bool playing;
        if (clip.kind != AB_DATA_STRING ||
            !ab_audio_bank_is_playing(audio, clip.string, &playing, error) ||
            (playing && !ab_audio_bank_stop(audio, clip.string, error)))
            return ab_fail(error, "Cannot stop actual soundboard group member");
    }
    return true;
}

/* Soundboard.clicked5424 retains launch state across pig/block sounds. A wrong
 * bird or reset clears started/state, while Play alone does not reset a launch. */
static bool soundboard_clicked(SoundboardScene *owner, size_t index, AbDataValue type,
                               AbDataValue *variant, AbError *error) {
    *variant = (AbDataValue){0};
    if (menu_route_equal(type, menu_data("play"))) {
        owner->started = true;
        *variant = menu_data("play");
    } else if (menu_route_equal(type, menu_data("slingshot"))) {
        owner->state = SOUNDBOARD_STRETCHED;
        *variant = menu_data("stretch");
    } else if (menu_route_equal(type, menu_data("bird"))) {
        if (owner->state == SOUNDBOARD_STRETCHED) {
            owner->state = SOUNDBOARD_LAUNCHED;
            owner->bird = index;
            owner->specialty = true;
            *variant = menu_data("launch");
        } else if (owner->state == SOUNDBOARD_LAUNCHED && owner->bird == index) {
            *variant = menu_data(owner->specialty ? "special" : "collide");
            owner->specialty = false;
        }
    } else if (menu_route_equal(type, menu_data("pig")) &&
               owner->state == SOUNDBOARD_LAUNCHED) {
        if (owner->started && !owner->base.star_collected &&
            !minigame_collect(&owner->base, error))
            return false;
        *variant = menu_data("kill");
    } else if (menu_route_equal(type, menu_data("block")) &&
               owner->state == SOUNDBOARD_LAUNCHED) {
        *variant = menu_data("hit");
    }
    if (variant->kind == AB_DATA_NIL) {
        owner->state = SOUNDBOARD_NONE;
        owner->started = false;
    }
    return true;
}

static bool soundboard_button_update(void *context, AbUiWidget *widget, float seconds,
                                     float time, AbError *error) {
    SoundboardButton *button = context;
    float scale;
    if (!ab_ui_widget_update_base(widget, seconds, time, error) ||
        !ab_ui_scaling_get(&button->owner->base.menu->scaling, &scale, error))
        return false;
    if (time < button->click_time + .1f)
        scale *= .875f;
    widget->frame->fields.scale_x = widget->frame->fields.scale_y = menu_number(scale);
    return true;
}

static bool soundboard_layout(SoundboardScene *owner, AbError *error) {
    PreviewMenu *menu = owner->base.menu;
    if (!minigame_layout(&owner->base, error))
        return false;
    for (size_t index = 0; index < 16; ++index) {
        AbDataValue descriptor = minigame_at(owner->descriptors, index + 1);
        float relative_x = 0, relative_y = 0;
        if (descriptor.kind != AB_DATA_TABLE ||
            !minigame_number(descriptor.table, "rel_x", &relative_x, error) ||
            !minigame_number(descriptor.table, "rel_y", &relative_y, error))
            return false;
        AbUiFrameFields *frame = &owner->buttons[index].widget.frame->fields;
        frame->x = menu_number(relative_x * menu->screen.x);
        frame->y = menu_number(relative_y * menu->screen.y);
        if (!ab_ui_scaling_set_to(&menu->scaling, frame, error))
            return false;
    }
    return true;
}

static bool soundboard_lifecycle(void *context, AbUiWidget *widget,
                                 AbUiFrameMethod method, AbError *error) {
    SoundboardScene *owner = context;
    if (!minigame_lifecycle(&owner->base, widget, method, error))
        return false;
    if (method == AB_UI_FRAME_LAYOUT)
        return soundboard_layout(owner, error);
    if (method == AB_UI_FRAME_ENTRY) {
        owner->state = SOUNDBOARD_NONE;
        owner->bird = 0;
        owner->specialty = owner->started = false;
        for (size_t index = 0; index < 16; ++index)
            owner->buttons[index].click_time = -.5f;
    }
    return true;
}

static bool soundboard_play(SoundboardScene *owner, size_t index, AbError *error) {
    if (!index || index > 16)
        return ab_fail(error, "Invalid actual soundboard button identity");
    AbDataValue descriptor = minigame_at(owner->descriptors, index);
    if (descriptor.kind != AB_DATA_TABLE)
        return ab_fail(error, "Missing actual soundboard descriptor table");
    AbDataValue variant;
    if (!soundboard_clicked(owner, index, menu_route_field(descriptor.table, "type"),
                            &variant, error))
        return false;
    AbDataValue clips = menu_route_field(descriptor.table, "audio");
    if (clips.kind != AB_DATA_TABLE)
        return ab_fail(error, "Missing actual soundboard audio table");
    const AbDataValue *selected =
        variant.kind != AB_DATA_NIL && clips.kind == AB_DATA_TABLE
            ? ab_data_get(clips.table, variant)
            : NULL;
    AbDataValue name = selected && menu_route_truthy(*selected)
                           ? *selected
                           : menu_route_field(clips.table, "default");
    AbDataValue resolved;
    uint32_t handle;
    if (!soundboard_stop(owner, name, error) ||
        !ab_audio_bank_name_value(owner->base.menu->audio, name, &resolved, error) ||
        resolved.kind != AB_DATA_STRING ||
        !ab_audio_bank_play_resource(owner->base.menu->audio, resolved.string, 1, false,
                                     0, &handle, error))
        return false;
    owner->buttons[index - 1].click_time = owner->base.menu->time;
    return true;
}

static bool soundboard_pointer(void *context, AbUiWidget *widget, AbSpan event,
                               AbVec2 point, AbUiPointerReply *reply, AbError *error) {
    SoundboardScene *owner = context;
    if (!ab_ui_widget_pointer_base(widget, event, point, reply, error))
        return false;
    bool handled;
    if (!minigame_controls(&owner->base, reply, &handled, error))
        return false;
    if (handled)
        return true;
    if (!menu_equal(reply->values[0], "PLAY_CLIP"))
        return true;
    if (reply->values[1].kind != AB_UI_POINTER_NUMBER)
        return ab_fail(error, "Soundboard button lost its numeric metadata");
    float index = menu_real(reply->values[1].number_bits);
    return isfinite(index) && index >= 1 && index <= 16 && floorf(index) == index &&
           soundboard_play(owner, (size_t)index, error);
}

static bool soundboard_definition(SoundboardScene *owner, AbError *error) {
    AbTargetData *data = &owner->definition;
    if (!minigame_script(
            &owner->base, "assets/data/scripts/menus/soundboard.lua",
            "1e7b1518b0b5eb880a7c4dbdd5ceeb0e2cbfacc8a5b2e9d191d4aa25d861ce2f", data,
            error) ||
        !ab_lua_data_literal_table_range(&data->chunk, 31, 152, 1, 1024 * 1024,
                                         &data->data, error) ||
        data->data.returned.kind != AB_DATA_TABLE ||
        data->data.returned.table->count != 16)
        return ab_fail(error, "Unexpected pinned soundboard descriptor constructor");
    owner->descriptors = data->data.returned.table;
    return true;
}

static bool soundboard_widgets(SoundboardScene *owner, AbError *error) {
    AbUiWidgetContext *context = &owner->base.menu->context;
    if (!minigame_widgets(&owner->base, "Soundboard", error) ||
        !minigame_widget_controls(&owner->base, error))
        return false;
    for (size_t index = 0; index < 16; ++index) {
        SoundboardButton *button = owner->buttons + index;
        button->owner = owner;
        int length = snprintf(button->name, sizeof(button->name),
                              "soundboard_button_%zu", index + 1);
        AbDataValue descriptor = minigame_at(owner->descriptors, index + 1);
        if (descriptor.kind != AB_DATA_TABLE)
            return ab_fail(error, "Missing authored soundboard button table");
        AbDataValue image = menu_route_field(descriptor.table, "image");
        if (length <= 0 || (size_t)length >= sizeof(button->name) ||
            image.kind != AB_DATA_STRING)
            return ab_fail(error, "Invalid actual soundboard button descriptor");
        AbUiImageButtonArguments arguments = {
            .image = {.frame.name = menu_text(button->name),
                      .image = {.kind = AB_UI_POINTER_TEXT, .text = image.string}},
            .return_value = menu_text("PLAY_CLIP"),
            .meta = menu_number((float)(index + 1))};
        if (!ab_ui_widget_init(&button->widget, context, AB_UI_WIDGET_IMAGE_BUTTON,
                               (AbUiWidgetChildren){0}, NULL, NULL, &arguments,
                               error) ||
            !ab_ui_widget_add(&owner->base.widget, &button->widget, error))
            return false;
        button->widget.methods =
            (AbUiWidgetMethods){.context = button, .update = soundboard_button_update};
    }
    owner->base.widget.methods = (AbUiWidgetMethods){.context = owner,
                                                     .lifecycle = soundboard_lifecycle,
                                                     .pointer = soundboard_pointer};
    return true;
}

static void soundboard_dispose(PreviewMinigame *base) {
    SoundboardScene *owner = (SoundboardScene *)base;
    ab_target_data_close(&owner->definition);
    free(owner);
}

static bool soundboard_construct(PreviewMinigame *base, AbError *error) {
    SoundboardScene *owner = (SoundboardScene *)base;
    return soundboard_definition(owner, error) && soundboard_widgets(owner, error);
}

static bool soundboard_new(PreviewMenu *menu, AbError *error) {
    SoundboardScene *owner = calloc(1, sizeof(*owner));
    if (!owner)
        return ab_fail(error, "Cannot allocate soundboard scene");
    owner->base = (PreviewMinigame){.menu = menu,
                                    .name = menu_data("SOUNDBOARD1"),
                                    .location = "soundboard",
                                    .groups = {"GOLDEN_EGGS", "BACKGROUNDS"},
                                    .group_count = 2,
                                    .construct = soundboard_construct,
                                    .dispose = soundboard_dispose};
    menu->minigame = &owner->base;
    return true;
}

#endif
