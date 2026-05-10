#include "system.h"
#include "aabb.h"
#include "entity.h"
#include "physics.h"
#include "player.h"
#include "render.h"
#include <stddef.h>

/// deletes entities that have passed the top, bottom, or left sides of the
/// screen
void delete_entities(world_t world, size_t world_size) {
  ITER_ENTITIES(world, world_size, entity, {
    if (entity->aabb &&
        (entity->aabb->x + entity->aabb->width <= 0 || entity->aabb->y >= 240 ||
         entity->aabb->y + entity->aabb->height < 0)) {
      delete_entity(entity, world, world_size);
    }
  });
}

void run_systems(world_t *world, size_t *world_size, uint32_t frame) {
  update_player_velocity(*world, *world_size);
  apply_gravity(*world, *world_size);
  move_entities(*world, *world_size);
  realign_aabbs_to_player(*world, *world_size);
  delete_entities(*world, *world_size);
  render_aabbs(*world, *world_size);
}
