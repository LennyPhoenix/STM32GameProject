#ifndef VELOCITY_H_
#define VELOCITY_H_

#include "aabb.h"
#include "entity.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>

/// stores information about a collision.
typedef struct collision {
  point_t entry_distance;
  point_t exit_distance;
  double_t entry_time;
  double_t exit_time;
  point_t normal;
  entity_t *entity;
} collision_t;

/// attach to an entity to move it each frame.
typedef struct velocity {
  int32_t x;
  int32_t y;

  /// Set to `true` to skip all collision checks
  bool skip_collisions;
  /// Bitmask for collision layers
  uint32_t mask;

  /// called when the entity collides with another
  void (*callback)(entity_t *entity, collision_t collision);
} velocity_t;

#endif
