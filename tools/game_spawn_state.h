#ifndef AB_GAME_SPAWN_STATE_H
#define AB_GAME_SPAWN_STATE_H

#include "ab/block_removal_spawn.h"

enum { GAME_SPAWN_NAME_BYTES = 256 };

typedef struct {
    AbBlockRemovalSpawnHost host;
    uint8_t scratch[GAME_SPAWN_NAME_BYTES];
    uint8_t (*names)[GAME_SPAWN_NAME_BYTES];
    size_t capacity, used;
} PreviewSpawns;

#endif
