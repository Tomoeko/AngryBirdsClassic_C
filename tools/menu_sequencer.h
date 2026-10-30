#ifndef AB_MENU_SEQUENCER_H
#define AB_MENU_SEQUENCER_H

#include "ab/hash.h"
#include "ab/ui_tiled_image.h"
#include "ab/ui_toggle_button.h"

typedef struct {
    AbUiToggleButton button;
    AbDataTable states, meta;
    AbDataEntry images[2], fields[2];
    float scales[2];
    char name[12];
} SequencerPig;

struct SequencerScene {
    PreviewMinigame base;
    AbTargetData definition;
    AbLuaData initial_images, face_images;
    AbUiWidget footprints, board, indicator, bird;
    AbUiTiledImage grass[2];
    SequencerPig pigs[3][8];
    float positions[8], minimum_x, maximum_x, value;
    float click_start_x, click_start_time, last_x;
    float animation, timer, play_delay;
    size_t play_position;
    enum { SEQUENCER_IDLE, SEQUENCER_CLICK, SEQUENCER_DRAGGING } drag;
    bool playing;
};

static bool sequencer_audio(SequencerScene *owner, const char *name, AbError *error) {
    AbDataValue resolved;
    uint32_t handle;
    AbAudioBank *audio = owner->base.menu->audio;
    return ab_audio_bank_name_value(audio, menu_data(name), &resolved, error) &&
           resolved.kind == AB_DATA_STRING &&
           ab_audio_bank_play_resource(audio, resolved.string, 1, false, 0, &handle,
                                       error);
}

static bool sequencer_bird_pivot(SequencerScene *owner, AbError *error) {
    AbVec2 bounds;
    AbUiImageResourceBoundaries *images = &owner->base.menu->context.images;
    if (!images->bounds(images->context, menu_span("BIRD_BIG_BROTHER"), &bounds, error))
        return false;
    owner->bird.state.image.rotation_pivot_x = menu_number(bounds.x * .5f);
    owner->bird.state.image.rotation_pivot_y = menu_number(bounds.y);
    return true;
}

static bool sequencer_faces(SequencerScene *owner, AbError *error) {
    const AbDataTable *faces = owner->face_images.returned.table;
    for (size_t row = 0; row < 3; ++row) {
        for (size_t column = 0; column < 8; ++column) {
            const char *group = !owner->playing                      ? "off"
                                : owner->play_position == column + 1 ? "hit"
                                                                     : "on";
            AbDataValue rows = menu_route_field(faces, group);
            AbDataValue images = rows.kind == AB_DATA_TABLE
                                     ? minigame_at(rows.table, row + 1)
                                     : (AbDataValue){0};
            if (images.kind != AB_DATA_TABLE)
                return ab_fail(error, "Missing actual sequencer face table");
            SequencerPig *pig = &owner->pigs[row][column];
            for (size_t state = 0; state < 2; ++state)
                pig->images[state].value = minigame_at(images.table, state + 1);
            if (!ab_ui_toggle_button_set_images(&pig->button, &pig->states,
                                                (AbUiPointerValue){0}, error))
                return false;
        }
    }
    return true;
}

static bool sequencer_toggle(SequencerScene *owner, AbError *error) {
    owner->playing = !owner->playing;
    if (owner->playing) {
        if (!minigame_set_image(&owner->base, &owner->bird, "BIRD_BIG_BROTHER_YELL",
                                error))
            return false;
        owner->play_delay = 1;
        owner->timer = 0;
        owner->play_position = 0;
        if (!sequencer_audio(owner, "big_brother_special_1", error))
            return false;
    } else {
        if (!minigame_set_image(&owner->base, &owner->bird, "BIRD_BIG_BROTHER_BLINK",
                                error))
            return false;
        owner->bird.frame->fields.angle = menu_number(0);
        owner->indicator.frame->fields.visible =
            (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN};
    }
    return sequencer_bird_pivot(owner, error) && sequencer_faces(owner, error);
}

static void sequencer_bird_position(SequencerScene *owner) {
    owner->bird.frame->fields.x = menu_number(
        owner->value * (owner->maximum_x - owner->minimum_x) + owner->minimum_x);
}

