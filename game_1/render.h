#ifndef RENDER_H_
#define RENDER_H_

#include "entity.h"
#include <stddef.h>

void realign_aabbs_to_player(world_t world, size_t world_size);
void render_aabbs(world_t world, size_t world_size);

#endif
