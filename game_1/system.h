#ifndef SYSTEM_H_
#define SYSTEM_H_

#include "entity.h"

#include <stddef.h>
#include <stdint.h>

void run_systems(world_t *world, size_t *world_size, uint32_t frame);

#endif
