#include "player.h"
#include "aabb.h"
#include "entity.h"
#include "gravity.h"
#include "physics.h"
#include "render.h"
#include "stdbool.h"
#include "velocity.h"
#include <math.h>
#include <stdlib.h>

/// callback function for when a player collides with something
void handle_player_collision(entity_t *player, collision_t collision) {
  // if normal is < 0, the player is on top of something. (down is positive!)
  if (collision.normal.y < 0.0) {
    player->player->on_ground = true;
  }
}

void update_player_velocity(world_t world, size_t world_size) {
  ITER_ENTITIES(world, world_size, entity, {
    if (entity->player) {
      if (entity->velocity) {
        if (entity->player->jumping) {
          // initial jump velocity = -50
          entity->velocity->y = -50;
        } else if (entity->player->on_ground) {
          // reset Y velocity if on ground
          entity->velocity->y = 0;
        }
        entity->velocity->x = 10;
      }
      // reset on_ground flag for next frame
      entity->player->on_ground = false;
    }
  });
}

entity_t *new_player(world_t *world, size_t *world_size) {
  entity_t *player = new_entity(world, world_size);

  aabb_t *aabb = init_aabb(player);
  aabb->x = 0;
  aabb->y = 0;
  aabb->width = 16;
  aabb->height = 32;
  aabb->layer = PLAYER_LAYER;

  velocity_t *velocity = init_velocity(player);
  velocity->x = 10;
  velocity->mask = ENVIRONMENT_LAYER;
  velocity->callback = handle_player_collision;

  gravity_t *gravity = init_gravity(player);
  gravity->g = 10;

  draw_rect_t *draw_rect = init_draw_rect(player);
  draw_rect->colour = 2;
  draw_rect->fill = 1;

  init_player(player);

  return player;
}
