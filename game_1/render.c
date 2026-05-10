#include "render.h"
#include "LCD.h"
#include "aabb.h"
#include "entity.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

void realign_aabbs_to_player(world_t world, size_t world_size) {
  aabb_t *player_aabb = NULL;

  ITER_ENTITIES(world, world_size, entity, {
    if (entity->player && entity->aabb) {
      player_aabb = entity->aabb;
      break;
    }
  });

  if (player_aabb) {
    int32_t player_x = player_aabb->x;
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
      aabb_t aabb = *entity->aabb;

      if (aabb.x + aabb.width <= 0 || aabb.y + aabb.height <= 0 ||
          aabb.x >= 240 || aabb.y >= 240) {
        continue;
      }

      uint32_t x;
      uint32_t w;
      uint32_t y;
      uint32_t h;
      if (aabb.x < 0) {
        x = 0;
        w = aabb.width + aabb.x;
      } else {
        x = aabb.x;
        w = aabb.width;
      }
      if (aabb.y < 0) {
        y = 0;
        h = aabb.height + aabb.y;
      } else {
        y = aabb.y;
        h = aabb.height;
      }

      int32_t right = x + w;
      if (right > 240) {
        w -= right - 240;
      }
      int32_t bottom = y + h;
      if (bottom > 240) {
        h -= bottom - 240;
      }

      // printf("rendering rect (%d,%d %lux%lu) from aabb (%d,%d %lux%lu)\n", x,
      //        y, w, h, aabb.x, aabb.y, aabb.width, aabb.height);

      // LCD is 240x240
      LCD_Draw_Rect(x, y, w, h, 1, 1);
    }
  });
}
