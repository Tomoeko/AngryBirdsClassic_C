#include "ab/ui_level_services.h"

#include <string.h>

static bool used_in(void *context, AbDataValue free_power, const AbDataTable **output,
                    AbError *error) {
    AbUiLevelServices *services = context;
    if (!services || !services->local_settings || !output || services->empty.count ||
        services->empty.capacity || services->empty.entries)
        return ab_fail(error, "Missing or invalid actual level UI settings owner");
    bool free = free_power.kind != AB_DATA_NIL &&
                (free_power.kind != AB_DATA_BOOLEAN || free_power.bits != 0);
    const char *name = free ? "freeEaglesUsedIn" : "eaglesUsedIn";
    const AbDataValue *value =
        ab_data_get(services->local_settings,
                    (AbDataValue){.kind = AB_DATA_STRING,
                                  .string = {(const uint8_t *)name, strlen(name)}});
    if (!value || value->kind == AB_DATA_NIL ||
        (value->kind == AB_DATA_BOOLEAN && !value->bits)) {
        *output = &services->empty;
        return true;
    }
    if (value->kind != AB_DATA_TABLE || !value->table)
        return ab_fail(error, "Saved eagle-use collection is not a table");
    *output = value->table;
    return true;
}

AbUiLevelButtonHost ab_ui_level_services_host(AbUiLevelServices *services) {
    return (AbUiLevelButtonHost){.episodes = services && services->statistics
                                                 ? services->statistics->episodes
                                                 : NULL,
                                 .statistics = services ? services->statistics : NULL,
                                 .context = services,
                                 .eagle_used_in = used_in};
}
