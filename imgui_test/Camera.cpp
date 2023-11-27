#include "Camera.h"

// mat4x4
#include "Matrix4f.h"

Camera::Camera() {
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
  update();
}

void Camera::update() {
  const float MAXY = 0.49f * 3.141593f;
  const float MAXXZ = 2 * 3.141593f;

  Vec3f viewDir = eye-at;
  viewDir.normalize();
  if (viewDir.norm2() == 0) {
    viewDir = Vec3f(0, 0, -1);
  }
  Vec3f yaxis = Vec3f(0, 1, 0);
  Vec3f right = viewDir.cross(yaxis);
  up = right.cross(viewDir);

  proj = Matrix4f::perspectiveProjection(fovRad, aspect, near, far, false);
  view = Matrix4f::lookAt(eye, at, up);
}

void Camera::VP(float* dst) { 
  //matrix on the right applied first.
  Matrix4f vp = proj * view; 
  const float* ptr = (const float*)(vp);
  for (unsigned i = 0; i < 16; i++) {
    dst[i] = ptr[i];
  }
}

void Camera::VIT(float* dst) {
  Matrix4f vit = view.inverse();
  vit.transpose();
  const float* ptr = (const float*)(vit);
  for (unsigned i = 0; i < 16; i++) {
    dst[i] = ptr[i];
  }
}
