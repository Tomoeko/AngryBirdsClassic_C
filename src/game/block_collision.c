#include "ab/block_collision.h"

#include <math.h>
#include <string.h>

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL &&
           (value.kind != AB_DATA_BOOLEAN || value.bits != 0);
}

static bool same_value(AbDataValue first, AbDataValue second) {
    if (first.kind != second.kind) {
        return false;
    }
    switch (first.kind) {
        case AB_DATA_NIL:
            return true;
        case AB_DATA_BOOLEAN:
            return (first.bits != 0) == (second.bits != 0);
        case AB_DATA_NUMBER: {
            float a, b;
            memcpy(&a, &first.bits, sizeof(a));
            memcpy(&b, &second.bits, sizeof(b));
            return a == b;
        }
        case AB_DATA_STRING:
            return first.string.size == second.string.size &&
                   (!first.string.size || (first.string.data && second.string.data &&
                                           memcmp(first.string.data, second.string.data,
                                                  first.string.size) == 0));
        case AB_DATA_TABLE:
            return first.table == second.table;
        case AB_DATA_OBJECT:
            return first.object == second.object;
    }
    return false;
}

static bool play_sound(const AbBlockCollisionHost *host, AbDataValue sound,
                       AbError *error) {
    if (!truthy(sound)) {
        return true;
    }
    if (!host->audio_name) {
        return ab_fail(error, "Missing block collision audio-name host");
    }
    bool (*audio)(void *, AbAudioPlay, AbError *) = host->audio;
    AbDataValue name;
    if (!host->audio_name(host->context, sound, &name, error)) {
        return false;
    }
    if (!audio) {
        return ab_fail(error, "Missing block collision audio host");
    }
    AbAudioPlay request = {
        .name = name,
        .volume = {.kind = AB_DATA_NUMBER, .bits = UINT32_C(0x3f000000)},
        .loop = {.kind = AB_DATA_BOOLEAN},
        .channel = {.kind = AB_DATA_NUMBER, .bits = UINT32_C(0x40000000)}};
    return audio(host->context, request, error);
}

static bool object_sound(const AbBlockCollisionHost *host, AbBlockCollisionSound kind,
                         AbBlockCollisionObject *object,
                         AbBlockCollisionDefinition *definition, AbError *error) {
    if (!host->sound) {
        return ab_fail(error, "Missing block collision level-effect sound host");
    }
    AbDataValue sound = {0};
    return host->sound(host->context, kind, object, definition, &sound, error) &&
           play_sound(host, sound, error);
}

static bool damage_state(const AbBlockCollisionHost *host,
                         AbBlockCollisionObject *object,
                         AbBlockCollisionDefinition *definition, AbError *error) {
    if (!host->sprites) {
        return ab_fail(error, "Missing block collision damage-sprite host");
    }
    AbDamageSpriteNames names = {0};
    if (!host->sprites(host->context, object, &names, error)) {
        return false;
    }
    if (same_value(names.sprite, object->damage_sprite)) {
        return true;
    }
    object->damage_sprite = names.sprite;
    object->blink_sprite = names.blink;
    object->smile_sprite = names.smile;
    if (!object_sound(host, AB_BLOCK_COLLISION_DAMAGE_SOUND, object, definition,
                      error)) {
        return false;
    }
    if (!host->particles) {
        return ab_fail(error, "Missing block collision damage-particle host");
    }
    return host->particles(host->context, object, error);
}

static bool add_score(const AbBlockCollisionHost *host, float damage, AbError *error) {
    if (!host->score) {
        return ab_fail(error, "Missing block collision damage-score host");
    }
    float increment;
    if (!host->score(host->context, damage, &increment, error)) {
        return false;
    }
    AbBlockCollisionScores *scores = host->scores;
    if (!scores || !isfinite(scores->total) || !isfinite(increment) ||
        scores->count > scores->capacity || (scores->capacity && !scores->entries)) {
        return ab_fail(error, "Invalid block collision score state");
    }
    scores->total = scores->total + increment;
    if (!isfinite(scores->total) || scores->count == scores->capacity) {
        return ab_fail(error, "Block collision score append exceeds finite storage");
    }
    scores->entries[scores->count++] = increment;
    return true;
}

static bool object_script(AbBlockCollisionObject *object, AbBlockCollisionNotice notice,
                          AbError *error) {
    AbBlockCollisionScript *script = object->script;
    if (!script) {
        return true;
    }
    if (!script->collision) {
        return ab_fail(error, "Missing block object-script collision method");
    }
    notice.other_name = notice.second->name;
    return script->collision(script->context, &notice, error);
}

