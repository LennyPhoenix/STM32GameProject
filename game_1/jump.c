#include "jump.h"
#include "entity.h"
#include "velocity.h"

#include <stdint.h>

void apply_jumps(world_t world, size_t world_size) {
  ITER_ENTITIES(world, world_size, entity, {
    if (entity->jump && entity->velocity && entity->jump->jumping) {
      entity->jump->jumping = false;
      entity->velocity->y = -(int32_t)entity->jump->power;
    }
  });
}
