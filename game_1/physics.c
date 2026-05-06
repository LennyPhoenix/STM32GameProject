#include "physics.h"

void move_entities(world_t world, size_t world_size) {
  for (size_t i = 0; i < world_size; i++) {
    entity_t *entity = world[i];
    if (entity->aabb && entity->velocity) {
      entity->aabb->x += entity->velocity->x;
      entity->aabb->y += entity->velocity->y;
    }
  }
}
