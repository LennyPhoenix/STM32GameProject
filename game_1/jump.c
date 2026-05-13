#include "jump.h"
#include "Buzzer.h"
#include "entity.h"
#include "velocity.h"

#include <stdint.h>

extern Buzzer_cfg_t buzzer_cfg; // Buzzer control

void apply_jumps(world_t world, size_t world_size) {
  ITER_ENTITIES(world, world_size, entity, {
    if (entity->jump && entity->velocity && entity->jump->jumping) {
      entity->jump->jumping = false;
      entity->velocity->y = -(int32_t)entity->jump->power;
      buzzer_tone(&buzzer_cfg, 600, 30);
    }
  });
}
