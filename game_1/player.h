#ifndef PLAYER_H_
#define PLAYER_H_

#include "entity.h"

#include <math.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct player {
  bool jumping, shooting, on_ground, dead;
  double_t aim_right, aim_down;
} player_t;

entity_t *new_player(world_t *world, size_t *world_size);
void update_player_velocity(world_t world, size_t world_size);

#endif