static bool sequencer_bird_pointer(void *context, AbUiWidget *widget, AbSpan event,
                                   AbVec2 point, AbUiPointerReply *reply,
                                   AbError *error) {
    SequencerScene *owner = context;
    bool hit;
    *reply = (AbUiPointerReply){0};
    if (!ab_ui_widget_image_hit(widget, point, &hit, error))
        return false;
    if (!hit)
        return true;
    AbUiPointerValue name = {.kind = AB_UI_POINTER_TEXT, .text = event};
    if (menu_equal(name, "LPRESS")) {
        owner->click_start_time = owner->base.menu->time;
        owner->click_start_x = owner->last_x = point.x;
        owner->drag = SEQUENCER_CLICK;
    } else if (menu_equal(name, "LHOLD")) {
        if (owner->drag == SEQUENCER_IDLE) {
            owner->click_start_time = owner->base.menu->time;
            owner->click_start_x = point.x;
            owner->drag = SEQUENCER_CLICK;
        }
        if (fabsf(owner->click_start_x - point.x) > owner->base.menu->tap_radius)
            owner->drag = SEQUENCER_DRAGGING;
        owner->last_x = point.x;
    } else if (menu_equal(name, "LRELEASE")) {
        if (owner->drag == SEQUENCER_CLICK && !sequencer_toggle(owner, error))
            return false;
        owner->drag = SEQUENCER_IDLE;
    }
    return true;
}

static bool sequencer_bird_update(void *context, AbUiWidget *widget, float seconds,
                                  float time, AbError *error) {
    SequencerScene *owner = context;
    if (!ab_ui_widget_update_base(widget, seconds, time, error))
        return false;
    if (owner->drag == SEQUENCER_DRAGGING) {
        owner->value = owner->last_x < owner->minimum_x ? 0
                       : owner->last_x > owner->maximum_x
                           ? 1
                           : (owner->last_x - owner->minimum_x) /
                                 (owner->maximum_x - owner->minimum_x);
        sequencer_bird_position(owner);
    }
    return true;
}

static bool sequencer_layout(SequencerScene *owner, AbError *error) {
    PreviewMenu *menu = owner->base.menu;
    float scale;
    if (!minigame_layout(&owner->base, error) ||
        !ab_ui_scaling_get(&menu->scaling, &scale, error))
        return false;
    float top = menu->screen.y * .15f;
    AbUiFrameFields *board = &owner->board.frame->fields;
    float height = menu_real(board->h.number_bits);
    board->x = menu_number(0);
    board->y = menu_number(top);
    board->w = menu_number(menu->screen.x / scale);
    if (!ab_ui_scaling_set_to(&menu->scaling, board, error))
        return false;
    for (size_t index = 0; index < 2; ++index) {
        AbUiTiledImage *grass = owner->grass + index;
        AbUiFrameFields *frame = &grass->widget.frame->fields;
        float width = menu_real(frame->w.number_bits);
        if (!isfinite(width) || width <= 0)
            return ab_fail(error, "Invalid sequencer grass width");
        frame->x = menu_number(0);
        frame->y = menu_number(top + (index ? height * scale : 0));
        if (!ab_ui_tiled_image_repeat(grass, ceilf(menu->screen.x / width),
                                      grass->repeat_y, error) ||
            !ab_ui_scaling_set_to(&menu->scaling, frame, error))
            return false;
    }
    float margin = menu_real(menu_route_field(&menu->margin, "x").bits);
    float span = menu->screen.x - margin * 2;
    for (size_t row = 0; row < 3; ++row) {
        for (size_t column = 0; column < 8; ++column) {
            SequencerPig *pig = &owner->pigs[row][column];
            AbUiFrameFields *frame = &pig->button.widget.frame->fields;
            float x = (span * .125f) * ((float)column + .5f) + margin;
            float y = top + ((height * .333f) * ((float)row + .5f)) * scale;
            frame->x = menu_number(x);
            frame->y = menu_number(y);
            AbVec2 bounds[2];
            for (size_t state = 0; state < 2; ++state) {
                AbUiPointerValue image;
                if (!ab_ui_toggle_button_get_state_image(&pig->button, state + 1,
                                                         &image, error) ||
                    image.kind != AB_UI_POINTER_TEXT ||
                    !ab_ui_scaling_sprite(&menu->scaling, &menu->context.images,
                                          image.text, false, bounds + state, error))
                    return false;
            }
            if (bounds[1].x <= 0)
                return ab_fail(error, "Invalid sequencer active pig width");
            pig->scales[0] = scale;
            pig->scales[1] = (bounds[0].x / bounds[1].x) * scale;
            if (!ab_ui_scaling_set_to(&menu->scaling, frame, error))
                return false;
            owner->positions[column] = x;
        }
    }
    owner->indicator.frame->fields.y = menu_number(top);
    if (!ab_ui_scaling_set_to(&menu->scaling, &owner->indicator.frame->fields, error))
        return false;
    AbVec2 bird, footprints;
    if (!ab_ui_scaling_sprite(&menu->scaling, &menu->context.images,
                              menu_span("BIRD_BIG_BROTHER"), false, &bird, error) ||
        !ab_ui_scaling_sprite(&menu->scaling, &menu->context.images,
                              menu_span("SOUNDBOARD_4_FOOTPRINT"), false, &footprints,
                              error))
        return false;
    owner->footprints.frame->fields.x = menu_number(menu->screen.x * .5f);
    owner->footprints.frame->fields.y =
        menu_number(menu->screen.y * .84f + bird.y * .4f);
    owner->bird.frame->fields.y = menu_number(menu->screen.y * .84f);
    owner->minimum_x = menu->screen.x * .5f - footprints.x * .5f;
    owner->maximum_x = menu->screen.x * .5f + footprints.x * .5f;
    if (!(owner->maximum_x > owner->minimum_x))
        return ab_fail(error, "Invalid sequencer tempo range");
    sequencer_bird_position(owner);
    /* Layout's temporary 1/1.5/3 bird scale is overwritten by setScaleTo. */
    return ab_ui_scaling_set_to(&menu->scaling, &owner->footprints.frame->fields,
                                error) &&
           ab_ui_scaling_set_to(&menu->scaling, &owner->bird.frame->fields, error) &&
           sequencer_bird_pivot(owner, error);
}

