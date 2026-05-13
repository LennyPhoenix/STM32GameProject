#include "enemy.h"
#include "Buzzer.h"
#include "aabb.h"
#include "entity.h"
#include "gravity.h"
#include "jump.h"
#include "physics.h"
#include "player.h"
#include "render.h"
#include "sensor.h"
#include "velocity.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

extern Buzzer_cfg_t buzzer_cfg; // Buzzer control

void spawn_enemies(world_t *world, size_t *world_size, uint32_t frame) {
  if (frame % 40 == 30) {
    new_zombie_jumper(world, world_size, frame);
  }
  if (frame % 55 == 25) {
    new_zombie_runner(world, world_size, frame);
  }
  if (frame % 75 == 50) {
    new_bat(world, world_size, frame);
  }
}

void enemy_sensor_callback(world_t *_world, size_t *_world_size,
                           uint32_t _frame, entity_t *enemy, entity_t *other) {
  if (other->player) {
    buzzer_tone(&buzzer_cfg, 300, 100);
    other->player->dead = true;
  }
}

void check_zombie_jumps(world_t world, size_t world_size, uint32_t frame) {
  for (size_t i = 0; i < (world_size); i++) {
    entity_t *entity = (world)[i];
    if (entity) {
      {
        if (entity->zombie && entity->zombie->next_jump <= frame &&
            entity->jump && entity->gravity && entity->gravity->on_ground) {
          entity->zombie->next_jump = frame + (uint32_t)(rand() % 60);
          entity->jump->jumping = true;
        }
      }
    }
  };
}

entity_t *new_zombie_runner(world_t *world, size_t *world_size,
                            uint32_t frame) {
  entity_t *zombie = new_entity(world, world_size);

  aabb_t *aabb = init_aabb(zombie);
  aabb->x = 260;
  aabb->y = 40;
  aabb->width = 16;
  aabb->height = 16;
  aabb->layer = ENEMY_LAYER;

  velocity_t *velocity = init_velocity(zombie);
  velocity->x = -6;
  velocity->mask = ENVIRONMENT_LAYER;

  draw_rect_t *draw_rect = init_draw_rect(zombie);
  draw_rect->colour = 5;
  draw_rect->fill = 1;

  gravity_t *gravity = init_gravity(zombie);
  gravity->g = 10;

  sensor_t *sensor = init_sensor(zombie);
  sensor->callback = enemy_sensor_callback;
  sensor->mask = PLAYER_LAYER;

  init_zombie(zombie);

  return zombie;
}

entity_t *new_zombie_jumper(world_t *world, size_t *world_size,
                            uint32_t frame) {
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

  zombie_t *zombie_c = init_zombie(zombie);
  zombie_c->next_jump = frame + (uint32_t)(rand() % 80);

  jump_t *jump = init_jump(zombie);
  jump->power = 35;

  return zombie;
}

entity_t *new_bat(world_t *world, size_t *world_size, uint32_t frame) {
  entity_t *bat = new_entity(world, world_size);

  aabb_t *aabb = init_aabb(bat);
  aabb->x = 260;
  aabb->y = 40;
  aabb->width = 16;
  aabb->height = 16;
  aabb->layer = ENEMY_LAYER;

  velocity_t *velocity = init_velocity(bat);
  velocity->x = 1;
  velocity->mask = ENVIRONMENT_LAYER;

  draw_rect_t *draw_rect = init_draw_rect(bat);
  draw_rect->colour = 6;
  draw_rect->fill = 1;

  sensor_t *sensor = init_sensor(bat);
  sensor->callback = enemy_sensor_callback;
  sensor->mask = PLAYER_LAYER;

  init_bat(bat);

  return bat;
}
