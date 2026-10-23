#include "ab/classic_visuals.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    AbObjectVisual object;
    AbSpriteAssetState asset;
    AbSpriteAtlasBinding binding;
    AbClassicVisuals *owner;
    size_t index;
} VisualEntry;

struct AbClassicVisuals {
    AbClassicLevel *level;
    AbClassicVisualResources resources;
    VisualEntry *entries;
    uint8_t *names;
    AbSpan *interned;
    size_t name_count, name_slots;
    size_t count, capacity, bytes;
};

static bool ordinary(void *context, AbSpan name, const AbSpriteRegion **output,
                     AbError *error) {
    VisualEntry *entry = context;
    AbSpriteAtlasBinding binding;
    (void)error;
    *output = NULL;
    AbClassicVisualResources resources = entry->owner->resources;
    bool found =
        resources.provider
            ? resources.provider->sprite(resources.provider->context, name, &binding)
            : ab_sprite_atlas_find(resources.atlas, name, &binding);
    if (found) {
        entry->binding = binding;
        *output = binding.sprite.region;
    }
    return true;
}

static bool composite(void *context, AbSpan name, const AbComposite **output,
                      AbError *error) {
    VisualEntry *entry = context;
    AbResolvedComposite binding;
    (void)error;
    *output = NULL;
    AbClassicVisualResources resources = entry->owner->resources;
    bool found =
        resources.provider
            ? resources.provider->composite &&
                  resources.provider->composite(resources.provider->context, name,
                                                &binding)
            : resources.composites &&
                  ab_composite_resources_find(resources.composites, name, &binding);
    if (found) {
        *output = binding.source;
    }
    return true;
}

static void missing(void *context, AbSpan name) {
    VisualEntry *entry = context;
    AbClassicVisualResources resources = entry->owner->resources;
    if (resources.missing) {
        resources.missing(resources.context, entry->index, name);
    }
}

static AbSpriteAssetHost host(VisualEntry *entry) {
    return (AbSpriteAssetHost){entry, ordinary, composite, missing};
}

static bool initialize_entries(AbClassicVisuals *visuals, AbClassicLevelView level,
                               AbError *error) {
    for (size_t index = 0; index < visuals->count; ++index) {
        VisualEntry *entry = visuals->entries + index;
        entry->owner = visuals;
        entry->index = index;
        AbSpan name = level.bodies->objects[index].name;
        if (name.size && !name.data) {
            return ab_fail(error, "Invalid classic visual object name");
        }
        bool ground = name.size == 6 && memcmp(name.data, "ground", 6) == 0;
        bool attached = !ground && level.frame->objects[index].collider != 7;
        entry->object.attached = attached ? &entry->asset : NULL;
        AbDataValue sprite = level.initial_sprites[index].sprite;
        if (sprite.kind == AB_DATA_NIL && !attached) {
            continue;
        }
        if (sprite.kind != AB_DATA_STRING ||
            !ab_classic_visuals_set_sprite(visuals, index, sprite.string, error)) {
            return ab_fail(error, "Unsupported initial classic visual sprite");
        }
    }
    return true;
}

bool ab_classic_visuals_open(AbClassicLevel *level, AbClassicVisualResources resources,
                             AbClassicVisuals **output, AbError *error) {
    if (!output) {
        return ab_fail(error, "Missing classic visual output");
    }
    *output = NULL;
    AbClassicLevelView view = ab_classic_level_view(level);
    size_t count = view.bodies ? view.bodies->count : 0;
    size_t capacity = view.bodies ? view.bodies->capacity : 0;
    if (!view.bodies || !view.frame || view.frame->count != count ||
        !view.sprites_initialized ||
        (count &&
         (!view.bodies->objects || !view.frame->objects || !view.initial_sprites)) ||
        (resources.provider
             ? !resources.provider->sprite || resources.atlas || resources.composites
             : !resources.atlas) ||
        resources.storage_bytes < sizeof(AbClassicVisuals) || count > capacity ||
        capacity > (resources.storage_bytes - sizeof(AbClassicVisuals)) /
                       sizeof(VisualEntry)) {
        return ab_fail(error, "Invalid classic visual resources or storage limit");
    }
    AbClassicVisuals *visuals = calloc(1, sizeof(*visuals));
    if (!visuals) {
        return ab_fail(error, "Cannot allocate classic visual owner");
    }
    visuals->level = level;
    visuals->resources = resources;
    visuals->count = count;
    visuals->capacity = capacity;
    visuals->bytes = sizeof(*visuals) + capacity * sizeof(VisualEntry);
    size_t name_bytes = 0;
    if (capacity && resources.name_capacity) {
        if (resources.name_capacity > SIZE_MAX - sizeof(AbSpan)) {
            ab_fail(error, "Classic visual name slots exceed supported size");
            goto release_visuals;
        }
        visuals->name_slots = (resources.storage_bytes - visuals->bytes) /
                              (resources.name_capacity + sizeof(AbSpan));
        name_bytes = visuals->name_slots * resources.name_capacity;
        visuals->bytes += name_bytes + visuals->name_slots * sizeof(AbSpan);
    }
    if (capacity) {
        visuals->entries = calloc(capacity, sizeof(*visuals->entries));
        if (!visuals->entries) {
            ab_fail(error, "Cannot allocate classic visual entries");
            goto release_visuals;
        }
    }
    if (name_bytes) {
        visuals->names = malloc(name_bytes);
        visuals->interned = calloc(visuals->name_slots, sizeof(*visuals->interned));
        if (!visuals->names || !visuals->interned) {
            ab_fail(error, "Cannot allocate classic visual immutable names");
            goto release_visuals;
        }
    }
    if (!initialize_entries(visuals, view, error)) {
        goto release_visuals;
    }
    *output = visuals;
    return true;

release_visuals:
    ab_classic_visuals_close(visuals);
    return false;
}

