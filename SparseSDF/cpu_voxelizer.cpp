#include "cpu_voxelizer.h"

#include <algorithm>
#include "BBox.h"
#include "Vec2.h"

Vec3f clamp(const Vec3f& v, const Vec3f& lb, const Vec3f& ub) {
  return Vec3f(std::clamp(v[0], lb[0], ub[0]), std::clamp(v[1], lb[1], ub[1]),
               std::clamp(v[2], lb[2], ub[2]));
}

// Mesh voxelization method
void cpu_voxelize_mesh(voxconf conf, const TrigMesh* mesh,
                       VoxCallback& cb) {
  // Common variables used in the voxelization process
  size_t n_triangles = mesh->t.size() / 3;
  float eps = 1e-3f;
  Vec3f delta_p = conf.unit + Vec3f(eps);
  Vec3f grid_max(
      conf.gridSize[0] - 1, conf.gridSize[1] - 1,
      conf.gridSize[2] - 1);  // grid max (grid runs from 0 to gridsize-1)

  for (size_t i = 0; i < n_triangles; i++) {
    Vec3f c(0.0f, 0.0f, 0.0f);  // critical point
    // COMPUTE COMMON TRIANGLE PROPERTIES
    // Move vertices to origin using bbox
    Vec3f v0 = mesh->GetTriangleVertex(i, 0) - conf.origin;
    Vec3f v1 = mesh->GetTriangleVertex(i, 1) - conf.origin;
    Vec3f v2 = mesh->GetTriangleVertex(i, 2) - conf.origin;

    // Edge vectors
    Vec3f e0 = v1 - v0;
    Vec3f e1 = v2 - v1;
    Vec3f e2 = v0 - v2;
    // Normal vector pointing up from the triangle
    Vec3f n = e0.cross(e1);
    n.normalize();
    cb.BeginTrig(i);
    // COMPUTE TRIANGLE BBOX IN GRID
    // Triangle bounding box in world coordinates is min(v0,v1,v2) and
    // max(v0,v1,v2)
    BBox t_bbox_world;
    ComputeBBox(v0, v1, v2, t_bbox_world);
    // catch triangles exactly on voxel faces
    t_bbox_world.vmin = t_bbox_world.vmin - Vec3f(eps, eps, eps);
    // Triangle bounding box in voxel grid coordinates is the world bounding box
    // divided by the grid unit vector
    IntBox t_bbox_grid;
    Vec3f gridvec =
        clamp(t_bbox_world.vmin / conf.unit, Vec3f(0.0f, 0.0f, 0.0f), grid_max);
    t_bbox_grid.min = Vec3i(gridvec[0], gridvec[1], gridvec[2]);
    gridvec =
        clamp(t_bbox_world.vmax / conf.unit, Vec3f(0.0f, 0.0f, 0.0f), grid_max);
    t_bbox_grid.max = Vec3i(gridvec[0], gridvec[1], gridvec[2]);

    // PREPARE PLANE TEST PROPERTIES
    if (n[0] > 0.0f) {
      c[0] = delta_p[0];
    }
    if (n[1] > 0.0f) {
      c[1] = delta_p[1];
    }
    if (n[2] > 0.0f) {
      c[2] = delta_p[2];
    }
    float d1 = n.dot(c - v0);
    float d2 = n.dot((delta_p - c) - v0);

    // PREPARE PROJECTION TEST PROPERTIES
    // XY plane
    Vec2f n_xy_e0(-1.0f * e0[1], e0[0]);
    Vec2f n_xy_e1(-1.0f * e1[1], e1[0]);
    Vec2f n_xy_e2(-1.0f * e2[1], e2[0]);
    if (n[2] < 0.0f) {
      n_xy_e0 = -n_xy_e0;
      n_xy_e1 = -n_xy_e1;
      n_xy_e2 = -n_xy_e2;
    }
    float d_xy_e0 = (-1.0f * n_xy_e0.dot(Vec2f(v0[0], v0[1]))) +
                    std::max(0.0f, delta_p[0] * n_xy_e0[0]) +
                    std::max(0.0f, delta_p[1] * n_xy_e0[1]);
    float d_xy_e1 = (-1.0f * n_xy_e1.dot(Vec2f(v1[0], v1[1]))) +
                    std::max(0.0f, delta_p[0] * n_xy_e1[0]) +
                    std::max(0.0f, delta_p[1] * n_xy_e1[1]);
    float d_xy_e2 = (-1.0f * n_xy_e2.dot(Vec2f(v2[0], v2[1]))) +
                    std::max(0.0f, delta_p[0] * n_xy_e2[0]) +
                    std::max(0.0f, delta_p[1] * n_xy_e2[1]);
    // YZ plane
    Vec2f n_yz_e0(-1.0f * e0[2], e0[1]);
    Vec2f n_yz_e1(-1.0f * e1[2], e1[1]);
    Vec2f n_yz_e2(-1.0f * e2[2], e2[1]);
    if (n[0] < 0.0f) {
      n_yz_e0 = -n_yz_e0;
      n_yz_e1 = -n_yz_e1;
      n_yz_e2 = -n_yz_e2;
    }
    float d_yz_e0 = (-1.0f * n_yz_e0.dot(Vec2f(v0[1], v0[2]))) +
                    std::max(0.0f, delta_p[1] * n_yz_e0[0]) +
                    std::max(0.0f, delta_p[2] * n_yz_e0[1]);
    float d_yz_e1 = (-1.0f * n_yz_e1.dot(Vec2f(v1[1], v1[2]))) +
                    std::max(0.0f, delta_p[1] * n_yz_e1[0]) +
                    std::max(0.0f, delta_p[2] * n_yz_e1[1]);
    float d_yz_e2 = (-1.0f * n_yz_e2.dot(Vec2f(v2[1], v2[2]))) +
                    std::max(0.0f, delta_p[1] * n_yz_e2[0]) +
                    std::max(0.0f, delta_p[2] * n_yz_e2[1]);
    // ZX plane
    Vec2f n_zx_e0(-1.0f * e0[0], e0[2]);
    Vec2f n_zx_e1(-1.0f * e1[0], e1[2]);
    Vec2f n_zx_e2(-1.0f * e2[0], e2[2]);
    if (n[1] < 0.0f) {
      n_zx_e0 = -n_zx_e0;
      n_zx_e1 = -n_zx_e1;
      n_zx_e2 = -n_zx_e2;
    }
    float d_xz_e0 = (-1.0f * n_zx_e0.dot(Vec2f(v0[2], v0[0]))) +
                    std::max(0.0f, delta_p[0] * n_zx_e0[0]) +
                    std::max(0.0f, delta_p[2] * n_zx_e0[1]);
    float d_xz_e1 = (-1.0f * n_zx_e1.dot(Vec2f(v1[2], v1[0]))) +
                    std::max(0.0f, delta_p[0] * n_zx_e1[0]) +
                    std::max(0.0f, delta_p[2] * n_zx_e1[1]);
    float d_xz_e2 = (-1.0f * n_zx_e2.dot(Vec2f(v2[2], v2[0]))) +
                    std::max(0.0f, delta_p[0] * n_zx_e2[0]) +
                    std::max(0.0f, delta_p[2] * n_zx_e2[1]);

    // test possible grid boxes for overlap
    for (int z = t_bbox_grid.min[2]; z <= t_bbox_grid.max[2]; z++) {
      for (int y = t_bbox_grid.min[1]; y <= t_bbox_grid.max[1]; y++) {
        for (int x = t_bbox_grid.min[0]; x <= t_bbox_grid.max[0]; x++) {
          // TRIANGLE PLANE THROUGH BOX TEST
          Vec3f p(x * conf.unit[0], y * conf.unit[1], z * conf.unit[2]);
          p -= 0.5f * Vec3f(eps);

          float nDOTp = n.dot(p);
          if (((nDOTp + d1) * (nDOTp + d2)) > 0.0f) {
            continue;
          }

          // PROJECTION TESTS
          // XY
          Vec2f p_xy(p[0], p[1]);
          if ((n_xy_e0.dot(p_xy) + d_xy_e0) < 0.0f) {
            continue;
          }
          if ((n_xy_e1.dot(p_xy) + d_xy_e1) < 0.0f) {
            continue;
          }
          if ((n_xy_e2.dot(p_xy) + d_xy_e2) < 0.0f) {
            continue;
          }

          // YZ
          Vec2f p_yz(p[1], p[2]);
          if ((n_yz_e0.dot(p_yz) + d_yz_e0) < 0.0f) {
            continue;
          }
          if ((n_yz_e1.dot(p_yz) + d_yz_e1) < 0.0f) {
            continue;
          }
          if ((n_yz_e2.dot(p_yz) + d_yz_e2) < 0.0f) {
            continue;
          }

          // XZ
          Vec2f p_zx(p[2], p[0]);
          if ((n_zx_e0.dot(p_zx) + d_xz_e0) < 0.0f) {
            continue;
          }
          if ((n_zx_e1.dot(p_zx) + d_xz_e1) < 0.0f) {
            continue;
          }
          if ((n_zx_e2.dot(p_zx) + d_xz_e2) < 0.0f) {
            continue;
          }
          cb(x, y, z, i);
        }
      }
    }
    cb.EndTrig(i);
  }
}
