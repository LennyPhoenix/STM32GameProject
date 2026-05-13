#ifndef AABB_H_
#define AABB_H_

#include <stdbool.h>
#include <stdint.h>

/// stores an XY pair
typedef struct point {
  int32_t x;
  int32_t y;
} point_t;

/// represents an axis-aligned bounding box, with a position and size
///
/// also stores the collision layer as a bitfield
typedef struct aabb {
  int32_t x;
  int32_t y;
  int32_t width;
  int32_t height;

  uint32_t layer;
} aabb_t;

/// method to check for intersection between two AABBs
bool intersects_aabb_aabb(aabb_t a, aabb_t b);
/// method to check whether a point is contained within an AABB
bool intersects_aabb_point(aabb_t a, point_t b);

#endif
