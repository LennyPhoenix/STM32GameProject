#ifndef PLATFORM_H_
#define PLATFORM_H_

#include "entity.h"

#include <stdbool.h>
#include <stdint.h>

/// platforms don't yet store any data
typedef struct platform {
  bool reached;
} platform_t;

/// instantiates a new platform entity
entity_t *new_platform(world_t *world, size_t *world_size, int32_t x,
                       int32_t y);

#endif
