#ifndef ENTITY_H_
#define ENTITY_H_

#include <stddef.h>

// X-macro table for entity components
// define X(component_type, component_name) to use
#define COMPONENTS_TABLE                                                       \
  X(aabb_t, aabb)                                                              \
  X(velocity_t, velocity)                                                      \
  X(gravity_t, gravity)                                                        \
  X(player_t, player)                                                          \
  X(platform_t, platform)                                                      \
  X(draw_rect_t, draw_rect)                                                    \
  X(zombie_t, zombie)                                                          \
  X(bat_t, bat)                                                                \
  X(sensor_t, sensor)                                                          \
  X(game_component_t, game_component)                                          \
  X(jump_t, jump)                                                              \
  X(weapon_t, weapon)                                                          \
  X(timer_component_t, timer)

// incomplete typedefs to prevent cyclic includes
#define X(component_type, component_name)                                      \
  typedef struct component_name component_type;
COMPONENTS_TABLE
#undef X

/// entity struct, may contain any component
typedef struct entity {
  // procedurally generated struct members
  // e.g.
  // aabb_t *aabb;
  // velocity_t *velocity;
  // gravity_t *gravity;
  // ...
#define X(component_type, component_name) component_type *component_name;
  COMPONENTS_TABLE
#undef X
} entity_t;
/// a world is a list of entity pointers.
typedef entity_t **world_t;

/// instantiates a new entity into the world, allocating more space to the world
/// if necessary.
entity_t *new_entity(world_t *world, size_t *world_size);
/// deletes an entity from the world.
/// all existing `entity_t *`s for this entity will become dangling pointers!
void delete_entity(entity_t *entity, world_t world, size_t world_size);

// procedural `init_component` methods.
#define X(component_type, component_name)                                      \
  component_type *init_##component_name(entity_t *entity);
COMPONENTS_TABLE
#undef X

// procedural `ensure_component` methods.
#define X(component_type, component_name)                                      \
  component_type *ensure_##component_name(entity_t *entity);
COMPONENTS_TABLE
#undef X

/// iterates over every entity in the world.
#define ITER_ENTITIES(world, world_size, entity, code)                         \
  for (size_t i = 0; i < (world_size); i++) {                                  \
    entity_t *entity = (world)[i];                                             \
    if (entity) {                                                              \
      code                                                                     \
    }                                                                          \
  }

#endif
