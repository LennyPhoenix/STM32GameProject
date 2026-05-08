#include "physics.h"

#include <stdint.h>

void apply_gravity(world_t world, size_t world_size) {
  ITER_ENTITIES(world, world_size, entity, {
    if (entity->gravity && entity->velocity) {
      entity->velocity->y -= entity->gravity->g;
    }
  });
}

void move_entities(world_t world, size_t world_size) {
  ITER_ENTITIES(world, world_size, entity, {
    if (entity->aabb && entity->velocity) {
      entity->aabb->x += entity->velocity->x;
      entity->aabb->y += entity->velocity->y;
    }
  });
}
