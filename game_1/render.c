#include "render.h"
#include "LCD.h"
#include "aabb.h"
#include "entity.h"
#include "game_entity.h"
#include "player.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

void realign_aabbs_to_player(world_t world, size_t world_size) {
  aabb_t *player_aabb = NULL;

  // find the first player in the world
  ITER_ENTITIES(world, world_size, entity, {
    if (entity->player && entity->aabb) {
      player_aabb = entity->aabb;
      break;
    }
  });

  if (player_aabb) {
    // if there is a player, move every entity to be relative to the player at
    // the origin.
    //
    // magic 40 here is so that the player has some space from the left side of
    // the screen.
    int32_t player_x = player_aabb->x - 40;
    ITER_ENTITIES(world, world_size, entity, {
      if (entity->aabb) {
        entity->aabb->x -= player_x;
      }
    });
  }
}

void render_rects(world_t world, size_t world_size) {
  ITER_ENTITIES(world, world_size, entity, {
    if (entity->aabb && entity->draw_rect) {
      aabb_t aabb = *entity->aabb;

      if (aabb.x + aabb.width <= 0 || aabb.y + aabb.height <= 0 ||
          aabb.x >= 240 || aabb.y >= 240) {
        // skip this entity if it is fully off-screen
        continue;
      }

      uint32_t x;
      uint32_t w;
      uint32_t y;
      uint32_t h;
      if (aabb.x < 0) {
        // clamp to left side of the screen
        x = 0;
        w = aabb.width + aabb.x;
      } else {
        // full width
        x = aabb.x;
        w = aabb.width;
      }
      if (aabb.y < 0) {
        // clamp to top of screen
        y = 0;
        h = aabb.height + aabb.y;
      } else {
        // full height
        y = aabb.y;
        h = aabb.height;
      }

      int32_t right = x + w;
      if (right > 240) {
        // clamp to right of screen
        w -= right - 240;
      }
      int32_t bottom = y + h;
      if (bottom > 240) {
        // clamp to bottom of screen
        h -= bottom - 240;
      }

      // printf("rendering rect (%d,%d %lux%lu) from aabb (%d,%d %lux%lu)\n", x,
      //        y, w, h, aabb.x, aabb.y, aabb.width, aabb.height);

      // LCD is 240x240
      LCD_Draw_Rect(x, y, w, h, entity->draw_rect->colour,
                    entity->draw_rect->fill);
    }
  });
}

void render_gameover(world_t world, size_t world_size) {
  entity_t *game = *world;
  if (game->game_component && game->game_component->game_over) {
    // todo: render the score
    LCD_printString("Game", 80, 40, 1, 3);
    LCD_printString("Over!", 70, 70, 1, 3);

    LCD_printString("Your Score:", 20, 110, 1, 3);
    char score[11];
    sprintf(score, "%.10lu", game->game_component->score);
    LCD_printString(score, 55, 140, 1, 2);
    LCD_printString("BTN3 to Restart", 60, 163, 1, 1);
  }
}

void render_score(world_t world, size_t world_size) {
  entity_t *game = *world;
  if (game && game->game_component && !game->game_component->game_over) {
    char score[10];
    sprintf(score, "Score: %lu", game->game_component->score);
    LCD_printString(score, 2, 2, 1, 2);
  }
}
