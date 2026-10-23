#include "ab/classic_scene.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

struct AbClassicScene {
    AbClassicLevel *level;
    const AbSpriteAtlas *atlas;
    const AbSpriteProvider *provider;
    AbSceneSprite *items;
    size_t *order;
    size_t count, capacity;
    size_t bytes;
    uint32_t first_texture;
    AbBlend blend;
};

static bool binding_texture(const AbClassicScene *scene,
                            const AbSpriteAtlasBinding *binding, uint32_t *texture) {
    if (!binding->image || !binding->sprite.region) {
        return false;
    }
    if (scene->provider) {
        return scene->provider->texture(scene->provider->context, binding->image,
                                        texture) &&
               *texture != 0;
    }
    AbSpriteAtlasImage image;
    if (!ab_sprite_atlas_image(scene->atlas, binding->image_index, &image) ||
        image.image != binding->image) {
        return false;
    }
    *texture = scene->first_texture + (uint32_t)binding->image_index;
    return true;
}

static void apply_binding(const AbSpriteAtlasBinding *binding, uint32_t texture,
                          AbSceneSprite *item) {
    item->region = binding->sprite.region;
    item->texture = texture;
    item->texture_width = binding->image->width;
    item->texture_height = binding->image->height;
    item->unbound = false;
}

static bool find_sprite(const AbClassicScene *scene, AbSpan name,
                        AbSpriteAtlasBinding *binding) {
    return scene->provider
               ? scene->provider->sprite(scene->provider->context, name, binding)
               : ab_sprite_atlas_find(scene->atlas, name, binding);
}

static bool sprite_binding(AbClassicScene *scene, AbSpan name, AbSceneSprite *item,
                           AbError *error) {
    AbSpriteAtlasBinding binding;
    uint32_t texture;
    if (!find_sprite(scene, name, &binding) ||
        !binding_texture(scene, &binding, &texture)) {
        return ab_fail(error, "Classic sprite or its texture is absent");
    }
    apply_binding(&binding, texture, item);
    return true;
}

static bool initial_binding(AbClassicScene *scene, AbDataValue name,
                            AbSceneSprite *item, AbError *error) {
    if (name.kind != AB_DATA_STRING || (name.string.size && !name.string.data)) {
        return ab_fail(error, "Invalid initial classic sprite name");
    }
    if (!scene->provider) {
        return name.string.size && sprite_binding(scene, name.string, item, error);
    }
    AbSpriteAtlasBinding binding;
    if (name.string.size && find_sprite(scene, name.string, &binding)) {
        uint32_t texture;
        if (!binding_texture(scene, &binding, &texture)) {
            return ab_fail(error, "Initial classic sprite texture is absent");
        }
        apply_binding(&binding, texture, item);
        return true;
    }
    AbResolvedComposite composite;
    if (name.string.size && scene->provider->composite &&
        scene->provider->composite(scene->provider->context, name.string, &composite)) {
        return ab_fail(error, "Composite assets require a separate scene renderer");
    }
    item->unbound = true;
    return true;
}

static bool object_layer(AbClassicLevelView view, const AbLevelObject *object,
                         int32_t *output, AbError *error) {
    AbDataValue key = {.kind = AB_DATA_STRING,
                       .string = {(const uint8_t *)"z_order", 7}};
    const AbDataValue *value = ab_data_get(object->placement, key);
    AbDataEntry entry = {key, value ? *value : (AbDataValue){0}};
    AbDataTable overrides = {&entry, 1, 1, NULL};
    AbObjectDefinition definition;
    if (!ab_object_definition_open(view.catalog, object->definition_name, &overrides,
                                   &definition, error)) {
        return false;
    }
    float number;
    memcpy(&number, &definition.z_order.bits, sizeof(number));
    if (definition.z_order.kind != AB_DATA_NUMBER || !isfinite(number) ||
        number < -2147483648.0f || number >= 2147483648.0f) {
        return ab_fail(error, "Classic renderer layer is outside signed range");
    }
    /* createRendererForGameObject uses VCVT.S32.F32, truncating toward zero. */
    *output = (int32_t)number;
    return true;
}

static bool initial_items(AbClassicScene *scene, AbClassicLevelView view, AbBlend blend,
                          AbError *error) {
    for (size_t index = 0; index < scene->count; ++index) {
        const AbGameFrameObject *frame = view.frame->objects + index;
        AbSceneSprite *item = scene->items + index;
        *item = (AbSceneSprite){
            .pose = {frame->rendered.position, frame->rendered.angle, {1, 1}},
            .collider = frame->collider,
            .blend = blend,
            .visible = true};
        AbSpan object_name = view.bodies->objects[index].name;
        if (object_name.size == 6 && memcmp(object_name.data, "ground", 6) == 0) {
            item->visible = false;
            continue;
        }
        if (item->collider == 7) {
            continue;
        }
        AbDataValue name = view.initial_sprites[index].sprite;
        if (!initial_binding(scene, name, item, error) ||
            !object_layer(view, view.bodies->objects + index, &item->layer, error)) {
            return ab_fail(error, "Cannot initialize ordinary classic renderer");
        }
    }
    return ab_sprite_scene_order(scene->items, scene->count, scene->order, scene->count,
                                 error);
}

