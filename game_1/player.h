#ifndef PLAYER_H_
#define PLAYER_H_

#include "entity.h"
#include <stddef.h>

entity_t *new_player(world_t *world, size_t *world_size);

#endif
