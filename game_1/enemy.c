#include "enemy.h"
#include "aabb.h"
#include "entity.h"
#include "gravity.h"
#include "physics.h"
#include "player.h"
#include "render.h"
#include "sensor.h"
#include "velocity.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

void spawn_enemies(world_t *world, size_t *world_size, uint32_t frame) {
  if (frame % 20 == 0) {
    new_zombie(world, world_size);
  }
}

void enemy_sensor_callback(world_t *_world, size_t *_world_size,
                           uint32_t _frame, entity_t *enemy, entity_t *other) {
  if (other->player) {
    // kill the player
    printf("player killed!");
    other->player->dead = true;
  }
}

entity_t *new_zombie(world_t *world, size_t *world_size) {
  entity_t *zombie = new_entity(world, world_size);

  aabb_t *aabb = init_aabb(zombie);
  aabb->x = 260;
  aabb->y = 40;
  aabb->width = 16;
  aabb->height = 32;
  aabb->layer = ENEMY_LAYER;

  velocity_t *velocity = init_velocity(zombie);
  velocity->x = -4;
  velocity->mask = ENVIRONMENT_LAYER;

  draw_rect_t *draw_rect = init_draw_rect(zombie);
  draw_rect->colour = 3;
  draw_rect->fill = 1;

  gravity_t *gravity = init_gravity(zombie);
  gravity->g = 10;

  sensor_t *sensor = init_sensor(zombie);
  sensor->callback = enemy_sensor_callback;
  sensor->mask = PLAYER_LAYER;

  init_zombie(zombie);

  return zombie;
}

entity_t *new_bat(world_t *world, size_t *world_size) {
  entity_t *bat = new_entity(world, world_size);

  init_bat(bat);

  return bat;
}
