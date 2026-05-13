#include "sensor.h"
#include "aabb.h"
#include "entity.h"

void check_sensors(world_t *world, size_t *world_size, uint32_t frame) {
  ITER_ENTITIES(*world, *world_size, entity, {
    if (entity->sensor && entity->aabb) {
      ITER_ENTITIES(*world, *world_size, other, {
        if (other->aabb && other->aabb->layer & entity->sensor->mask) {
          if (intersects_aabb_aabb(*entity->aabb, *other->aabb)) {
            entity->sensor->callback(world, world_size, frame, entity, other);
          }
        }
      });
    }
  });
}
