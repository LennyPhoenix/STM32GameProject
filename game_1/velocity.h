#ifndef VELOCITY_H_
#define VELOCITY_H_

#include <stdint.h>

typedef struct velocity {
  uint32_t x;
  uint32_t y;

  // Bitmask for collision layers
  uint32_t mask;
} velocity_t;

#endif
