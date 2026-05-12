#include "player.h"
#include "InputHandler.h"
#include "Joystick.h"
#include "aabb.h"
#include "entity.h"
#include "game_entity.h"
#include "gravity.h"
#include "jump.h"
#include "physics.h"
#include "platform.h"
#include "render.h"
#include "sensor.h"
#include "stdbool.h"
#include "timer.h"
#include "velocity.h"
#include "weapon.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>

extern Joystick_t joystick_data; // Current joystick readings

void update_player_inputs(world_t world, size_t world_size) {
  if ((*world)->game_component && (*world)->game_component->game_over) {
    // do not update inputs if the game is over
    return;
  }

  ITER_ENTITIES(world, world_size, player, {
    if (player->player && player->jump && player->weapon) {
      player->jump->jumping =
          current_input.btn2_pressed && player->gravity->on_ground;

      player->weapon->firing = current_input.btn4_pressed;
      player->weapon->aim_right = joystick_data.coord_mapped.x;
      player->weapon->aim_down = joystick_data.coord_mapped.y;
    }
  });
}

void update_player_velocity(world_t world, size_t world_size) {
  ITER_ENTITIES(world, world_size, entity, {
    if (entity->player && entity->velocity) {
      if (entity->player->dead) {
        entity->velocity->x = 1;
      } else {
        entity->velocity->x = 10;
      }
    }
  });
}

void check_player_death(world_t world, size_t world_size) {
  entity_t *game = *world;
  if (game->game_component) {
    bool game_over = true;
    // if no player is found, the game is considered over
    ITER_ENTITIES(world, world_size, entity, {
      if (entity->player) {
        game_over = entity->player->dead;
        break;
      }
    });

    // game stays over forever. the logic here should make that true anyway but
    // theres no harm in being safe in each system. maybe the player becomes
    // alive again? the game shouldn't magically continue like nothing happened
    if (game_over) {
      game->game_component->game_over = true;
    }
  }
}

void player_collision_callback(world_t *world, size_t *world_size,
                               entity_t *player, collision_t collision) {
  entity_t *collider = collision.entity;
  entity_t *game = **world;
  if (collider && collider->platform && !collider->platform->reached && game &&
      game->game_component) {
    collider->platform->reached = true;
    game->game_component->score += 100;
  }
}

void player_projectile_collision_callback(world_t *world, size_t *world_size,
                                          entity_t *projectile,
                                          collision_t collision) {

  entity_t *detected = collision.entity;

  entity_t *game = **world;
  if (game->game_component && detected) {
    uint32_t score = 999;

    if (detected->zombie) {
      score = 100;
    } else if (detected->bat) {
      score = 200;
    }

    game->game_component->score += score;

    // TODO play a noise

    delete_entity(detected, *world, *world_size);
    delete_entity(projectile, *world, *world_size);
  }
}

void player_projectile_sensor_callback(world_t *world, size_t *world_size,
                                       uint32_t frame, entity_t *projectile,
                                       entity_t *detected) {
  entity_t *game = **world;
  if (game->game_component) {
    uint32_t score = 999;

    if (detected->zombie) {
      score = 100;
    } else if (detected->bat) {
      score = 200;
    }

    game->game_component->score += score;

    // TODO play a noise

    delete_entity(detected, *world, *world_size);
    delete_entity(projectile, *world, *world_size);
  }
}

entity_t *new_player_projectile(world_t *world, size_t *world_size,
                                uint32_t frame) {
  entity_t *projectile = new_entity(world, world_size);

  aabb_t *aabb = init_aabb(projectile);
  aabb->width = 16;
  aabb->height = 3;
  aabb->layer = PLAYER_PROJECTILE_LAYER;

  draw_rect_t *draw_rect = init_draw_rect(projectile);
  draw_rect->colour = 4;
  draw_rect->fill = 1;

  velocity_t *velocity = init_velocity(projectile);
  velocity->callback = player_projectile_collision_callback;
  velocity->mask = ENEMY_LAYER;

  sensor_t *sensor = init_sensor(projectile);
  sensor->callback = player_projectile_sensor_callback;
  sensor->mask = ENEMY_LAYER;

  timer_component_t *timer = init_timer(projectile);
  timer->timeout = 60;

  return projectile;
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
  velocity->callback = player_collision_callback;

  gravity_t *gravity = init_gravity(player);
  gravity->g = 10;

  draw_rect_t *draw_rect = init_draw_rect(player);
  draw_rect->colour = 2;
  draw_rect->fill = 1;

  jump_t *jump = init_jump(player);
  jump->power = 40;

  weapon_t *weapon = init_weapon(player);
  weapon->projectile_speed = 30;
  weapon->cooldown = 15;
  weapon->new_projectile = new_player_projectile;

  init_player(player);

  return player;
}
