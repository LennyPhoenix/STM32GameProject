#ifndef PLAYER_H_
#define PLAYER_H_

#include <math.h>
#include <stdbool.h>

typedef struct player {
  bool jumping, shooting;
  double_t aim_left, aim_right;
} player_t;

#endif
