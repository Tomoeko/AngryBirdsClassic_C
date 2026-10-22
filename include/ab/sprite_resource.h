#ifndef AB_SPRITE_RESOURCE_H
#define AB_SPRITE_RESOURCE_H

#include "ab/resource_owners.h"
#include "ab/sprite_atlas.h"

typedef struct AbSpriteResource AbSpriteResource;

/* Own one decoded SPRT source, its final named regions and its decoded image.
 * Expose real retain/release/image-unload callbacks for AbResourceOwners. Borrow
 * the archive until the last reference is released. The caller receives one
 * initial reference; close releases it, independently of retained map references.
 * allocation_limit bounds retained metadata/members/pixels; entry_limit bounds
 * temporary ZIP payloads, with codec workspace separately bounded. No VM or GPU
 * objects occur. Serialize references, unloading and view access. Output is NULL
 * on failure and partial allocations are released. */
bool ab_sprite_resource_open(const AbZip *archive, const char *metadata_path,
                             size_t allocation_limit, size_t entry_limit,
                             AbSpriteResource **output, AbError *error);
void ab_sprite_resource_close(AbSpriteResource *resource);

/* Views borrow this resource while a reference remains. Atlas/image views expire
 * on image unload; metadata/source members remain until the last release. The
 * byte count drops when image storage is unloaded. No allocation or I/O. */
const AbResourceSource *ab_sprite_resource_source(const AbSpriteResource *resource);
/* Recognize this module's source descriptor; other source providers return NULL.
 * The descriptor must remain live. Returned metadata views allocate nothing. */
const AbSpriteResource *ab_sprite_resource_from_source(const AbResourceSource *source);
bool ab_sprite_resource_member(const AbSpriteResource *resource,
                               const AbResourceMember *member, AbSpriteBinding *output);
const AbSpriteAtlas *ab_sprite_resource_atlas(const AbSpriteResource *resource);
AbSpan ab_sprite_resource_path(const AbSpriteResource *resource);
size_t ab_sprite_resource_bytes(const AbSpriteResource *resource);

/* Resolve only an active registry binding owned by this resource. An unloaded
 * image or a different source/composite masks it without a fallback. Failure
 * preserves output; registry lookup and returned pixels allocate nothing. */
bool ab_sprite_resource_find(const AbSpriteResource *resource,
                             const AbSpriteRegistry *registry, AbSpan name,
                             AbSpriteAtlasBinding *output);

#endif
