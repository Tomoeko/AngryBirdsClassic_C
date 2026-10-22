#ifndef AB_PARTICLE_DRAW_H
#define AB_PARTICLE_DRAW_H

#include "ab/particles.h"
#include "ab/object_sprite.h"
#include "ab/sprite_provider.h"

/* Original ordinary world-particle render state and Sprite rectangle. Keep
 * camera translation separate from sprite position before division by scale;
 * combining them changes binary32 rounding. Preserve pivot, rotation and scale
 * order. No opacity/lifetime fade is introduced here. Ordinary sprite metadata,
 * finite nonzero particle scale and finite used camera/pose values only.
 * Failure preserves outputs. Menu/level-layer/composite paths are separate. */
bool ab_particle_sprite_state(const AbParticle *particle, const AbSpriteRegion *region,
                              AbCoordinateCamera camera, AbImageRectangle *rectangle,
                              AbSpriteRenderState *state, AbError *error);
/* Level draw uses the same geometry/translation but multiplies the already
 * combined particle/camera scale by the positive requested layer, otherwise1.
 * Filtering belongs to the list consumer below. Material remains caller-owned.
 */
bool ab_level_particle_sprite_state(const AbParticle *particle,
                                    const AbSpriteRegion *region,
                                    AbCoordinateCamera camera, int32_t layer,
                                    AbImageRectangle *rectangle,
                                    AbSpriteRenderState *state, AbError *error);
/* Original menu draw ignores the world camera, writes positive-zero translation
 * and uses g_menu_particle_scale*particle.scale for both axes and position
 * division. Ordinary sprite, finite nonzero combined scale/used pose domain.
 * Failure preserves outputs, as above. */
bool ab_menu_particle_sprite_state(const AbParticle *particle,
                                   const AbSpriteRegion *region, float menu_scale,
                                   AbImageRectangle *rectangle,
                                   AbSpriteRenderState *state, AbError *error);

typedef struct {
    const AbSpriteProvider *provider;
    AbCoordinateCamera camera;
    uint32_t width, height;
    AbBlend blend;
    float color[4], opacity;
} AbParticleDrawStyle;

/* Append the normal world's retained particle list in vector order. Preserve
 * cached missing sprites; a nonzero retained handle requires the same immutable
 * preloaded provider binding. No loading, allocation, GPU work or sorting. The
 * caller supplies the effective material/color/opacity inherited at this draw
 * boundary. Source resources/provider must stay unchanged while particles live.
 * Clip geometry uses the native-matched ordinary Image rectangle path. A failed
 * append restores list.count; memory beyond it may change. Complete root
 * layer/material scheduling remains caller-owned. */
bool ab_particles_draw_world(const AbParticles *particles, AbParticleDrawStyle style,
                             AbDrawList *list, AbError *error);
/* Original drawLevelParticles retained-list traversal. Zero selects all rows;
 * any other layer selects exact matching rows and keeps their vector order.
 * The positive requested layer also multiplies camera/particle scale above.
 * No implicit draw ordering relative to theme/world/HUD or GPU material setup.
 * Share the same immutable provider and failure rollback rules as world draw.
 */
bool ab_particles_draw_level(const AbParticles *particles, AbParticleDrawStyle style,
                             int32_t layer, AbDrawList *list, AbError *error);
/* Draw the separate alternate/menu list in vector order. Pass the actual global
 * g_menu_particle_scale at this boundary. style.camera is unused. Inherit the
 * same provider/material/rollback rules; do not update particles here. */
bool ab_particles_draw_menu(const AbParticles *particles, AbParticleDrawStyle style,
                            float menu_scale, AbDrawList *list, AbError *error);

#endif
