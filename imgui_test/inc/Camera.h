#ifndef CAMERA_H
#define CAMERA_H

#include "Vec3.h"

struct Camera {
  Camera();

  void update();

  float angle_xz, angle_y;
  Vec3f eye;
  Vec3f at;
  Vec3f up;
  // projection matrix
  float p[4][4];
  // aspect ratio for perspective matrix;
  float ratio = 1.0f;
};
#endif