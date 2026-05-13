#include "timer.h"

#include "entity.h"

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

void check_timers(world_t world, size_t world_size, uint32_t frame) {
  ITER_ENTITIES(world, world_size, entity, {
    timer_component_t *timer = entity->timer;

    if (!timer) {
      continue;
    }

    if (!timer->start) {
      timer->start = frame;
    }

    if (timer->start + timer->timeout <= frame) {
      delete_entity(entity, world, world_size);
    }
  })
}