static bool open_scene(AbClassicLevel *level, const AbSpriteAtlas *atlas,
                       const AbSpriteProvider *provider, uint32_t first_texture,
                       AbBlend blend, size_t allocation_limit, AbClassicScene **output,
                       AbError *error) {
    if (!output) {
        return ab_fail(error, "Missing classic scene output");
    }
    *output = NULL;
    AbClassicLevelView view = ab_classic_level_view(level);
    size_t count = view.bodies ? view.bodies->count : 0;
    size_t capacity = view.bodies ? view.bodies->capacity : 0;
    if (!view.bodies || !view.frame || !view.catalog || !view.sprites_initialized ||
        (count &&
         (!view.bodies->objects || !view.initial_sprites || !view.frame->objects)) ||
        view.frame->count != count ||
        (provider ? !provider->sprite || !provider->texture
                  : !atlas || !first_texture ||
                        ab_sprite_atlas_count(atlas) >
                            (size_t)(UINT32_MAX - first_texture) + 1) ||
        (unsigned)blend > AB_BLEND_MULTIPLY ||
        allocation_limit < sizeof(AbClassicScene) || count > capacity ||
        capacity > (allocation_limit - sizeof(AbClassicScene)) /
                       (sizeof(AbSceneSprite) + sizeof(size_t))) {
        return ab_fail(error, "Invalid classic scene input or allocation limit");
    }
    AbClassicScene *scene = calloc(1, sizeof(*scene));
    if (!scene) {
        return ab_fail(error, "Cannot allocate classic scene owner");
    }
    scene->level = level;
    scene->atlas = atlas;
    scene->provider = provider;
    scene->count = count;
    scene->capacity = capacity;
    scene->blend = blend;
    scene->first_texture = first_texture;
    scene->bytes = sizeof(*scene) + capacity * (sizeof(AbSceneSprite) + sizeof(size_t));
    if (capacity) {
        scene->items = calloc(capacity, sizeof(*scene->items));
        scene->order = calloc(capacity, sizeof(*scene->order));
        if (!scene->items || !scene->order) {
            ab_fail(error, "Cannot allocate classic scene entries");
            goto release_scene;
        }
    }
    if (!initial_items(scene, view, blend, error)) {
        goto release_scene;
    }
    *output = scene;
    return true;

release_scene:
    ab_classic_scene_close(scene);
    return false;
}

bool ab_classic_scene_open(AbClassicLevel *level, const AbSpriteAtlas *atlas,
                           uint32_t first_texture, AbBlend blend,
                           size_t allocation_limit, AbClassicScene **output,
                           AbError *error) {
    return open_scene(level, atlas, NULL, first_texture, blend, allocation_limit,
                      output, error);
}

bool ab_classic_scene_open_resources(AbClassicLevel *level,
                                     const AbSpriteProvider *provider, AbBlend blend,
                                     size_t allocation_limit, AbClassicScene **output,
                                     AbError *error) {
    return open_scene(level, NULL, provider, 0, blend, allocation_limit, output, error);
}

void ab_classic_scene_close(AbClassicScene *scene) {
    if (scene) {
        free(scene->items);
        free(scene->order);
        free(scene);
    }
}

AbClassicSceneView ab_classic_scene_view(const AbClassicScene *scene) {
    return scene ? (AbClassicSceneView){scene->items, scene->order, scene->count}
                 : (AbClassicSceneView){0};
}

size_t ab_classic_scene_bytes(const AbClassicScene *scene) {
    return scene ? scene->bytes : 0;
}

static bool construct_object(AbClassicScene *scene, size_t object, float layer,
                             bool replacing, AbError *error) {
    AbClassicLevelView level = ab_classic_level_view(scene ? scene->level : NULL);
    if (!scene || (replacing ? object >= scene->count : object != scene->count) ||
        object >= scene->capacity || !level.bodies || object >= level.bodies->count ||
        level.frame->count != level.bodies->count || level.removed_objects[object] ||
        level.frame->objects[object].collider == 7 || !isfinite(layer) ||
        layer < -2147483648.0f || layer >= 2147483648.0f)
        return ab_fail(error, "Invalid dynamic ordinary renderer slot");
    AbGameFrameObject *frame = level.frame->objects + object;
    scene->items[object] = (AbSceneSprite){
        .pose = {frame->rendered.position, frame->rendered.angle, {1, 1}},
        .layer = (int32_t)layer,
        .collider = frame->collider,
        .blend = scene->blend,
        .visible = true,
        .unbound = true};
    if (!replacing)
        ++scene->count;
    return ab_sprite_scene_order(scene->items, scene->count, scene->order,
                                 scene->capacity, error);
}

bool ab_classic_scene_append_object(AbClassicScene *scene, size_t object, float layer,
                                    AbError *error) {
    return construct_object(scene, object, layer, false, error);
}

