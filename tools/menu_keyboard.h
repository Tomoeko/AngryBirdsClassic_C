#ifndef AB_MENU_KEYBOARD_H
#define AB_MENU_KEYBOARD_H

#include "ab/hash.h"

typedef struct {
    KeyboardScene *owner;
    AbUiWidget widget, shadow;
    AbDataValue note, inactive, active;
    const AbDataTable *descriptor;
    AbUiHitRectangle hit_box;
    float click_time, click_scale, click_offset;
    char name[24], shadow_name[24];
    bool swipe_ready;
} KeyboardKey;

struct KeyboardScene {
    PreviewMinigame base;
    AbTargetData definition;
    AbLuaData song_data;
    const AbDataTable *song;
    KeyboardKey keys[8];
    AbDataValue last_notes[5];
    size_t note_count, play_position;
    float play_timer;
    bool autoplay;
};

static bool keyboard_key_update(void *context, AbUiWidget *widget, float seconds,
                                float time, AbError *error) {
    KeyboardKey *key = context;
    PreviewMenu *menu = key->owner->base.menu;
    bool active = time < key->click_time + .1f;
    AbDataValue image = active ? key->active : key->inactive;
    if (!ab_ui_widget_update_base(widget, seconds, time, error) ||
        !ab_ui_image_state_set_image(
            &widget->state.image,
            (AbUiPointerValue){.kind = AB_UI_POINTER_TEXT, .text = image.string},
            &menu->context.images, error))
        return false;
    key->click_scale = active ? .8f : 1;
    key->click_offset = 0;
    if (active) {
        AbVec2 pivot, bounds;
        if (!ab_ui_scaling_sprite(&menu->scaling, &menu->context.images, image.string,
                                  true, &pivot, error) ||
            !ab_ui_scaling_sprite(&menu->scaling, &menu->context.images, image.string,
                                  false, &bounds, error))
            return false;
        float bottom = bounds.y - pivot.y;
        key->click_offset = bottom - bottom * .8f;
    }
    return true;
}

static bool keyboard_key_draw(void *context, AbUiWidget *widget,
                              const AbUiDrawInput *input, AbError *error) {
    KeyboardKey *key = context;
    AbUiDrawTransform transform;
    if (!ab_ui_draw_resolve(input, &transform))
        return ab_fail(error, "Invalid keyboard parent draw coordinates");
    AbUiDrawInput draw = {
        .transform = {.position = {transform.position.x,
                                   transform.position.y + key->click_offset},
                      .scale = {key->click_scale, key->click_scale}},
        .present = 15};
    return ab_ui_widget_draw_base(widget, &draw, error);
}

static bool keyboard_key_pointer(void *context, AbUiWidget *widget, AbSpan event,
                                 AbVec2 point, AbUiPointerReply *reply,
                                 AbError *error) {
    (void)widget;
    KeyboardKey *key = context;
    bool hit;
    if (!ab_ui_rectangle_hit(&key->hit_box, point, &hit))
        return ab_fail(error, "Invalid keyboard note hit rectangle");
    *reply = (AbUiPointerReply){0};
    if (!hit) {
        key->swipe_ready = true;
        return true;
    }
    AbUiPointerValue event_name = {.kind = AB_UI_POINTER_TEXT, .text = event};
    if (!menu_equal(event_name, "LPRESS") &&
        !(menu_equal(event_name, "LHOLD") && key->swipe_ready))
        return true;
    key->swipe_ready = false;
    reply->values[0] = menu_text("PLAY_NOTE");
    reply->values[1] =
        (AbUiPointerValue){.kind = AB_UI_POINTER_TEXT, .text = key->note.string};
    reply->values[2] =
        (AbUiPointerValue){.kind = AB_UI_POINTER_OBJECT, .object = (uintptr_t)key};
    return true;
}

