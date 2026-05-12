#ifndef TIMER_H_
#define TIMER_H_

#include "entity.h"

#include <stddef.h>
#include <stdint.h>

typedef struct timer {
  uint32_t timeout;
  uint32_t start;
} timer_component_t;

void check_timers(world_t world, size_t world_size, uint32_t frame);

#endif