static bool sequencer_lifecycle(void *context, AbUiWidget *widget,
                                AbUiFrameMethod method, AbError *error) {
    SequencerScene *owner = context;
    return minigame_lifecycle(&owner->base, widget, method, error) &&
           (method != AB_UI_FRAME_LAYOUT || sequencer_layout(owner, error));
}

static bool sequencer_pointer(void *context, AbUiWidget *widget, AbSpan event,
                              AbVec2 point, AbUiPointerReply *reply, AbError *error) {
    SequencerScene *owner = context;
    bool handled;
    if (!ab_ui_widget_pointer_base(widget, event, point, reply, error) ||
        !minigame_controls(&owner->base, reply, &handled, error))
        return false;
    if (handled || !menu_equal(reply->values[0], "PIG_CLICKED"))
        return true;
    for (size_t row = 0; row < 3; ++row) {
        for (size_t column = 0; column < 8; ++column) {
            SequencerPig *pig = &owner->pigs[row][column];
            if (reply->values[2].kind != AB_UI_POINTER_OBJECT ||
                reply->values[2].object != (uintptr_t)&pig->button.widget)
                continue;
            float state =
                menu_real(ab_ui_toggle_button_get_state(&pig->button).number_bits);
            if (state != 1 && state != 2)
                return ab_fail(error, "Invalid sequencer pig toggle state");
            pig->button.widget.frame->fields.scale_x =
                pig->button.widget.frame->fields.scale_y =
                    menu_number(pig->scales[(size_t)state - 1]);
            return true;
        }
    }
    return ab_fail(error, "Foreign sequencer pig pointer reply");
}

static bool sequencer_play_column(SequencerScene *owner, AbError *error) {
    if (!owner->play_position || owner->play_position > 8)
        return ab_fail(error, "Invalid sequencer play column");
    char hat[20], snare[20];
    int hat_length = snprintf(hat, sizeof(hat), "pig_hi-hat_%zu",
                              (owner->play_position - 1) % 2 + 1);
    int snare_length = snprintf(snare, sizeof(snare), "pig_snare_%zu",
                                (owner->play_position - 1) % 4 + 1);
    if (hat_length <= 0 || (size_t)hat_length >= sizeof(hat) || snare_length <= 0 ||
        (size_t)snare_length >= sizeof(snare))
        return ab_fail(error, "Sequencer sound identifier exceeds storage");
    const char *sounds[] = {hat, snare, "pig_bd"};
    for (size_t row = 0; row < 3; ++row) {
        SequencerPig *pig = &owner->pigs[row][owner->play_position - 1];
        if (menu_real(ab_ui_toggle_button_get_state(&pig->button).number_bits) == 2 &&
            !sequencer_audio(owner, sounds[row], error))
            return false;
    }
    return true;
}

