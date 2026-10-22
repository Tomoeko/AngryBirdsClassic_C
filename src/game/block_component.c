#include "ab/block_component.h"

#include <string.h>

static bool valid_table(const AbDataTable *table) {
    return table && table->count <= table->capacity &&
           (!table->count || table->entries);
}

static const AbDataValue *field(const AbDataTable *table, const char *name) {
    return ab_data_get(table,
                       (AbDataValue){.kind = AB_DATA_STRING,
                                     .string = {(const uint8_t *)name, strlen(name)}});
}

bool ab_block_component(const AbDataTable *definition, AbBlockComponentKind kind,
                        const AbDataTable **output, AbError *error) {
    static const char *const names[] = {"flame", "ambientAnimations",
                                        "ambientParticles"};
    if (!valid_table(definition) || !output ||
        (unsigned)kind >= AB_BLOCK_COMPONENT_COUNT) {
        return ab_fail(error, "Invalid block component request");
    }
    const AbDataTable *result = NULL;
    const AbDataValue *components = field(definition, "components");
    if (components && components->kind == AB_DATA_TABLE) {
        if (!valid_table(components->table)) {
            return ab_fail(error, "Invalid block components table");
        }
        const AbDataValue *value = field(components->table, names[kind]);
        if (value && value->kind == AB_DATA_TABLE) {
            if (!valid_table(value->table)) {
                return ab_fail(error, "Invalid selected block component table");
            }
            result = value->table;
        }
    }
    *output = result;
    return true;
}