static bool keyboard_layout(KeyboardScene *owner, AbError *error) {
    PreviewMenu *menu = owner->base.menu;
    float scale;
    if (!minigame_layout(&owner->base, error) ||
        !ab_ui_scaling_get(&menu->scaling, &scale, error))
        return false;
    for (size_t index = 0; index < 8; ++index) {
        KeyboardKey *key = owner->keys + index;
        AbUiFrameFields *frame = &key->widget.frame->fields;
        AbUiFrameFields *shadow = &key->shadow.frame->fields;
        AbVec2 bounds, pivot;
        float x = 0, y = 0, offset = 0, shadow_scale = 0;
        if (!minigame_number(key->descriptor, "x", &x, error) ||
            !minigame_number(key->descriptor, "y", &y, error) ||
            !minigame_number(key->descriptor, "so", &offset, error) ||
            !minigame_number(key->descriptor, "ss", &shadow_scale, error) ||
            !ab_ui_scaling_sprite(&menu->scaling, &menu->context.images,
                                  key->widget.state.image.image.text, false, &bounds,
                                  error) ||
            !ab_ui_scaling_sprite(&menu->scaling, &menu->context.images,
                                  key->widget.state.image.image.text, true, &pivot,
                                  error))
            return false;
        x *= menu->screen.x / 480;
        y *= menu->screen.y / 320;
        frame->x = menu_number(x);
        frame->y = menu_number(y);
        shadow->x = menu_number(x + offset);
        shadow->y = menu_number(y + (bounds.y - pivot.y));
        shadow->scale_x = shadow->scale_y = menu_number(shadow_scale);
        if (frame->w.kind != AB_UI_POINTER_NUMBER ||
            frame->h.kind != AB_UI_POINTER_NUMBER)
            return ab_fail(error, "Keyboard note has no actual resource dimensions");
        key->hit_box =
            (AbUiHitRectangle){.center = {x, y},
                               .size = {menu_real(frame->w.number_bits) * scale,
                                        menu_real(frame->h.number_bits) * scale}};
        /* Source layout4062 assigns ss, then Scaling.setScaleTo overwrites it. */
        if (!ab_ui_scaling_set_to(&menu->scaling, frame, error) ||
            !ab_ui_scaling_set_to(&menu->scaling, shadow, error))
            return false;
    }
    return true;
}

static bool keyboard_lifecycle(void *context, AbUiWidget *widget,
                               AbUiFrameMethod method, AbError *error) {
    KeyboardScene *owner = context;
    if (!minigame_lifecycle(&owner->base, widget, method, error))
        return false;
    if (method == AB_UI_FRAME_LAYOUT)
        return keyboard_layout(owner, error);
    if (method == AB_UI_FRAME_ENTRY) {
        owner->autoplay = false;
        owner->note_count = 0;
        for (size_t index = 0; index < 8; ++index) {
            owner->keys[index].click_time = -.5f;
            owner->keys[index].click_scale = 1;
        }
    }
    return true;
}

static KeyboardKey *keyboard_find(KeyboardScene *owner, AbDataValue note) {
    for (size_t index = 0; index < 8; ++index)
        if (menu_route_equal(owner->keys[index].note, note))
            return owner->keys + index;
    return NULL;
}

static bool keyboard_play(KeyboardScene *owner, AbDataValue note, AbError *error) {
    KeyboardKey *key = keyboard_find(owner, note);
    char sound[16];
    if (!key || note.kind != AB_DATA_STRING || note.string.size > 3)
        return ab_fail(error, "Unknown authored keyboard note");
    int length = snprintf(sound, sizeof(sound), "note%.*s", (int)note.string.size,
                          (const char *)note.string.data);
    uint32_t handle;
    if (length <= 0 || (size_t)length >= sizeof(sound) ||
        !ab_audio_bank_play_resource(owner->base.menu->audio, menu_span(sound), 1,
                                     false, 0, &handle, error))
        return false;
    key->click_time = owner->base.menu->time;
    return true;
}