static bool sequencer_all_pigs(SequencerScene *owner) {
    size_t enabled = 0;
    for (size_t row = 0; row < 3; ++row)
        for (size_t column = 0; column < 8; ++column)
            enabled += menu_real(ab_ui_toggle_button_get_state(
                                     &owner->pigs[row][column].button)
                                     .number_bits) == 2;
    return enabled == 24;
}

static bool sequencer_update(void *context, AbUiWidget *widget, float seconds,
                             float time, AbError *error) {
    SequencerScene *owner = context;
    if (!ab_ui_widget_update_base(widget, seconds, time, error))
        return false;
    if (!owner->playing)
        return true;
    if (owner->play_delay > 0) {
        owner->play_delay = fmaxf(owner->play_delay - seconds, 0);
        if (owner->play_delay != 0)
            return true;
        if (!minigame_set_image(&owner->base, &owner->bird, "BIRD_BIG_BROTHER",
                                error) ||
            !sequencer_bird_pivot(owner, error))
            return false;
        owner->indicator.frame->fields.visible =
            (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN, .boolean = true};
        owner->indicator.frame->fields.x = menu_number(owner->positions[0]);
        owner->play_position = 1;
        return sequencer_play_column(owner, error) && sequencer_faces(owner, error);
    }
    float interval = owner->value * -.35f + .5f;
    float phase = owner->animation + seconds / (6 * interval);
    owner->animation =
        (float)((double)phase - floor((double)(phase / .4f)) * (double).4f);
    float angle = owner->animation <= .1f   ? owner->animation
                  : owner->animation <= .3f ? .2f - owner->animation
                                            : owner->animation - .4f;
    owner->bird.frame->fields.angle = menu_number(angle);
    owner->timer += seconds;
    if (owner->timer >= interval) {
        owner->timer -= interval;
        if (++owner->play_position == 9)
            owner->play_position = 1;
        if (!sequencer_play_column(owner, error) || !sequencer_faces(owner, error))
            return false;
    }
    owner->indicator.frame->fields.x =
        menu_number(owner->positions[owner->play_position - 1]);
    if (!owner->base.star_collected && owner->value >= .98f &&
        sequencer_all_pigs(owner))
        return minigame_collect(&owner->base, error) && sequencer_toggle(owner, error);
    return true;
}

static bool sequencer_definition(SequencerScene *owner, AbError *error) {
    AbTargetData *data = &owner->definition;
    if (!minigame_script(
            &owner->base, "assets/data/scripts/menus/sequencer.lua",
            "ca200bbcd3bafef273cb8078e2b39a00cc94c28b0828b190bfc75e33d65faae0", data,
            error) ||
        data->chunk.root.child_count != 21 ||
        data->chunk.root.children[8].file_offset != 2335 ||
        data->chunk.root.children[20].file_offset != 9746)
        return ab_fail(error, "Unexpected pinned sequencer prototypes");
    AbLuaChunk view = data->chunk;
    view.root = data->chunk.root.children[8];
    if (!ab_lua_data_literal_table_range(&view, 75, 14, 7, 1024 * 1024,
                                         &owner->initial_images, error))
        return false;
    view.root = data->chunk.root.children[20];
    return ab_lua_data_literal_table_range(&view, 0, 46, 1, 1024 * 1024,
                                           &owner->face_images, error) &&
           owner->initial_images.returned.kind == AB_DATA_TABLE &&
           owner->face_images.returned.kind == AB_DATA_TABLE;
}

