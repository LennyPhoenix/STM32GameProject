#include <stdlib.h>
#include <string.h>

#include "aabb.h"
#include "entity.h"
#include "gravity.h"
#include "platform.h"
#include "player.h"
#include "velocity.h"

entity_t *new_entity(world_t *world, size_t *world_size) {
  entity_t *entity = calloc(sizeof(entity_t), 1);

  // search for empty slot in world
  entity_t **slot = NULL;
  for (size_t i = 0; i < *world_size; i++) {
    entity_t **entity = (*world) + i;
    if (!(*entity)) {
      slot = entity;
    }
  }

  if (slot) {
    *slot = entity;
  } else {
    (*world_size)++;
    *world = realloc(*world, sizeof(entity_t *) * *world_size);
    (*world)[*world_size - 1] = entity;
  }

  return entity;
}

void delete_entity(entity_t *entity, world_t world, size_t world_size) {
  for (size_t i = 0; i < world_size; i++) {
    if (world[i] == entity) {
      world[i] = NULL;
    }
  }

#define X(component_type, component_name)                                      \
  if (entity->component_name) {                                                \
    free(entity->component_name);                                              \
  }
  COMPONENTS_TABLE
#undef X

  free(entity);
}

#define X(component_type, component_name)                                      \
  component_type *init_##component_name(entity_t *entity) {                    \
    if (entity->component_name) {                                              \
      memset(entity->component_name, 0, sizeof(component_type));               \
    } else {                                                                   \
      entity->component_name = calloc(sizeof(component_type), 1);              \
    }                                                                          \
    return entity->component_name;                                             \
  }
COMPONENTS_TABLE
#undef X
