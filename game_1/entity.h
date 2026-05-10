#ifndef ENTITY_H_
#define ENTITY_H_

#include <stddef.h>

#define COMPONENTS_TABLE                                                       \
  X(aabb_t, aabb)                                                              \
  X(velocity_t, velocity)                                                      \
  X(gravity_t, gravity)                                                        \
  X(player_t, player)

#define X(component_type, component_name)                                      \
  typedef struct component_name component_type;
COMPONENTS_TABLE
#undef X

typedef struct entity {
#define X(component_type, component_name) component_type *component_name;
  COMPONENTS_TABLE
#undef X
} entity_t;
/// a world is a list of entity pointers
typedef entity_t **world_t;

/// instantiates a new entity
entity_t *new_entity(world_t *world, size_t *world_size);
/// deletes an entity
void delete_entity(entity_t *entity, world_t world, size_t world_size);

#define X(component_type, component_name)                                      \
  component_type *init_##component_name(entity_t *entity);
COMPONENTS_TABLE
#undef X

/// Iterates over every entity in the world
#define ITER_ENTITIES(world, world_size, entity, code)                         \
  for (size_t i = 0; i < (world_size); i++) {                                  \
    entity_t *entity = (world)[i];                                             \
    if (entity) {                                                              \
      code                                                                     \
    }                                                                          \
  }

#endif
