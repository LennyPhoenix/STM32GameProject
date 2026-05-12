#ifndef GAME_ENTITY_H_
#define GAME_ENTITY_H_

#include "entity.h"

#include <stdint.h>

typedef struct game_component {
  bool game_over;
  uint32_t score;
} game_component_t;

void new_game(world_t *world, size_t *world_size);

#endif
