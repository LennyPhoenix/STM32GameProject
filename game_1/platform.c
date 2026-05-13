#include "platform.h"
#include "aabb.h"
#include "entity.h"
#include "physics.h"
#include "render.h"

#include <stdint.h>

entity_t *new_platform(world_t *world, size_t *world_size, int32_t x,
                       int32_t y) {
  entity_t *platform = new_entity(world, world_size);

  // a platform needs a collider
  aabb_t *aabb = init_aabb(platform);
  aabb->x = x;
  aabb->y = 150 + y;
  aabb->width = 180;
  aabb->height = 100;
  aabb->layer = ENVIRONMENT_LAYER;

  // and a renderer
  draw_rect_t *draw_rect = init_draw_rect(platform);
  draw_rect->colour = 1;

  init_platform(platform);

  return platform;
}
