#include "render.h"
#include "LCD.h"
#include "entity.h"
#include <stddef.h>
#include <stdint.h>

void realign_aabbs_to_player(world_t world, size_t world_size) {
  entity_t *player = NULL;

  ITER_ENTITIES(world, world_size, entity, {
    if (entity->player && entity->aabb) {
      player = entity;
      break;
    }
  });

  if (player) {
    int32_t player_x = player->aabb->x;
    ITER_ENTITIES(world, world_size, entity, {
      if (entity->aabb) {
        entity->aabb->x -= player_x;
      }
    });
  }
}

void render_aabbs(world_t world, size_t world_size) {
  ITER_ENTITIES(world, world_size, entity, {
    if (entity->aabb) {
      uint32_t x;
      uint32_t y;
      uint32_t w;
      uint32_t h;

      if (entity->aabb->x >= 0) {
        x = entity->aabb->x;
        w = entity->aabb->width;
      } else if (entity->aabb->x + entity->aabb->width > 0) {
        x = 0;
        w = entity->aabb->width + entity->aabb->x;
      } else {
        continue;
      };

      if (entity->aabb->y >= 0) {
        y = entity->aabb->y;
        h = entity->aabb->height;
      } else if (entity->aabb->y + entity->aabb->height > 0) {
        y = 0;
        h = entity->aabb->height + entity->aabb->y;
      } else {
        continue;
      };
      
      // LCD is 240x240
      LCD_Draw_Rect(x, y, w, h, 1, 1);
    }
  });
}
