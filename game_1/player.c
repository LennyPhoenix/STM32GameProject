#include "player.h"
#include "aabb.h"
#include "entity.h"
#include "gravity.h"
#include "player_entity.h"
#include "velocity.h"
#include <math.h>
#include <stdlib.h>

entity_t *new_player(world_t *world, size_t *world_size) {
  entity_t *player = new_entity(world, world_size);

  aabb_t *aabb = init_aabb(player);
  aabb->x = 0;
  aabb->y = 200;
  aabb->width = 16;
  aabb->height = 32;

  velocity_t *velocity = init_velocity(player);
  velocity->x = 5;

  gravity_t *gravity = init_gravity(player);
  gravity->g = 1;

  init_player(player);

  return player;
}
