#include "physics.h"

#include "aabb.h"
#include "entity.h"
#include "gravity.h"
#include "velocity.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void apply_gravity(world_t world, size_t world_size) {
  ITER_ENTITIES(world, world_size, entity, {
    if (entity->gravity && entity->velocity) {
      entity->velocity->y += entity->gravity->g;
    }
  });
}

void move_entities(world_t world, size_t world_size) {
  ITER_ENTITIES(world, world_size, entity, {
    if (entity->aabb && entity->velocity) {
      move_and_slide(world, world_size, entity);
    }
  });
}

aabb_t velocity_aabb_broad_phase(aabb_t aabb, velocity_t velocity) {
  aabb_t broad_phase = {0};

  if (velocity.x >= 0) {
    broad_phase.x = aabb.x;
    broad_phase.width = aabb.width + velocity.x;
  } else {
    broad_phase.x = aabb.x + velocity.x;
    broad_phase.width = aabb.width - velocity.x;
  }

  if (velocity.y >= 0) {
    broad_phase.y = aabb.y;
    broad_phase.height = aabb.height + velocity.y;
  } else {
    broad_phase.y = aabb.y + velocity.y;
    broad_phase.height = aabb.height - velocity.y;
  }

  return broad_phase;
}

/// Gets the entry and exit distances for a 1-dimensional collision.
void get_1d_collision_distances(int32_t p1, int32_t p2, int32_t w1, int32_t w2,
                                int32_t v1, int32_t *out_entry_distance,
                                int32_t *out_exit_distance) {
  int32_t r1_to_l2 = p2 - (p1 + w1);
  int32_t l1_to_r2 = (p2 + w2) - p1;

  // if v1 is positive, and a collision is inevitable the right side of 1 is
  // going to be moving towards the left of 2
  if (v1 > 0) {
    *out_entry_distance = r1_to_l2;
    *out_exit_distance = l1_to_r2;
  } else {
    *out_entry_distance = l1_to_r2;
    *out_exit_distance = r1_to_l2;
  }
}

void get_1d_collision_times(int32_t entry, int32_t exit, int32_t velocity,
                            double_t *out_entry_time, double_t *out_exit_time) {
  if (velocity == 0) {
    *out_entry_time = (double_t)INFINITY;
    *out_exit_time = -(double_t)INFINITY;
  } else {
    *out_entry_time = (double_t)entry / (double_t)velocity;
    *out_exit_time = (double_t)exit / (double_t)velocity;
  }
}

void move_and_slide(world_t world, size_t world_size, entity_t *entity) {
  if (entity->aabb && entity->velocity) {
    if (entity->velocity->skip_collisions) {
      entity->aabb->x += entity->velocity->x;
      entity->aabb->y += entity->velocity->y;
      return;
    }

    velocity_t velocity = *entity->velocity;
    aabb_t aabb = *entity->aabb;
    aabb_t broad_phase = velocity_aabb_broad_phase(aabb, velocity);

    collision_t next_collision = {
        .entry_time = INFINITY,
        .entry_distance = INFINITY,
        .exit_time = INFINITY,
        .exit_distance = INFINITY,
        .normal = {0},
        .entity = NULL,
    };

    // find next collision
    //
    // this is very inoptimal, we are doing absolutely zero caching or spatial
    // hashing here.
    //
    // i believe there are simple ways to do a spatial hash, i'm not sure how
    // effective they will be on the stm board though.
    ITER_ENTITIES(world, world_size, other, {
      if (other != entity && other->aabb) {
        aabb_t other_aabb = *other->aabb;
        if (other_aabb.layer & velocity.mask &&
            intersects_aabb_aabb(other_aabb, broad_phase)) {

          collision_t collision;
          collision.entity = other;
          memset(&collision, 0, sizeof(collision_t));
          get_1d_collision_distances(
              aabb.x, other_aabb.x, aabb.width, other_aabb.width, velocity.x,
              &collision.entry_distance.x, &collision.exit_distance.x);
          get_1d_collision_distances(
              aabb.y, other_aabb.y, aabb.height, other_aabb.height, velocity.y,
              &collision.entry_distance.y, &collision.exit_distance.y);

          double_t x_entry_time;
          double_t x_exit_time;
          double_t y_entry_time;
          double_t y_exit_time;
          get_1d_collision_times(collision.entry_distance.x,
                                 collision.exit_distance.x, velocity.x,
                                 &x_entry_time, &x_exit_time);
          get_1d_collision_times(collision.entry_distance.y,
                                 collision.exit_distance.y, velocity.y,
                                 &y_entry_time, &y_exit_time);

          if (x_entry_time > y_entry_time) {
            collision.entry_time = x_entry_time;

            if (collision.entry_distance.x < 0) {
              collision.normal.x = 1;
            } else {
              collision.normal.x = -1;
            }
          } else {
            collision.entry_time = y_entry_time;

            if (collision.entry_distance.y < 0) {
              collision.normal.y = 1;
            } else {
              collision.normal.y = -1;
            }
          }

          if (x_exit_time < y_exit_time) {
            collision.exit_time = x_exit_time;
          } else {
            collision.exit_time = y_exit_time;
          }

          if (collision.entry_time > collision.exit_time ||
              collision.exit_time <= 0.0 || collision.entry_time > 1) {
            continue;
          } else if (collision.entry_time >= 0.0 &&
                     collision.entry_time < next_collision.entry_time) {
            next_collision = collision;
          }
        }
      }
    });

    if (next_collision.entry_time < 1.0) {
      double_t dot_product = (velocity.x * next_collision.normal.y +
                              velocity.y * next_collision.normal.x);

      entity->aabb->x += velocity.x * next_collision.entry_time;
      entity->aabb->y += velocity.y * next_collision.entry_time;

      if (velocity.callback) {
        velocity.callback(entity, next_collision);
      }

      // slide
      entity->aabb->x += next_collision.normal.y * dot_product *
                         (1.0 - next_collision.entry_time);
      entity->aabb->y += next_collision.normal.x * dot_product *
                         (1.0 - next_collision.entry_time);

    } else {
      entity->aabb->x += velocity.x;
      entity->aabb->y += velocity.y;
    }
  }
}
