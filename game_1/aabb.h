#ifndef AABB_H_
#define AABB_H_

#include <stdbool.h>
#include <stdint.h>

typedef struct point {
  int32_t x;
  int32_t y;
} point_t;

typedef struct aabb {
  int32_t x;
  int32_t y;
  int32_t width;
  int32_t height;

  uint32_t layer;
} aabb_t;

bool intersects_aabb_aabb(aabb_t a, aabb_t b);
bool intersects_aabb_point(aabb_t a, point_t b);

#endif
