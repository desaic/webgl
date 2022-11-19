#include "Camera.h"

//mat4x4
#include "linmath.h"

Camera::Camera() : angle_xz(3.14f), angle_y(0) {
  for (int ii = 0; ii < 3; ii++) {
    eye[ii] = 0.0f;
    at[ii] = 0.0f;
    up[ii] = 0.0f;
  }
  at[2] = -1.0f;
  at[1] = 0.5f;
  eye[2] = -2.0f;
  eye[1] = 0.5f;
  up[1] = 1.0f;

  mat4x4_perspective(p, 3.14f / 3, 1, 0.1f, 20);
}

void Camera::update() {
  const float MAXY = 0.49f * 3.141593f;
  const float MAXXZ = 2 * 3.141593f;

  Vec3f viewDir(-sin(angle_xz), 0, cos(angle_xz));
  if (angle_y > MAXY) {
    angle_y = MAXY;
  }
  if (angle_y < -MAXY) {
    angle_y = -MAXY;
  }

  if (angle_xz > MAXXZ) {
    angle_xz -= MAXXZ;
  }
  if (angle_xz < -MAXXZ) {
    angle_xz += MAXXZ;
  }

  Vec3f yaxis = Vec3f(0, 1, 0);
  Vec3f right = viewDir.cross(yaxis);
  viewDir = cos(angle_y) * viewDir + sin(angle_y) * yaxis;
  at = eye + viewDir;
  up = right.cross(viewDir);
  mat4x4_perspective(p, 3.14f / 3, ratio, 0.1f, 20);
}