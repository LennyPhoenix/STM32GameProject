#include <stdlib.h>

#include "entity.h"

entity_t *new_entity(world_t *world, size_t *world_size) {
  entity_t *entity = calloc(sizeof(entity_t), 1);

  (*world_size)++;
  *world = realloc(*world, sizeof(entity_t *) * *world_size);
  (*world)[*world_size - 1] = entity;

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