void ab_classic_visuals_close(AbClassicVisuals *visuals) {
    if (visuals) {
        free(visuals->interned);
        free(visuals->names);
        free(visuals->entries);
        free(visuals);
    }
}

AbClassicVisualView ab_classic_visuals_view(const AbClassicVisuals *visuals) {
    return visuals ? (AbClassicVisualView){visuals->level, visuals->resources.atlas,
                                           visuals->resources.composites,
                                           visuals->count, visuals->resources.provider}
                   : (AbClassicVisualView){0};
}

size_t ab_classic_visuals_bytes(const AbClassicVisuals *visuals) {
    return visuals ? visuals->bytes : 0;
}

bool ab_classic_visuals_attach_object(AbClassicVisuals *visuals, size_t object,
                                      AbError *error) {
    AbClassicLevelView level = ab_classic_level_view(visuals ? visuals->level : NULL);
    if (!visuals || object > visuals->count || object >= visuals->capacity ||
        (object < visuals->count && visuals->entries[object].object.attached) ||
        !level.bodies || object >= level.bodies->count ||
        object >= level.properties->count || level.removed_objects[object] ||
        level.frame->objects[object].collider == 7)
        return ab_fail(error, "Invalid new ordinary visual attachment");
    AbSpan name = level.bodies->objects[object].name;
    if (name.size == 6 && !memcmp(name.data, "ground", 6))
        return ab_fail(error, "Ground has no ordinary visual attachment");
    VisualEntry *entry = visuals->entries + object;
    *entry = (VisualEntry){0};
    entry->owner = visuals;
    entry->index = object;
    entry->object.attached = &entry->asset;
    if (object == visuals->count)
        ++visuals->count;
    return true;
}

const AbObjectVisual *ab_classic_visuals_object(const AbClassicVisuals *visuals,
                                                size_t object) {
    return visuals && object < visuals->count ? &visuals->entries[object].object : NULL;
}

bool ab_classic_visuals_binding(const AbClassicVisuals *visuals, size_t object,
                                AbSpriteAtlasBinding *output) {
    if (!visuals || object >= visuals->count || !output) {
        return false;
    }
    const VisualEntry *entry = visuals->entries + object;
    if (entry->object.attached != &entry->asset || !entry->asset.sprite ||
        entry->asset.sprite != entry->binding.sprite.region) {
        return false;
    }
    *output = entry->binding;
    return true;
}

bool ab_classic_visuals_detach(AbClassicVisuals *visuals, size_t object,
                               AbError *error) {
    if (!visuals || object >= visuals->count)
        return ab_fail(error, "Unknown classic visual removal object");
    visuals->entries[object].object.attached = NULL;
    return true;
}

static bool intern_name(AbClassicVisuals *visuals, AbSpan name, AbSpan *output,
                        AbError *error) {
    if (!name.size) {
        *output = (AbSpan){0};
        return true;
    }
    for (size_t index = 0; index < visuals->name_count; ++index) {
        AbSpan existing = visuals->interned[index];
        if (existing.size == name.size &&
            !memcmp(existing.data, name.data, name.size)) {
            *output = existing;
            return true;
        }
    }
    if (visuals->name_count == visuals->name_slots)
        return ab_fail(error, "Classic visual immutable name storage is full");
    uint8_t *destination =
        visuals->names + visuals->name_count * visuals->resources.name_capacity;
    memmove(destination, name.data, name.size);
    AbSpan owned = {destination, name.size};
    visuals->interned[visuals->name_count++] = owned;
    *output = owned;
    return true;
}