bool ab_classic_scene_recreate_object(AbClassicScene *scene, size_t object, float layer,
                                      AbError *error) {
    return construct_object(scene, object, layer, true, error);
}

static bool ordinary_object(const AbClassicScene *scene, size_t object) {
    if (!scene || object >= scene->count || scene->items[object].collider == 7) {
        return false;
    }
    AbSpan name = ab_classic_level_view(scene->level).bodies->objects[object].name;
    return name.size != 6 || memcmp(name.data, "ground", 6) != 0;
}

bool ab_classic_scene_set_sprite(AbClassicScene *scene, size_t object, AbSpan name,
                                 AbError *error) {
    if (!ordinary_object(scene, object) ||
        (!scene->items[object].region && !scene->provider)) {
        return ab_fail(error, "Invalid classic sprite object");
    }
    return sprite_binding(scene, name, scene->items + object, error);
}

bool ab_classic_scene_set_scale(AbClassicScene *scene, size_t object, AbVec2 scale,
                                AbError *error) {
    if (!scene || object >= scene->count || !isfinite(scale.x) || !isfinite(scale.y) ||
        scale.x == 0 || scale.y == 0) {
        return ab_fail(error, "Invalid classic object scale");
    }
    scene->items[object].pose.scale = scale;
    return true;
}

bool ab_classic_scene_set_visible(AbClassicScene *scene, size_t object, bool visible,
                                  AbError *error) {
    if (!scene || object >= scene->count ||
        (visible && ab_classic_level_view(scene->level).removed_objects[object]) ||
        (visible && !scene->items[object].region &&
         !(scene->provider && ordinary_object(scene, object) &&
           scene->items[object].unbound))) {
        return ab_fail(error, "Invalid classic visibility object");
    }
    scene->items[object].visible = visible;
    return true;
}

bool ab_classic_scene_sync_poses(AbClassicScene *scene, AbError *error) {
    if (!scene)
        return ab_fail(error, "Missing classic scene pose owner");
    AbClassicLevelView view = ab_classic_level_view(scene->level);
    if (view.bodies->count != scene->count || view.frame->count != scene->count) {
        return ab_fail(error, "Classic scene membership changed outside its owner");
    }
    for (size_t index = 0; index < scene->count; ++index) {
        AbSceneSprite *item = scene->items + index;
        if (view.removed_objects[index]) {
            item->visible = false;
            continue;
        }
        if (!item->visible || item->unbound || item->collider == 7) {
            continue;
        }
        const AbGameFrameObject *frame = view.frame->objects + index;
        if (frame->body != view.bodies->objects[index].physics.body ||
            !ab_world_has_body(view.world, frame->body)) {
            return ab_fail(error, "Missing active classic scene body");
        }
        item->pose.physics_position = frame->rendered.position;
        item->pose.angle_radians = frame->rendered.angle;
    }
    return true;
}

bool ab_classic_scene_sync_visuals(AbClassicScene *scene,
                                   const AbClassicVisuals *visuals, AbError *error) {
    AbClassicVisualView view = ab_classic_visuals_view(visuals);
    if (!scene || view.level != scene->level || view.atlas != scene->atlas ||
        view.provider != scene->provider || view.count != scene->count) {
        return ab_fail(error, "Classic visual and scene ownership differ");
    }
    AbClassicLevelView level = ab_classic_level_view(scene->level);
    for (size_t index = 0; index < scene->count; ++index) {
        if (level.removed_objects[index]) {
            continue;
        }
        const AbObjectVisual *object = ab_classic_visuals_object(visuals, index);
        if (object->attached && object->attached->composite) {
            return ab_fail(error, "Composite assets require a separate scene renderer");
        }
        if (object->attached && object->attached->sprite) {
            AbSpriteAtlasBinding binding;
            uint32_t texture;
            if (!ab_classic_visuals_binding(visuals, index, &binding) ||
                !binding_texture(scene, &binding, &texture)) {
                return ab_fail(error, "Invalid cached classic visual binding");
            }
        }
    }
    for (size_t index = 0; index < scene->count; ++index) {
        AbSpriteAtlasBinding binding;
        AbSceneSprite *item = scene->items + index;
        if (level.removed_objects[index]) {
            item->visible = false;
            continue;
        }
        item->unbound = !ab_classic_visuals_binding(visuals, index, &binding);
        if (!item->unbound) {
            uint32_t texture;
            if (!binding_texture(scene, &binding, &texture)) {
                return ab_fail(error, "Classic visual texture changed during sync");
            }
            apply_binding(&binding, texture, item);
        }
    }
    return true;
}

bool ab_classic_scene_draw(AbClassicScene *scene, AbCoordinateCamera camera,
                           uint32_t viewport_width, uint32_t viewport_height,
                           AbDrawList *list, AbError *error) {
    return scene ? ab_classic_scene_sync_poses(scene, error) &&
                       ab_sprite_scene_draw(scene->items, scene->count, scene->order,
                                            camera, viewport_width, viewport_height,
                                            list, error)
                 : ab_fail(error, "Missing classic scene");
}
