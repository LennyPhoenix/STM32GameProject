#ifndef ENTITY_H_
#define ENTITY_H_

#include <stddef.h>
#include "aabb.h"
#include "velocity.h"

#define COMPONENTS_TABLE                                                       \
  X(aabb_t, aabb)                                                              \
  X(velocity_t, velocity)

/// an entity
typedef struct entity {
#define X(component_type, component_name) component_type *component_name;
  COMPONENTS_TABLE
#undef X
} entity_t;

/// list of `entity_t *`
typedef entity_t **world_t;

/// instantiates a new entity
entity_t *new_entity(world_t *world, size_t *world_size);
/// deletes an entity
void delete_entity(entity_t *entity, world_t world, size_t world_size);

#endif
