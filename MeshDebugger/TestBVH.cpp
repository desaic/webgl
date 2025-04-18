#include <iostream>
#include <vector>
#define TINYBVH_IMPLEMENTATION
#include "tiny_bvh.h"
#include "TrigMesh.h"
#include "ImageIO.h"
#include "BBox.h"

void TestBVH() {
  TrigMesh m;
  m.LoadStl("F:/meshes/cap/cap.stl");
  size_t numTrigs = m.GetNumTrigs();

  std::vector<tinybvh::bvhvec4> triangles(numTrigs * 3);
  for (size_t t = 0; t < numTrigs; t++) {
    for (size_t vi = 0; vi < 3; vi++) {
      Vec3f vert = m.GetTriangleVertex(t, vi);
      triangles[3 * t + vi].x = vert[0];
      triangles[3 * t + vi].y = vert[1];
      triangles[3 * t + vi].z = vert[2];
    }
  }

  // build a BVH over the scene
  tinybvh::BVH bvh;
  bvh.Build(triangles.data(), numTrigs);

  Vec2u imageSize(800, 700);
  Array2D8u depth(imageSize[0], imageSize[1]);

  Box3f box = ComputeBBox(m.v);
  std::cout << box.vmin[0] << " " << box.vmin[1] << " " << box.vmin[2] << "\n";
  std::cout << box.vmax[0] << " " << box.vmax[1] << " " << box.vmax[2] << "\n";

  Vec3f boxCenter = 0.5f * (box.vmin + box.vmax);
  Vec3f boxSize = box.vmax - box.vmin;
  float len = boxSize.norm();
  len = std::max(1.0f, len);
  Vec3f eye = boxCenter;
  eye[1] -= len;
  Vec3f dir(0, 1, 0);
  Vec3f up(0, 0, 1);

  Vec3f right = dir.cross(up);
  right.normalize();
  up = right.cross(dir);
  up.normalize();

  float fovDeg = 50;

  float fovRad = (fovDeg / 180.0f) * 3.1415927f;

  float imagePlane = 20;
  float imageHalfWidth = imagePlane * std::tan(fovRad / 2);
  float imageHalfHeight = imageHalfWidth / imageSize[0] * imageSize[1];
  float pixSize = (2 * imageHalfWidth) / imageSize[0];

  tinybvh::bvhvec3 O(eye[0], eye[1], eye[2]);
  std::cout << O.x << " " << O.y << " " << O.z << "\n";
  for (unsigned y = 0; y < imageSize[1]; y++) {
    float pixy = -imageHalfHeight + (float(y) + 0.5f) * pixSize;
    if (y == 0 || y == imageSize[1] - 1) {
      std::cout << pixy << " y\n";
    }
    for (unsigned x = 0; x < imageSize[0]; x++) {
      float pixx = -imageHalfWidth + (float(x) + 0.5f) * pixSize;
      if ( (x == 0 || x == imageSize[0] - 1) && y==0) {
        std::cout << pixx << " x\n";
      }
      Vec3f ray1 = eye + imagePlane * dir + pixx * right + pixy * up;
      Vec3f rayDir = ray1 - eye;
      rayDir.normalize();

      tinybvh::bvhvec3 D(rayDir[0], rayDir[1], rayDir[2]);
      tinybvh::Ray ray(O, D);
      int steps = bvh.Intersect(ray);
      float t = ray.hit.t;
      std::cout << ray.hit.prim;
      if (t < 0 || t > 1000) {
        depth(x, y) = 0;
        continue;
      }
      depth(x, y) = 255 - uint8_t(t);
    }
  }
  SavePngGrey("F:/meshes/shellVar/depth.png", depth);
}
