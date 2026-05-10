#ifndef PLATFORM_H_
#define PLATFORM_H_

#include "entity.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct platform {
  bool test;
} platform_t;

entity_t *add_platform(world_t *world, size_t *world_size, int32_t x,
                       int32_t y);

#endif