static bool sequencer_widgets(SequencerScene *owner, AbError *error) {
    PreviewMenu *menu = owner->base.menu;
    if (!minigame_widgets(&owner->base, "Sequencer", error) ||
        !minigame_image(&owner->base, &owner->footprints, "footprints",
                        "SOUNDBOARD_4_FOOTPRINT", error) ||
        !minigame_image(&owner->base, &owner->board, "sequencer_bg", "SOUNDBOARD_4_BG",
                        error) ||
        !minigame_image(&owner->base, &owner->indicator, "indicator",
                        "SOUNDBOARD_4_HIGHLIGHT", error))
        return false;
    owner->indicator.frame->fields.visible =
        (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN};
    const char *const names[] = {"top_grass", "bottom_grass"};
    const char *const images[] = {"SOUNDBOARD_4_GRASS_TOP",
                                  "SOUNDBOARD_4_GRASS_BOTTOM"};
    for (size_t index = 0; index < 2; ++index) {
        AbUiFrameFields frame = {.name = menu_text(names[index])};
        if (!ab_ui_tiled_image_init(owner->grass + index, &menu->context,
                                    (AbUiWidgetChildren){0}, &frame, error) ||
            !ab_ui_tiled_image_set_image(owner->grass + index, menu_span(images[index]),
                                         error) ||
            !ab_ui_widget_add(&owner->base.widget, &owner->grass[index].widget, error))
            return false;
    }
    for (size_t row = 0; row < 3; ++row) {
        AbDataValue states = minigame_at(owner->initial_images.returned.table, row + 1);
        if (states.kind != AB_DATA_TABLE)
            return ab_fail(error, "Missing sequencer initial image row");
        for (size_t column = 0; column < 8; ++column) {
            SequencerPig *pig = &owner->pigs[row][column];
            int length = snprintf(pig->name, sizeof(pig->name), "pig%zu%zu", row + 1,
                                  column + 1);
            if (length <= 0 || (size_t)length >= sizeof(pig->name))
                return ab_fail(error, "Sequencer pig name exceeds storage");
            for (size_t state = 0; state < 2; ++state)
                pig->images[state] =
                    (AbDataEntry){menu_route_number((float)(state + 1)),
                                  minigame_at(states.table, state + 1)};
            pig->states = (AbDataTable){pig->images, 2, 2, NULL};
            pig->fields[0] =
                (AbDataEntry){menu_data("col"), menu_route_number((float)(row + 1))};
            pig->fields[1] =
                (AbDataEntry){menu_data("row"), menu_route_number((float)(column + 1))};
            pig->meta = (AbDataTable){pig->fields, 2, 2, NULL};
            AbUiImageButtonArguments arguments = {
                .image.frame.name = menu_text(pig->name),
                .return_value = menu_text("PIG_CLICKED"),
                .meta = {.kind = AB_UI_POINTER_OBJECT,
                         .object = (uintptr_t)&pig->meta}};
            if (!ab_ui_toggle_button_init(&pig->button, &menu->context,
                                          (AbUiWidgetChildren){0}, &arguments, error) ||
                !ab_ui_toggle_button_set_images(&pig->button, &pig->states,
                                                (AbUiPointerValue){0}, error) ||
                !ab_ui_widget_add(&owner->base.widget, &pig->button.widget, error))
                return false;
        }
    }
    if (!minigame_image(&owner->base, &owner->bird, "bird", "BIRD_BIG_BROTHER_BLINK",
                        error) ||
        !minigame_widget_controls(&owner->base, error))
        return false;
    owner->value = .5f;
    owner->play_position = 1;
    owner->bird.methods = (AbUiWidgetMethods){.context = owner,
                                              .pointer = sequencer_bird_pointer,
                                              .update = sequencer_bird_update};
    owner->base.widget.methods = (AbUiWidgetMethods){.context = owner,
                                                     .lifecycle = sequencer_lifecycle,
                                                     .pointer = sequencer_pointer,
                                                     .update = sequencer_update};
    return true;
}

static bool sequencer_construct(PreviewMinigame *base, AbError *error) {
    SequencerScene *owner = (SequencerScene *)base;
    return sequencer_definition(owner, error) && sequencer_widgets(owner, error);
}

static void sequencer_dispose(PreviewMinigame *base) {
    SequencerScene *owner = (SequencerScene *)base;
    ab_lua_data_close(&owner->initial_images);
    ab_lua_data_close(&owner->face_images);
    ab_target_data_close(&owner->definition);
    free(owner);
}

static bool sequencer_new(PreviewMenu *menu, AbError *error) {
    SequencerScene *owner = calloc(1, sizeof(*owner));
    if (!owner)
        return ab_fail(error, "Cannot allocate sequencer scene");
    owner->base =
        (PreviewMinigame){.menu = menu,
                          .name = menu_data("SEQUENCER"),
                          .location = "sequencer",
                          .groups = {"GOLDEN_EGGS", "INGAME_PIGS_OLD", "BACKGROUNDS"},
                          .group_count = 3,
                          .construct = sequencer_construct,
                          .dispose = sequencer_dispose};
    menu->minigame = &owner->base;
    return true;
}

#endif
