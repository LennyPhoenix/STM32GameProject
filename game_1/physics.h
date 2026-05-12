#ifndef PHYSICS_H_
#define PHYSICS_H_

#include "entity.h"
#include <stddef.h>

#define ENVIRONMENT_LAYER (1 << 0)
#define PLAYER_LAYER (1 << 1)
#define ENEMY_LAYER (1 << 2)

/// system to apply gravity to an entity
void apply_gravity(world_t world, size_t world_size);
/// system to move entities with velocity
void move_entities(world_t *world, size_t *world_size);

/// extends an aabb in the direction of a given velocity
aabb_t velocity_aabb_broad_phase(aabb_t aabb, velocity_t velocity);
/// moves an entity according to its velocity, sliding across any colliders it
/// hits
void move_and_slide(world_t *world, size_t *world_size, entity_t *entity);

#endif
