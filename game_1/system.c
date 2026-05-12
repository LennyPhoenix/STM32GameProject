#include "system.h"
#include "aabb.h"
#include "enemy.h"
#include "entity.h"
#include "physics.h"
#include "platform.h"
#include "player.h"
#include "render.h"
#include "sensor.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/// deletes entities that have passed the top, bottom, or left sides of the
/// screen
void delete_entities(world_t *world, size_t *world_size) {
  for (size_t i = 0; i < (*world_size); i++) {
    entity_t *entity = (*world)[i];
    if (entity) {
      {
        if (entity->aabb && (entity->aabb->x + entity->aabb->width <= 0 ||
                             entity->aabb->y >= 240 ||
                             entity->aabb->y + entity->aabb->height < 0)) {
          if (entity->platform) {
            double_t weight = (double_t)rand() / (double_t)RAND_MAX;
            new_platform(world, world_size, 240, 80 * weight);
          }
          delete_entity(entity, *world, *world_size);
        }
      }
    }
  };
}

void run_systems(world_t *world, size_t *world_size, uint32_t frame) {
  update_player_inputs(*world, *world_size);
  spawn_enemies(world, world_size, frame);
  apply_gravity(*world, *world_size);
  update_player_velocity(*world, *world_size);
  move_entities(world, world_size);
  check_sensors(world, world_size, frame);
  realign_aabbs_to_player(*world, *world_size);
  delete_entities(world, world_size);
  render_rects(*world, *world_size);
  check_player_death(*world, *world_size);
  render_gameover(*world, *world_size);
  render_score(*world, *world_size);
}
