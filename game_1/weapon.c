#include "weapon.h"
#include "Buzzer.h"
#include "aabb.h"
#include "entity.h"
#include "velocity.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>

extern Buzzer_cfg_t buzzer_cfg; // Buzzer control

void fire_weapons(world_t *world, size_t *world_size, uint32_t frame) {
  ITER_ENTITIES(*world, *world_size, entity, {
    if (entity->weapon && entity->aabb) {
      weapon_t *weapon = entity->weapon;

      if (weapon->firing &&
          frame >= weapon->last_frame_fired + weapon->cooldown) {
        weapon->last_frame_fired = frame;
        entity_t *projectile = weapon->new_projectile(world, world_size, frame);

        aabb_t *aabb = ensure_aabb(projectile);
        aabb->x = entity->aabb->x + entity->aabb->width / 2 - aabb->width / 2;
        aabb->y = entity->aabb->y + entity->aabb->height / 2 - aabb->height / 2;

        velocity_t *velocity = ensure_velocity(projectile);
        velocity->x = weapon->projectile_speed * weapon->aim_right;
        velocity->y = weapon->projectile_speed * -weapon->aim_down;

        buzzer_tone(&buzzer_cfg, 1000, 60);
      }
    }
  });
}
