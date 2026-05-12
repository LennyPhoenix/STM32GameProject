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
      if (entity->gravity->on_ground) {
        entity->velocity->y = 0;
      }
      entity->velocity->y += entity->gravity->g;
      entity->gravity->on_ground = false;
    }
  });
}

void move_entities(world_t *world, size_t *world_size) {
  ITER_ENTITIES(*world, *world_size, entity, {
    if (entity->aabb && entity->velocity) {
      move_and_slide(world, world_size, entity);
    }
  });
}

aabb_t velocity_aabb_broad_phase(aabb_t aabb, velocity_t velocity) {
  aabb_t broad_phase = {0};

  if (velocity.x >= 0) {
    // velocity is > 0, extend width
    broad_phase.x = aabb.x;
    broad_phase.width = aabb.width + velocity.x;
  } else {
    // velocity is < 0, shift X back and extend width
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
  // distance between right of 1 and left of 2
  int32_t r1_to_l2 = p2 - (p1 + w1);
  // distance between left of 1 and right of 2
  int32_t l1_to_r2 = (p2 + w2) - p1;

  if (v1 > 0) {
    // if moving right, r1 will hit l2 in a collision
    *out_entry_distance = r1_to_l2;
    *out_exit_distance = l1_to_r2;
  } else {
    // if moving left, l1 will hit r2 in a collision
    *out_entry_distance = l1_to_r2;
    *out_exit_distance = r1_to_l2;
  }
}

void get_1d_collision_times(int32_t entry, int32_t exit, int32_t velocity,
                            double_t *out_entry_time, double_t *out_exit_time) {
  if (velocity == 0) {
    // avoid division by zero errors
    *out_entry_time = (double_t)INFINITY;
    *out_exit_time = -(double_t)INFINITY;
  } else {
    // t = d / v
    *out_entry_time = (double_t)entry / (double_t)velocity;
    *out_exit_time = (double_t)exit / (double_t)velocity;
  }
}

void move_and_slide(world_t *world, size_t *world_size, entity_t *entity) {
  if (entity->aabb && entity->velocity) {
    if (entity->velocity->skip_collisions) {
      // just move without checks
      entity->aabb->x += entity->velocity->x;
      entity->aabb->y += entity->velocity->y;
      return;
    }

    velocity_t velocity = *entity->velocity;
    aabb_t aabb = *entity->aabb;
    aabb_t broad_phase = velocity_aabb_broad_phase(aabb, velocity);

    // default collision: infinite time, distance and no entity reference.
    collision_t next_collision = {
        .entry_time = INFINITY,
        .entry_distance = INT32_MAX,
        .exit_time = INFINITY,
        .exit_distance = INT32_MAX,
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
    ITER_ENTITIES(*world, *world_size, other, {
      // do not collide with self
      if (other != entity && other->aabb) {
        aabb_t other_aabb = *other->aabb;
        // can only collide if layer and mask match, and if broad phase
        // intersects, otherwise skip this entity
        if (other_aabb.layer & velocity.mask &&
            intersects_aabb_aabb(other_aabb, broad_phase)) {

          // initialise new collision object
          collision_t collision;
          memset(&collision, 0, sizeof(collision_t));

          // store a reference to the colliding entity
          collision.entity = other;

          // get collision distances for both axes
          get_1d_collision_distances(
              aabb.x, other_aabb.x, aabb.width, other_aabb.width, velocity.x,
              &collision.entry_distance.x, &collision.exit_distance.x);
          get_1d_collision_distances(
              aabb.y, other_aabb.y, aabb.height, other_aabb.height, velocity.y,
              &collision.entry_distance.y, &collision.exit_distance.y);

          // get times for both axes
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

          // collision occurs for the axis which has the largest entry time
          if (x_entry_time > y_entry_time) {
            collision.entry_time = x_entry_time;

            // if entry distance is negative, then normal is positive and
            // vice-versa
            if (collision.entry_distance.x < 0) {
              collision.normal.x = 1;
            } else {
              collision.normal.x = -1;
            }
          } else {
            // same logic as above but for y axis
            collision.entry_time = y_entry_time;

            if (collision.entry_distance.y < 0) {
              collision.normal.y = 1;
            } else {
              collision.normal.y = -1;
            }
          }

          // use earliest exit time
          if (x_exit_time < y_exit_time) {
            collision.exit_time = x_exit_time;
          } else {
            collision.exit_time = y_exit_time;
          }

          if (collision.entry_time > collision.exit_time ||
              collision.exit_time <= 0.0 || collision.entry_time > 1) {
            // - skip if entry time is greater than exit time, this isn't a
            // valid collision
            // - if exit time is zero or less, the collision already occured
            // - if entry time is greater than 1, the collision will not occur
            // in this frame
            continue;
          } else if (collision.entry_time >= 0.0 &&
                     collision.entry_time < next_collision.entry_time) {
            // collision has not happened yet and happens before
            // `next_collision` occurs, replace `next_collision` in this case
            next_collision = collision;
          }
        }
      }
    });

    if (next_collision.entry_time < 1.0) {
      // if next collision occurs this frame, move entity to point of collision
      entity->aabb->x += velocity.x * next_collision.entry_time;
      entity->aabb->y += velocity.y * next_collision.entry_time;

      if (entity->gravity) {
        if (next_collision.normal.y < 0) {
          entity->gravity->on_ground = true;
        }
      }

      // run collision callback if applicable
      if (velocity.callback) {
        velocity.callback(world, world_size, entity, next_collision);
      }

      // calculate the dot product of the transposed collision normal and
      // velocity
      double_t dot_product = (velocity.x * next_collision.normal.y +
                              velocity.y * next_collision.normal.x);

      // slide along the edge of the collider for the rest of the frame time
      // NOTE: this could move the object inside another collider, we should
      // really be running multiple iterations of `move_and_slide` here
      entity->aabb->x += next_collision.normal.y * dot_product *
                         (1.0 - next_collision.entry_time);
      entity->aabb->y += next_collision.normal.x * dot_product *
                         (1.0 - next_collision.entry_time);
    } else {
      // no collision this frame, move the full distance
      entity->aabb->x += velocity.x;
      entity->aabb->y += velocity.y;
    }
  }
}