static bool invoke(const AbBlockCollisionInput *input, const AbBlockCollisionHost *host,
                   unsigned depth, AbError *error) {
    if (depth > 1) {
        return ab_fail(error, "Block collision object lookup changed sensor recursion");
    }
    AbBlockCollisionObject *objects[2] = {0};
    if (!host->object(host->context, input->first, objects, error) ||
        !host->object(host->context, input->second, objects + 1, error)) {
        return false;
    }
    if (!objects[0] || !objects[1]) {
        return ab_fail(error, "Missing block collision world object");
    }
    if (truthy(objects[0]->sensor)) {
        if (truthy(objects[1]->sensor)) {
            return true;
        }
    } else if (truthy(objects[1]->sensor)) {
        AbBlockCollisionInput swapped = *input;
        swapped.first = input->second;
        swapped.second = input->first;
        if (!invoke(&swapped, host, depth + 1, error)) {
            return false;
        }
    }
    if (!isfinite(input->force) || !isfinite(input->damage) ||
        !isfinite(input->normal.x) || !isfinite(input->normal.y) ||
        !isfinite(input->point.x) || !isfinite(input->point.y)) {
        return ab_fail(error, "Invalid block collision numeric input");
    }
    AbBlockCollisionNotice notice = {.first = objects[0],
                                     .second = objects[1],
                                     .force = input->force,
                                     .damage = input->damage};
    bool allow_score = true;
    if (host->level) {
        AbDataValue result = {0};
        if (!host->level(host->context, &notice, &result, error)) {
            return false;
        }
        allow_score = !truthy(result);
    }
    if (!host->definition) {
        return ab_fail(error, "Missing block collision definition host");
    }
    AbBlockCollisionDefinition *definitions[2] = {0};
    if (!host->definition(host->context, input->first, definitions, error) ||
        !host->definition(host->context, input->second, definitions + 1, error)) {
        return false;
    }
    if (!definitions[0] || !definitions[1]) {
        return ab_fail(error, "Missing block collision definition");
    }
    notice.normal_present = true;
    for (size_t side = 0; side < 2; ++side) {
        notice.first = objects[side];
        notice.second = objects[1u - side];
        notice.normal =
            side ? (AbVec2){-input->normal.x, -input->normal.y} : input->normal;
        AbDataValue name = definitions[side]->on_collision;
        if (truthy(name)) {
            if (!host->named) {
                return ab_fail(error, "Missing named block collision handler host");
            }
            if (!host->named(host->context, name, &notice, error)) {
                return false;
            }
        }
        if (!host->scripts) {
            return ab_fail(error, "Missing block collision script-event host");
        }
        if (!host->scripts(host->context, &notice, error)) {
            return false;
        }
    }
    notice.normal = input->normal;
    notice.point = input->point;
    notice.point_present = true;
    notice.first = objects[0];
    notice.second = objects[1];
    if (!object_script(objects[0], notice, error)) {
        return false;
    }
    if (!truthy(objects[1]->sensor)) {
        notice.first = objects[1];
        notice.second = objects[0];
        if (!object_script(objects[1], notice, error)) {
            return false;
        }
    }
    if (input->damage > 0) {
        for (size_t side = 0; side < 2; ++side) {
            if (!damage_state(host, objects[side], definitions[side], error)) {
                return false;
            }
        }
        if (allow_score && !add_score(host, input->damage, error)) {
            return false;
        }
    }
    if (!isfinite(host->sound_force_threshold)) {
        return ab_fail(error, "Invalid block collision sound threshold");
    }
    if (input->force > host->sound_force_threshold) {
        if (!object_sound(host, AB_BLOCK_COLLISION_CONTACT_SOUND, objects[0],
                          definitions[0], error)) {
            return false;
        }
        if (!truthy(objects[1]->sensor) &&
            !object_sound(host, AB_BLOCK_COLLISION_CONTACT_SOUND, objects[1],
                          definitions[1], error)) {
            return false;
        }
    }
    return true;
}

bool ab_block_collision_callback(const AbBlockCollisionInput *input,
                                 const AbBlockCollisionHost *host, AbError *error) {
    if (!input || !host || !host->object || (input->first.size && !input->first.data) ||
        (input->second.size && !input->second.data)) {
        return ab_fail(error, "Invalid block collision input or host");
    }
    const AbBlockCollisionInput snapshot = *input;
    return invoke(&snapshot, host, 0, error);
}
