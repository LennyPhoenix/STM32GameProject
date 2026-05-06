#include "system.h"
#include "physics.h"
#include "render.h"

void run_systems(world_t *world, size_t *world_size, uint32_t frame) {
  move_entities(*world, *world_size);
  render_aabbs(*world, *world_size);
}
