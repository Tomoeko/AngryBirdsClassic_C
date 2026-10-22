#include "ab/block_removal_score.h"
#include "ab/ui_value_text.h"
#include "script_float_internal.h"

#include <string.h>

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL &&
           (value.kind != AB_DATA_BOOLEAN || value.bits != 0);
}

static bool field(AbDataValue table, const char *name, AbDataValue *output,
                  AbError *error) {
    if (table.kind != AB_DATA_TABLE || !table.table ||
        table.table->count > table.table->capacity ||
        (table.table->count && !table.table->entries)) {
        return ab_fail(error, "Removal scoring indexes a non-table");
    }
    const AbDataValue *found = ab_data_get(
        table.table, (AbDataValue){.kind = AB_DATA_STRING,
                                   .string = {(const uint8_t *)name, strlen(name)}});
    *output = found ? *found : (AbDataValue){0};
    return true;
}

static bool damage_sound(AbDataValue definition, AbBlockRemovalScoreHost *host,
                         AbError *error) {
    AbDataValue sound = {0};
    if (!field(definition, "damageSound", &sound, error)) {
        return false;
    }
    if (sound.kind == AB_DATA_NIL) {
        return true;
    }
    if (!host->audio_name) {
        return ab_fail(error, "Missing removal damage-sound name host");
    }
    bool (*audio)(void *, AbAudioPlay, AbError *) = host->audio;
    AbDataValue name = {0};
    if (!host->audio_name(host->context, sound, &name, error)) {
        return false;
    }
    if (!audio) {
        return ab_fail(error, "Missing removal damage-sound audio host");
    }
    return audio(
        host->context,
        (AbAudioPlay){.name = name,
                      .volume = {.kind = AB_DATA_NUMBER, .bits = UINT32_C(0x3f800000)},
                      .loop = {.kind = AB_DATA_BOOLEAN}},
        error);
}

static bool floating(AbDataValue object, AbDataValue definition, bool goal,
                     AbScoreNumber increment, AbBlockRemovalScoreHost *host,
                     AbError *error) {
    AbBlockRemovalFloatingScore request = {.score = increment};
    bool sprite = goal && increment.present && increment.value == 5000.0f;
    if (!goal) {
        if (!field(definition, "spriteScore", &request.sprite, error)) {
            return false;
        }
        sprite = request.sprite.kind != AB_DATA_NIL;
    }
    if (!field(object, "x", &request.x, error) ||
        !field(object, "y", &request.y, error)) {
        return false;
    }
    uint8_t storage[32];
    if (sprite) {
        if (goal && !field(definition, "spriteScore", &request.sprite, error)) {
            return false;
        }
        request.lifetime = 0.9f;
    } else {
        if (!increment.present) {
            return ab_fail(error, "Missing breaking score in removal text");
        }
        uint32_t word;
        memcpy(&word, &increment.value, sizeof(word));
        if (!ab_ui_value_text(
                (AbUiPointerValue){.kind = AB_UI_POINTER_NUMBER, .number_bits = word},
                storage, sizeof(storage), &request.text)) {
            return ab_fail(error, "Cannot format removal score text");
        }
        request.text_present = true;
        request.lifetime = 0.6f;
    }
    if (!host->floating_scaling.present) {
        return ab_fail(error, "Missing numeric removal floating-score scaling");
    }
    float factor = 1;
    if (!sprite) {
        factor = script_float_binary(increment.value, 3000.0f, AB_SCRIPT_FLOAT_DIVIDE);
        factor = script_float_binary(0.25f, factor, AB_SCRIPT_FLOAT_ADD);
    }
    request.max_scale = script_float_binary(host->floating_scaling.value, factor,
                                            AB_SCRIPT_FLOAT_MULTIPLY);
    if (!host->floating) {
        return ab_fail(error, "Missing removal floating-score host");
    }
    return host->floating(host->context, &request, error);
}

static bool accumulate(AbScoreNumber increment, AbBlockRemovalScoreHost *host,
                       AbError *error) {
    AbBlockRemovalScores *scores = host->scores;
    if (!scores || !scores->total.present || !increment.present) {
        return ab_fail(error, "Missing numeric destruction-score total or increment");
    }
    scores->total.value =
        script_float_binary(scores->total.value, increment.value, AB_SCRIPT_FLOAT_ADD);
    if (scores->count >= scores->capacity || !scores->entries) {
        return ab_fail(error, "Destruction-score append exceeds storage");
    }
    scores->entries[scores->count++] = increment.value;
    return true;
}

bool ab_block_removal_score(const AbBlockRemovalEntry *entry,
                            AbBlockRemovalScoreHost *host, AbBlockRemovalScored *output,
                            AbError *error) {
    if (!entry || !host || !output || (entry->key.size && !entry->key.data)) {
        return ab_fail(error, "Invalid removal scoring input");
    }
    const AbBlockRemovalEntry captured = *entry;
    if (!host->destroy) {
        return ab_fail(error, "Missing required removal onDestroy host");
    }
    if (!host->destroy(host->context, captured.object, error)) {
        return false;
    }
    if (!host->definition) {
        return ab_fail(error, "Missing removal definition host");
    }
    AbBlockRemovalScored result = {0};
    if (!host->definition(host->context, captured.key, &result.definition, error)) {
        return false;
    }
    if (!host->score) {
        return ab_fail(error, "Missing removal breaking-score host");
    }
    if (!host->score(host->context, captured.object, &result.increment, error)) {
        return false;
    }
    AbDataValue level_goal = {0};
    if (!field(captured.object, "levelGoal", &level_goal, error)) {
        return false;
    }
    bool goal = truthy(level_goal);
    if ((goal && !damage_sound(result.definition, host, error)) ||
        !floating(captured.object, result.definition, goal, result.increment, host,
                  error) ||
        !accumulate(result.increment, host, error)) {
        return false;
    }
    *output = result;
    return true;
}