bool ab_classic_visuals_set_sprite(AbClassicVisuals *visuals, size_t object,
                                   AbSpan name, AbError *error) {
    if (!visuals || object >= visuals->count || (name.size && !name.data) ||
        name.size > visuals->resources.name_capacity) {
        return ab_fail(error, "Invalid classic visual object or sprite name");
    }
    AbClassicLevelView level = ab_classic_level_view(visuals->level);
    if (!level.properties || object >= level.properties->count)
        return ab_fail(error, "Missing classic visual physical-property binding");
    AbDataTable *physical = &level.properties->objects[object].physical;
    AbDataValue sprite_key = {.kind = AB_DATA_STRING,
                              .string = {(const uint8_t *)"sprite", 6}};
    if (physical->count > physical->capacity ||
        (physical->count && !physical->entries) ||
        (!ab_data_get(physical, sprite_key) && physical->count == physical->capacity))
        return ab_fail(error, "Classic visual physical sprite storage is full");
    VisualEntry *entry = visuals->entries + object;
    AbSpan owned = {0};
    if (!intern_name(visuals, name, &owned, error))
        return false;
    AbObjectPropertyAccess access = {
        .physics_object = {.kind = AB_DATA_TABLE, .table = physical}};
    return ab_object_property_set(
               &access, sprite_key.string,
               (AbDataValue){.kind = AB_DATA_STRING, .string = owned}, error) &&
           ab_object_visual_set_sprite(&entry->object, owned, host(entry), error);
}

static AbDataValue field(const AbDataTable *table, const char *name) {
    const AbDataValue *value = ab_data_get(
        table, (AbDataValue){.kind = AB_DATA_STRING,
                             .string = {(const uint8_t *)name, strlen(name)}});
    return value ? *value : (AbDataValue){0};
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL &&
           (value.kind != AB_DATA_BOOLEAN || value.bits != 0);
}

bool ab_classic_visuals_damage_names(AbClassicVisuals *visuals, size_t object,
                                     AbStringOrderWorkspace *workspace,
                                     AbDamageSpriteNames *output, AbError *error) {
    if (!visuals || object >= visuals->count || !output)
        return ab_fail(error, "Invalid classic damage-sprite query");
    AbClassicLevelView level = ab_classic_level_view(visuals->level);
    const AbDataTable *physical = &level.properties->objects[object].physical;
    AbDataValue base = field(physical, "sprite");
    AbDamageSpriteNames selected = {.sprite = base, .blink = base, .smile = base};
    AbDataValue name = field(physical, "definition");
    AbDataValue blocks = field(level.catalog, "blocks");
    if (name.kind == AB_DATA_NIL ||
        (name.kind == AB_DATA_STRING && !name.string.size) || !truthy(blocks)) {
        *output = selected;
        return true;
    }
    if (blocks.kind != AB_DATA_TABLE || !blocks.table)
        return ab_fail(error, "Classic damage-sprite definitions are not a table");
    const AbDataValue *definition = ab_data_get(blocks.table, name);
    if (!definition || definition->kind == AB_DATA_NIL) {
        *output = selected;
        return true;
    }
    if (definition->kind != AB_DATA_TABLE || !definition->table)
        return ab_fail(error, "Classic damage-sprite definition is not a table");
    AbDataValue ranges = field(physical, "damageSprites");
    if (!truthy(ranges))
        ranges = field(definition->table, "damageSprites");
    AbDataValue strength = field(physical, "strength");
    if (ranges.kind == AB_DATA_NIL || strength.kind == AB_DATA_NIL) {
        *output = selected;
        return true;
    }
    if (ranges.kind != AB_DATA_TABLE || !ranges.table || !workspace ||
        !level.catalog_data)
        return ab_fail(error, "Unsupported classic damage-sprite range binding");
    const AbDataTableLayout *layout =
        ab_lua_table_layout(level.catalog_data, ranges.table);
    if (!layout || !layout->no_nil_assignments ||
        !ab_data_string_order(ranges.table, layout->hash_capacity, workspace, error))
        return ab_fail(error, "Unsupported live damage-sprite range order");
    AbDataValue initial = field(physical, "initialStrength");
    AbDataValue denominator = field(definition->table, "strength");
    if (!truthy(initial) && denominator.kind == AB_DATA_NIL) {
        AbObjectDefinition normalized;
        if (name.kind != AB_DATA_STRING ||
            !ab_object_definition_open(level.catalog, name.string, NULL, &normalized,
                                       error))
            return ab_fail(error, "Unsupported damage-sprite material strength");
        denominator =
            ab_object_property(&normalized, (AbSpan){(const uint8_t *)"strength", 8});
    }
    return ab_damage_sprite_names(base, strength, initial, denominator, ranges.table,
                                  workspace->indices, output, error);
}

bool ab_classic_visuals_update_damage(AbClassicVisuals *visuals, size_t object,
                                      AbStringOrderWorkspace *workspace,
                                      AbDamageSpriteNames *output, AbError *error) {
    if (!output)
        return ab_fail(error, "Missing classic damage-sprite output");
    AbDamageSpriteNames names;
    if (!ab_classic_visuals_damage_names(visuals, object, workspace, &names, error))
        return false;
    AbClassicLevelView level = ab_classic_level_view(visuals->level);
    const AbDataTable *physical = &level.properties->objects[object].physical;
    if (!truthy(field(physical, "dontUpdateDamageSprite")) &&
        !truthy(field(physical, "frozenSolid"))) {
        if (names.sprite.kind != AB_DATA_STRING)
            return ab_fail(error, "Unsupported live damage-sprite name");
        if (!ab_classic_visuals_set_sprite(visuals, object, names.sprite.string, error))
            return false;
    }
    *output = names;
    return true;
}
