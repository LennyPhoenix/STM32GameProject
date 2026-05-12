#ifndef SENSOR_H_
#define SENSOR_H_

#include "entity.h"

#include <stddef.h>
#include <stdint.h>

/// add this component to scan for intersections each frame
/// must specify a mask and a callback function
typedef struct sensor {
  uint32_t mask;
  void (*callback)(world_t *world, size_t *world_size, uint32_t frame,
                   entity_t *sensor, entity_t *detected);
} sensor_t;

void check_sensors(world_t *world, size_t *world_size, uint32_t frame);

#endif