static bool keyboard_pointer(void *context, AbUiWidget *widget, AbSpan event,
                             AbVec2 point, AbUiPointerReply *reply, AbError *error) {
    KeyboardScene *owner = context;
    bool handled;
    if (!ab_ui_widget_pointer_base(widget, event, point, reply, error) ||
        !minigame_controls(&owner->base, reply, &handled, error))
        return false;
    if (handled || !menu_equal(reply->values[0], "PLAY_NOTE"))
        return true;
    if (reply->values[1].kind != AB_UI_POINTER_TEXT)
        return ab_fail(error, "Keyboard key lost its note metadata");
    AbDataValue note = {.kind = AB_DATA_STRING, .string = reply->values[1].text};
    if (!keyboard_play(owner, note, error))
        return false;
    if (owner->note_count == 5) {
        memmove(owner->last_notes, owner->last_notes + 1,
                4 * sizeof(*owner->last_notes));
        --owner->note_count;
    }
    owner->last_notes[owner->note_count++] = note;
    const char *const melody[] = {"C", "D", "Dis", "C", "G"};
    bool matches = owner->note_count == 5 && !owner->base.star_collected;
    for (size_t index = 0; matches && index < 5; ++index)
        matches = menu_route_equal(owner->last_notes[index], menu_data(melody[index]));
    if (matches) {
        owner->autoplay = true;
        owner->play_position = 1;
        owner->play_timer = 0;
    }
    return true;
}

static bool keyboard_update(void *context, AbUiWidget *widget, float seconds,
                            float time, AbError *error) {
    KeyboardScene *owner = context;
    if (!ab_ui_widget_update_base(widget, seconds, time, error))
        return false;
    if (!owner->autoplay)
        return true;
    if (owner->play_position > owner->song->count) {
        owner->autoplay = false;
        return owner->base.star_collected || minigame_collect(&owner->base, error);
    }
    AbDataValue row = minigame_at(owner->song, owner->play_position);
    float delay = 0;
    if (row.kind != AB_DATA_TABLE ||
        !minigame_number(row.table, "delay", &delay, error))
        return ab_fail(error, "Invalid actual keyboard melody row");
    owner->play_timer += seconds;
    if (owner->play_timer < delay)
        return true;
    owner->play_timer -= delay;
    if (!keyboard_play(owner, menu_route_field(row.table, "note"), error))
        return false;
    ++owner->play_position;
    return true;
}

static bool keyboard_definition(KeyboardScene *owner, AbError *error) {
    AbTargetData *data = &owner->definition;
    if (!minigame_script(
            &owner->base, "assets/data/scripts/menus/keyboard.lua",
            "7599c736b1f07c67aaaa69da81694d76d5ae15917e2e589aaaa3074bafe8e873", data,
            error) ||
        !ab_lua_data_literal_table_range(&data->chunk, 12, 65, 1, 1024 * 1024,
                                         &data->data, error) ||
        !ab_lua_data_literal_table_range(&data->chunk, 78, 38, 1, 1024 * 1024,
                                         &owner->song_data, error) ||
        data->data.returned.kind != AB_DATA_TABLE ||
        data->data.returned.table->count != 8 ||
        owner->song_data.returned.kind != AB_DATA_TABLE ||
        owner->song_data.returned.table->count != 12)
        return ab_fail(error, "Unexpected pinned keyboard constructors");
    owner->song = owner->song_data.returned.table;
    return true;
}

