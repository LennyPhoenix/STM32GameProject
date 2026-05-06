#include "render.h"
#include "LCD.h"
#include "entity.h"
#include <stddef.h>

void render_aabbs(world_t world, size_t world_size) {
  for (size_t i = 0; i < world_size; i++) {
    entity_t *entity = world[i];

    if (entity->aabb) {
      LCD_Draw_Rect(entity->aabb->x, entity->aabb->y, entity->aabb->width,
                    entity->aabb->height, 1, 1);
    }
  }
}
