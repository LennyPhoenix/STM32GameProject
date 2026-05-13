#ifndef ENEMY_H_
#define ENEMY_H_

#include "entity.h"
#include <stddef.h>
#include <stdint.h>

typedef struct zombie {
  uint32_t next_jump;
} zombie_t;

typedef struct bat {
} bat_t;

void spawn_enemies(world_t *world, size_t *world_size, uint32_t frame);
void check_zombie_jumps(world_t world, size_t world_size, uint32_t frame);

entity_t *new_zombie_jumper(world_t *world, size_t *world_size, uint32_t frame);
entity_t *new_zombie_runner(world_t *world, size_t *world_size, uint32_t frame);
entity_t *new_bat(world_t *world, size_t *world_size, uint32_t frame);

#endif
