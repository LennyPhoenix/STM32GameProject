#ifndef GRAVITY_H_
#define GRAVITY_H_

/// attach to an entity to apply a constant downwards force
typedef struct gravity {
  /// Downward velocity in m/s
  int32_t g;
} gravity_t;

#endif
