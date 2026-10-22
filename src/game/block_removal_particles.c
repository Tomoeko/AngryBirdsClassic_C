#include "ab/block_removal_particles.h"

#include <string.h>

static AbSpan span(const char *value) {
    return (AbSpan){(const uint8_t *)value, strlen(value)};
}

static bool field(AbDataValue table, const char *name, AbDataValue *output,
                  AbError *error) {
    if (table.kind != AB_DATA_TABLE || !table.table ||
        table.table->count > table.table->capacity ||
        (table.table->count && !table.table->entries)) {
        return ab_fail(error, "Removal particles index a non-table value");
    }
    const AbDataValue *found = ab_data_get(
        table.table, (AbDataValue){.kind = AB_DATA_STRING, .string = span(name)});
    *output = found ? *found : (AbDataValue){0};
    return true;
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL &&
           (value.kind != AB_DATA_BOOLEAN || value.bits != 0);
}

static bool removal_functions(AbDataValue object, AbBlockRemovalParticlesHost *host,
                              AbError *error) {
    if (!host->functions) {
        return ab_fail(error, "Missing current removal-function field view");
    }
    AbUiPointerValue binding = {0};
    if (!host->functions(host->context, object, &binding, error)) {
        return false;
    }
    if (binding.kind == AB_UI_POINTER_NONE) {
        return true;
    }
    if (binding.kind != AB_UI_POINTER_OBJECT || !binding.object) {
        return ab_fail(error, "Removal functions require an ipairs table");
    }
    AbBlockRemovalFunctions *list = (AbBlockRemovalFunctions *)binding.object;
    if (list->capacity && !list->items) {
        return ab_fail(error, "Missing removal-function item storage");
    }
    AbUiPointerValue *items = list->items;
    size_t capacity = list->capacity;
    for (size_t index = 0; index < capacity; ++index) {
        AbUiPointerValue function = items[index];
        if (function.kind == AB_UI_POINTER_NONE) {
            break;
        }
        if (function.kind != AB_UI_POINTER_FUNCTION || !function.function) {
            return ab_fail(error, "Removal-function item is not callable");
        }
        if (!host->call) {
            return ab_fail(error, "Missing removal-function call host");
        }
        if (!host->call(host->context, function, object, error)) {
            return false;
        }
    }
    return true;
}

static bool particles(const AbBlockRemovalEntry *entry,
                      const AbBlockRemovalEffects *cached,
                      AbBlockRemovalParticlesHost *host, AbError *error) {
    AbDataValue silent = {0};
    if (!field(entry->object, "killSilently", &silent, error)) {
        return false;
    }
    if (truthy(silent)) {
        return true;
    }
    if (!host->property) {
        return ab_fail(error, "Missing removal particle-property host");
    }
    AbDataValue use_new = {0};
    if (!host->property(host->context, entry->object, span("useNewParticleTable"),
                        &use_new, error)) {
        return false;
    }
    if (!truthy(use_new)) {
        if (!host->old_particles) {
            return ab_fail(error, "Missing legacy removal-particle host");
        }
        return host->old_particles(host->context, entry->key, cached->particles,
                                   cached->amount, false, false, error);
    }
    bool (*emit)(void *, AbSpan, AbDataValue, bool, bool, AbError *) =
        host->new_particles;
    if (!host->property) {
        return ab_fail(error, "Missing fresh removal particle-property host");
    }
    AbDataValue current = {0}, descriptor = {0};
    if (!host->property(host->context, entry->object, span("particles"), &current,
                        error) ||
        !field(current, "removal", &descriptor, error)) {
        return false;
    }
    if (!emit) {
        return ab_fail(error, "Missing current removal-particle host");
    }
    return emit(host->context, entry->key, descriptor, false, false, error);
}

bool ab_block_removal_particles(const AbBlockRemovalEntry *entry,
                                const AbBlockRemovalEffects *cached_effects,
                                AbBlockRemovalParticlesHost *host, AbError *error) {
    if (!entry || !cached_effects || !host || (entry->key.size && !entry->key.data)) {
        return ab_fail(error, "Invalid removal particle input");
    }
    const AbBlockRemovalEntry captured = *entry;
    const AbBlockRemovalEffects cached = *cached_effects;
    if (!host->notify) {
        return ab_fail(error, "Missing removal notification host");
    }
    return host->notify(host->context, captured.object, error) &&
           removal_functions(captured.object, host, error) &&
           particles(&captured, &cached, host, error);
}
