#ifndef RENDER_H_
#define RENDER_H_

#include "entity.h"
#include <stddef.h>
#include <stdint.h>

/// attach to an entity to draw the entity's aabb as a rect
typedef struct draw_rect {
  uint8_t colour;
  uint8_t fill;
} draw_rect_t;

/// moves all entities to be positioned relative to the player
void realign_aabbs_to_player(world_t world, size_t world_size);
/// renders all rect entities
void render_rects(world_t world, size_t world_size);
/// draws the gameover screen if applicable
void render_gameover(world_t world, size_t world_size);
/// renders the current score to the top left of the screen
void render_score(world_t world, size_t world_size);

#endif
