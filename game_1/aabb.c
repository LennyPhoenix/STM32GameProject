#include "aabb.h"

bool intersects_aabb_aabb(aabb_t a, aabb_t b) {
  // calculate sides for each AABB
  int32_t left_a, right_a, top_a, bottom_a;
  int32_t left_b, right_b, top_b, bottom_b;

  left_a = a.x;
  left_b = b.x;
  right_a = left_a + a.width;
  right_b = left_b + b.width;

  top_a = a.y;
  top_b = b.y;
  bottom_a = top_a + a.height;
  bottom_b = top_b + b.height;

  // `>` instead of `>=` used here as the right side of an AABB does not occupy
  // the position it points to
  // e.g. a rect with position 0,0 and size 5x5 has a right side at x=5 (0 + 5 =
  // 5), but the AABB does not actually occupy the coordinate (5,0).
  return (left_b < right_a && top_b < bottom_a && right_b > left_a &&
          bottom_b > top_a);
}

bool intersects_aabb_point(aabb_t a, point_t b) {
  int32_t left, right, top, bottom;
  left = a.x;
  right = left + a.width;
  top = a.y;
  bottom = top + a.height;

  // see above note, `>=` for left, `<` for right
  return (b.x >= left && b.x < right && b.y >= top && b.y < bottom);
}
