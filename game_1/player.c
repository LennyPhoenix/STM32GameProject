#include "player.h"
#include "aabb.h"
#include "entity.h"
#include <math.h>
#include <stdlib.h>

#define MAX_HORIZONTAL_SPEED (50)
#define MIN_VERTICAL_SPEED (-50)

entity_t *new_player(world_t *world, size_t *world_size) {
  entity_t *player = new_entity(world, world_size);

  player->aabb = calloc(sizeof(aabb_t), 1);
  player->aabb->x = 50;
  player->aabb->y = 50;
  player->aabb->width = 32;
  player->aabb->height = 64;

  return player;
}