static bool keyboard_widgets(KeyboardScene *owner, AbError *error) {
    PreviewMenu *menu = owner->base.menu;
    if (!minigame_widgets(&owner->base, "Keyboard", error) ||
        !minigame_widget_controls(&owner->base, error))
        return false;
    const AbDataTable *descriptors = owner->definition.data.returned.table;
    AbStringMap *order = NULL;
    bool okay = ab_string_map_open(8, 8, 16384, &order, error);
    for (size_t index = 0; okay && index < descriptors->count; ++index) {
        AbDataEntry entry = descriptors->entries[index];
        okay = entry.key.kind == AB_DATA_STRING && entry.value.kind == AB_DATA_TABLE &&
               ab_string_map_set(order, entry.key.string, index, true, error);
    }
    size_t cursor = 0;
    for (size_t index = 0; okay && index < 8; ++index) {
        AbStringMapEntry entry;
        if (!ab_string_map_next(order, &cursor, &entry, error) || !entry.present ||
            entry.value >= descriptors->count) {
            okay = ab_fail(error, "Invalid keyboard constructor traversal");
            break;
        }
        KeyboardKey *key = owner->keys + index;
        key->owner = owner;
        key->note = (AbDataValue){.kind = AB_DATA_STRING, .string = entry.key};
        key->descriptor = descriptors->entries[entry.value].value.table;
        key->inactive = menu_route_field(key->descriptor, "inactive");
        key->active = menu_route_field(key->descriptor, "active");
        int length = snprintf(key->name, sizeof(key->name), "key%.*s",
                              (int)entry.key.size, (const char *)entry.key.data);
        int shadow_length =
            snprintf(key->shadow_name, sizeof(key->shadow_name), "shadow%.*s",
                     (int)entry.key.size, (const char *)entry.key.data);
        if (length <= 0 || (size_t)length >= sizeof(key->name) || shadow_length <= 0 ||
            (size_t)shadow_length >= sizeof(key->shadow_name) ||
            key->inactive.kind != AB_DATA_STRING ||
            key->active.kind != AB_DATA_STRING) {
            okay = ab_fail(error, "Invalid authored keyboard key descriptor");
            break;
        }
        AbUiImageArguments shadow = {.frame.name = menu_text(key->shadow_name),
                                     .image = menu_text("SOUNDBOARD_3_SHADOW")};
        AbUiImageArguments image = {
            .frame.name = menu_text(key->name),
            .image = {.kind = AB_UI_POINTER_TEXT, .text = key->inactive.string}};
        okay = ab_ui_widget_init(&key->shadow, &menu->context, AB_UI_WIDGET_IMAGE,
                                 (AbUiWidgetChildren){0}, NULL, &shadow, NULL, error) &&
               ab_ui_widget_add(&owner->base.widget, &key->shadow, error) &&
               ab_ui_widget_init(&key->widget, &menu->context, AB_UI_WIDGET_IMAGE,
                                 (AbUiWidgetChildren){0}, NULL, &image, NULL, error) &&
               ab_ui_widget_add(&owner->base.widget, &key->widget, error);
        key->widget.methods = (AbUiWidgetMethods){.context = key,
                                                  .update = keyboard_key_update,
                                                  .draw = keyboard_key_draw,
                                                  .pointer = keyboard_key_pointer};
    }
    ab_string_map_close(order);
    owner->base.widget.methods = (AbUiWidgetMethods){.context = owner,
                                                     .lifecycle = keyboard_lifecycle,
                                                     .pointer = keyboard_pointer,
                                                     .update = keyboard_update};
    return okay;
}

static bool keyboard_construct(PreviewMinigame *base, AbError *error) {
    KeyboardScene *owner = (KeyboardScene *)base;
    return keyboard_definition(owner, error) && keyboard_widgets(owner, error);
}

static void keyboard_dispose(PreviewMinigame *base) {
    KeyboardScene *owner = (KeyboardScene *)base;
    ab_lua_data_close(&owner->song_data);
    ab_target_data_close(&owner->definition);
    free(owner);
}

static bool keyboard_new(PreviewMenu *menu, AbError *error) {
    KeyboardScene *owner = calloc(1, sizeof(*owner));
    if (!owner)
        return ab_fail(error, "Cannot allocate keyboard scene");
    owner->base = (PreviewMinigame){.menu = menu,
                                    .name = menu_data("KEYBOARD"),
                                    .location = "keyboard",
                                    .groups = {"GOLDEN_EGGS", "BACKGROUNDS"},
                                    .group_count = 2,
                                    .construct = keyboard_construct,
                                    .dispose = keyboard_dispose};
    menu->minigame = &owner->base;
    return true;
}

#endif
