#ifndef JUMP_H_
#define JUMP_H_

#include "entity.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct jump {
  bool jumping;
  uint32_t power;
} jump_t;

void apply_jumps(world_t world, size_t world_size);

#endif
