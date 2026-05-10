#include "platform.h"
#include "aabb.h"
#include "entity.h"
#include "physics.h"

#include <stdint.h>

entity_t *add_platform(world_t *world, size_t *world_size, int32_t x,
                       int32_t y) {
  entity_t *platform = new_entity(world, world_size);

  aabb_t *aabb = init_aabb(platform);
  aabb->x = x;
  aabb->y = 150 + y;
  aabb->width = 180;
  aabb->height = 100;
  aabb->layer = ENVIRONMENT_LAYER;

  init_platform(platform);

  return platform;
}
