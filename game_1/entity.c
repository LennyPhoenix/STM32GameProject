#include <stdlib.h>
#include <string.h>

#include "aabb.h"
#include "enemy.h"
#include "entity.h"
#include "game_entity.h"
#include "gravity.h"
#include "jump.h"
#include "platform.h"
#include "player.h"
#include "render.h"
#include "sensor.h"
#include "timer.h"
#include "velocity.h"
#include "weapon.h"

entity_t *new_entity(world_t *world, size_t *world_size) {
  // use calloc as it initialises to zero
  entity_t *entity = calloc(sizeof(entity_t), 1);
  memset(entity, 0, sizeof(entity_t));

  // search for empty slot in world
  //
  // world slot is `entity_t *`, so we want an `entity_t **` to write to it
  entity_t **slot = NULL;
  for (size_t i = 0; i < *world_size; i++) {
    // world_t is an entity pointer, we deref the `world_t *` then add `i` to
    // index it
    entity_t **entity = *world + i;
    // if the entity slot is empty, then store a reference to the slot
    if (!(*entity)) {
      slot = entity;
      break;
    }
  }

  if (slot) {
    // if an empty slot was found, store the new entity in it
    *slot = entity;
  } else {
    // no empty slots, expand the world buffer!
    (*world_size)++;
    *world = realloc(*world, sizeof(entity_t *) * *world_size);
    (*world)[*world_size - 1] = entity;
  }

  // return a reference to this new entity
  return entity;
}

void delete_entity(entity_t *entity, world_t world, size_t world_size) {
  // clear the entity slot in the world buffer
  for (size_t i = 0; i < world_size; i++) {
    if (world[i] == entity) {
      world[i] = NULL;
      // there should only be one slot in the world buffer per entity
      break;
    }
  }

  // procedurally generated logic to free each component on the entity if it is
  // allocated, e.g.
  //
  // if (entity->aabb) {
  //   free(entity->aabb);
  // }
#define X(component_type, component_name)                                      \
  if (entity->component_name) {                                                \
    free(entity->component_name);                                              \
  }
  COMPONENTS_TABLE
#undef X

  // deallocate the entity itself
  free(entity);
}

// procedurally generated `init_component` methods.
// e.g.
// aabb_t *init_aabb(entity_t *entity) {
//   if (entity->aabb) {
//     // zero out the component if already allocated
//     memset(entity->aabb, 0, sizeof(aabb_t));
//   } else {
//     // allocate the component with calloc if needed
//     entity->aabb = calloc(sizeof(aabb_t), 1);
//   }
//   return entity->aabb;
// }
#define X(component_type, component_name)                                      \
  component_type *init_##component_name(entity_t *entity) {                    \
    if (!entity->component_name) {                                             \
      entity->component_name = calloc(sizeof(component_type), 1);              \
    }                                                                          \
    memset(entity->component_name, 0, sizeof(component_type));                 \
    return entity->component_name;                                             \
  }
COMPONENTS_TABLE
#undef X

// same as above, but do not wipe if already found
#define X(component_type, component_name)                                      \
  component_type *ensure_##component_name(entity_t *entity) {                  \
    if (!entity->component_name) {                                             \
      entity->component_name = calloc(sizeof(component_type), 1);              \
      memset(entity->component_name, 0, sizeof(component_type));               \
    }                                                                          \
    return entity->component_name;                                             \
  }
COMPONENTS_TABLE
#undef X
