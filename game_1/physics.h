#ifndef PHYSICS_H_
#define PHYSICS_H_

#include "entity.h"
#include <stddef.h>

#define ENVIRONMENT_LAYER (1 << 0)
#define PLAYER_LAYER (1 << 1)
#define ENEMY_LAYER (1 << 2)

void apply_gravity(world_t world, size_t world_size);
void move_entities(world_t world, size_t world_size);

aabb_t velocity_aabb_broad_phase(aabb_t aabb, velocity_t velocity);
void move_and_slide(world_t world, size_t world_size, entity_t *entity);

#endif
