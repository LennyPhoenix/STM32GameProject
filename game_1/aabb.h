#ifndef AABB_H_
#define AABB_H_

#include <stdint.h>

typedef struct aabb {
  uint32_t x;
  uint32_t y;
  uint32_t width;
  uint32_t height;

  uint32_t layer;
} aabb_t;

#endif
