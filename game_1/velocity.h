#ifndef VELOCITY_H_
#define VELOCITY_H_

#include <stdbool.h>
#include <stdint.h>

typedef struct velocity {
  int32_t x;
  int32_t y;

  /// Set to `true` to skip all collision checks
  bool skip_collisions;
  /// Bitmask for collision layers
  uint32_t mask;
} velocity_t;

#endif
