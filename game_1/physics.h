#ifndef PHYSICS_H_
#define PHYSICS_H_

#include "entity.h"
#include <stddef.h>

void apply_gravity(world_t world, size_t world_size);
void move_entities(world_t world, size_t world_size);

#endif
