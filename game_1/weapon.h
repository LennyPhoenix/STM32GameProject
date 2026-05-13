#ifndef WEAPON_H_
#define WEAPON_H_

#include "entity.h"

#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct weapon {
  // inputs
  double_t aim_right, aim_down;
  bool firing;

  // config
  uint32_t cooldown;
  uint32_t projectile_speed;
  entity_t *(*new_projectile)(world_t *world, size_t *world_size,
                              uint32_t frame);

  // state
  uint32_t last_frame_fired;
} weapon_t;

void fire_weapons(world_t *world, size_t *world_size, uint32_t frame);

#endif
