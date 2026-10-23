#ifndef AB_CLASSIC_RENDER_SCENE_H
#define AB_CLASSIC_RENDER_SCENE_H

#include "ab/classic_scene.h"
#include "ab/lua_asset_renderer.h"

typedef struct AbClassicRenderScene AbClassicRenderScene;

typedef enum {
    AB_CLASSIC_RENDER_OBJECT,
    AB_CLASSIC_RENDER_LUA,
} AbClassicRenderKind;

typedef struct {
    AbClassicRenderKind kind;
    size_t index;
} AbClassicRenderRoute;

typedef struct {
    const AbSpriteProvider *provider;
    uint32_t *id_counter;
    size_t extra_renderers, storage_bytes;
    AbBlend object_blend;
} AbClassicRenderSceneConfig;

typedef struct {
    const AbSceneMember *members;
    const AbClassicRenderRoute *routes;
    const AbSceneId *initial_ids;
    const AbLuaAssetRenderer *extra_renderers;
    size_t count, initial_count, extra_count;
    /* initial_ids is stable object-index storage for this whole prefix,
     * including dynamic objects. initial_count retains its load-time meaning. */
    size_t object_count;
} AbClassicRenderSceneView;

/* Own one ordered scene for ordinary object renderers and a bounded number of
 * later ordinary Lua asset renderers. Reserve the level's full object capacity
 * for explicit dynamic object registration. Ground has no renderer.
 * Initial IDs follow body-object creation order; layer order stays separate.
 * Collider 7 membership is not recovered for this owner and rejects explicitly.
 * Level, provider/context, source resources/textures and the application-wide
 * counter stay borrowed until close. Close before releasing those owners.
 *
 * Extra renderer slots remain stable until close, including after scene removal;
 * they are not recycled while callers may hold property views. Resource names
 * stay borrowed as in the typed constructor. Resource loading, script numeric
 * maps/GC, dynamic physics/script registration and destruction callbacks remain
 * separate. storage_bytes bounds the owner, fixed arrays and owned ordinary scene. No
 * per-frame allocation. Failed open releases storage; consumed IDs survive.
 */
bool ab_classic_render_scene_open(AbClassicLevel *level,
                                  AbClassicRenderSceneConfig config,
                                  AbClassicRenderScene **output, AbError *error);
void ab_classic_render_scene_close(AbClassicRenderScene *scene);
size_t ab_classic_render_scene_bytes(const AbClassicRenderScene *scene);
AbClassicRenderSceneView
ab_classic_render_scene_view(const AbClassicRenderScene *scene);

/* Borrow the ordinary scene for visual/scale/visibility actions. Keep its
 * membership and layers fixed; this owner manages ordered registration. */
AbClassicScene *ab_classic_render_scene_objects(AbClassicRenderScene *scene);

/* Consume one stable extra slot, construct native defaults, bind its resource,
 * assign an ID and register in layer order. The caller can then publish resolved
 * position/angle and maintain its attached-object/script registry. Output starts
 * NULL; resource failures may consume an ID without adding a renderer. */
bool ab_classic_render_scene_create_lua(AbClassicRenderScene *scene, AbSpan name,
                                        float layer, AbLuaAssetRenderer **output,
                                        AbError *error);

/* Actual ordinary createRendererForGameObject sequence for the next new stable
 * level slot or a detached row after native body replacement: consume renderer ID,
 * create/register renderer, attach asset, then publish/resolve sprite. Layers truncate
 * toward zero. Both this scene and the visual owner reserve the level's full capacity
 * at open. Resource errors retain earlier ID/membership/attachment/property effects and
 * require session discard. No script, behavior, audio or dynamic physics-adapter
 * registration occurs. Missing sprites retain an empty asset through the existing
 * visual policy. Composites and collider 7 remain unsupported. No frame allocation/GPU
 * calls. */
bool ab_classic_render_scene_create_object(AbClassicRenderScene *scene,
                                           AbClassicVisuals *visuals, size_t object,
                                           AbSpan sprite, float layer, AbError *error);
bool ab_classic_render_scene_remove_number(AbClassicRenderScene *scene, float id,
                                           bool *found, AbError *error);

/* Native Scene.remove(RenderableObject&) selects the renderer's current layer
 * and exact identity. IDs may alias and are not used by this boundary. */
AbClassicLevel *ab_classic_render_scene_level(const AbClassicRenderScene *scene);
bool ab_classic_render_scene_remove_object(AbClassicRenderScene *scene, size_t object,
                                           bool *found, AbError *error);

/* At the recovered post-physics boundary, remove forgotten initial objects from
 * scene membership and sync surviving rendered poses. Prior effects survive a
 * later failure. This does not advance physics, camera, visuals or scripts. */
bool ab_classic_render_scene_sync(AbClassicRenderScene *scene, AbError *error);

/* CameraNative.updateGFXEffects visits every scene member in layer/vector order.
 * These owned ordinary GameObjectRenderer instances have the native constructor's
 * absent primary effect and empty effect vector. Their update therefore has no
 * effect; owned LuaAssetRenderer.update is the original empty method. No GFX
 * attachment API is supplied by this owner. Adding such an API must extend this
 * phase before it can publish effects. Duration is unused in this closed shape,
 * including nonfinite values. Does not sync poses/visuals, draw or allocate. */
bool ab_classic_render_scene_update_gfx(AbClassicRenderScene *scene, float seconds,
                                        AbError *error);

/* Append ordinary objects and Lua sprites in one shared order, using the last
 * synchronized poses. No lookup/allocation. Failure restores list.count while
 * storage beyond it may change. All resource/source owners remain stable. */
bool ab_classic_render_scene_draw(const AbClassicRenderScene *scene,
                                  AbCoordinateCamera camera, uint32_t viewport_width,
                                  uint32_t viewport_height, AbDrawList *list,
                                  AbError *error);

#endif
