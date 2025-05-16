#include <iostream>
#include <vector>
#define TINYBVH_IMPLEMENTATION
#include "tiny_bvh.h"
#include "TrigMesh.h"
#include "MeshUtil.h"
#include "ImageIO.h"
#include "BBox.h"
#include "AdapSDF.h"
#include "AdapUDF.h"
#include "SDFMesh.h"

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

static inline float ModUV(float u) { 
  if (u < 0) {
    return u - int(u) + 1;
  } else {
    return u - int(u);
  }
}

struct LineSeg {
  Vec3f v0, v1;
};

void SaveLineSegsObj(const std::string& filename,
                     const std::vector<LineSeg>& lines) {
  std::ofstream out(filename);
  for (size_t i = 0; i < lines.size(); i++) {
    out << "v " << lines[i].v0[0] << " " << lines[i].v0[1] << " "
        << lines[i].v0[2] << "\n";
    out << "v " << lines[i].v1[0] << " " << lines[i].v1[1] << " "
        << lines[i].v1[2] << "\n";
  }
  for (size_t i = 0; i < lines.size(); i++) {
    out << "l " << (2 * i + 1) << " " << (2 * i + 2) << "\n";
  }
}

Vec3u heatmapColor(float value) {
  if (value < 0.0f ){    
    return Vec3u(0, 0, 0);
  }
  if (value > 10) {
    return Vec3u(255,255,255);
  }
  float normalizedValue = value / 10.0f;  // Normalize to 0-1

  if (normalizedValue < 0.25f) {
    // Blue to Cyan
    int blue = 255;
    int green = static_cast<int>(255 * normalizedValue * 4);
    return Vec3u(0, green, blue);
  } else if (normalizedValue < 0.5f) {
    // Cyan to Green
    int blue = static_cast<int>(255 * (1 - (normalizedValue - 0.25f) * 4));
    int green = 255;
    return Vec3u(0, green, blue);
  } else if (normalizedValue < 0.75f) {
    // Green to Yellow
    int green = 255;
    int red = static_cast<int>(255 * (normalizedValue - 0.5f) * 4);
    return Vec3u(red, green, 0);
  } else {
    // Yellow to Red
    int green = static_cast<int>(255 * (1 - (normalizedValue - 0.75f) * 4));
    int red = 255;
    return Vec3u(red, green, 0);
  }
}

//acceleration grid.
struct AccGrid {
  //0 for empty cell
  Array3D<unsigned> grid;
  //first entry reserved for empty cells
  std::vector<std::vector<unsigned> > lists;
  
  // first entry reserved for empty cells
  AccGrid():lists(1) {}

  void Allocate(const Vec3u& size) { grid.Allocate(size, 0);
  }

  Vec3i GetGridIndex(const Vec3f& x) const {
    Vec3f local = x - origin;
    Vec3i index(int(local[0] / h), int(local[1] / h), int(local[2] / h));
    return index;
  }

  bool IsInside(const Vec3i& index) const {
    const Vec3u& size = grid.GetSize();
    if (index[0] < 0 || index[0] >= int(size[0])) {
      return false;
    }
    if (index[1] < 0 || index[1] >= int(size[1])) {
      return false;
    }
    if (index[2] < 0 || index[2] >= int(size[2])) {
      return false;
    }
    return true;
  }

  //
  void Add(const Vec3f& x, unsigned id) {
    Vec3i gi=GetGridIndex(x);
    if (IsInside(gi)) {
      unsigned linearIndex = grid(gi[0], gi[1], gi[2]);
      
      if (linearIndex == 0) {
        linearIndex = lists.size();
        grid(gi[0], gi[1], gi[2]) = linearIndex;
        lists.push_back(std::vector<unsigned>(1, id));
      } else {
        lists[linearIndex].push_back(id);
      }
    }
  }

  std::vector<unsigned> GetNearby(const Vec3f &coord)const {
    std::vector<unsigned> l;
    Vec3i index0 = GetGridIndex(coord);
    //search +-1 around index0
    int searchRange = 1;
    for (int z = index0[2] - searchRange; z <= index0[2] + searchRange; z++) {
      for (int y = index0[1] - searchRange; y <= index0[1] + searchRange; y++) {
        for (int x = index0[0] - searchRange; x <= index0[0] + searchRange;
             x++) {
          if (!IsInside(Vec3i(x,y,z))) {
            continue;
          }
          unsigned listIndex = grid(unsigned(x), unsigned(y), unsigned(z));
          if (listIndex == 0) {
            continue;
          }
          const std::vector<unsigned> list = lists[listIndex];
          l.insert(l.end(), list.begin(), list.end());
        }
      }
    }
    return l;
  }

  Array3D8u GetDebugGrid() {
    Array3D8u d;
    d.Allocate(grid.GetSize(), 0);
    for (size_t i = 0; i < d.GetData().size(); i++) {
      d.GetData()[i] = grid.GetData()[i] > 0;
    }
    return d;
  }

  Vec3f origin;
  float h = 1.0f;
};

void TestSurfRaySample(){
  TrigMesh m;
  m.LoadObj("F:/meshes/shellVar/cap_uv.obj");
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

  tinybvh::BVH bvh;
  bvh.Build(triangles.data(), numTrigs);
  m.ComputeVertNormals();

  Vec2u imageSize(1600, 1600);
  Array2D8u texture(imageSize[0] * 3, imageSize[1]);

  std::vector<SurfacePoint> points;
  std::vector<float> thickness;
  SamplePoints(m, points, 0.5);
  const float MIN_T = 0.01;
  const size_t MAX_RAY_DIST = 1e20;
  std::vector<LineSeg> rays(points.size());
  thickness.resize(points.size());
  for (size_t i = 0; i < points.size(); i++) {
    Vec3f n = points[i].n;

    Vec3f rayDir = -points[i].n;
    tinybvh::bvhvec3 O(points[i].v[0],points[i].v[1],points[i].v[2]);
    tinybvh::bvhvec3 D(rayDir[0], rayDir[1], rayDir[2]);
    O[0] += MIN_T * D[0];
    O[1] += MIN_T * D[1];
    O[2] += MIN_T * D[2];
    tinybvh::Ray ray(O, D);
    int steps = bvh.Intersect(ray);
    float t = ray.hit.t;
    if (t > MAX_RAY_DIST) {
      t = 0.01;
    }
    if (steps < 0) {
      t = 0.001;
    }
    thickness[i] = t;
    LineSeg seg = {points[i].v, points[i].v + t * rayDir};
    rays[i]=seg;
    Vec2f uv = points[i].uv;
    uv[0] = unsigned(imageSize[0] * ModUV(uv[0]));
    uv[1] = unsigned(imageSize[1] * ModUV(uv[1]));
    if (uv[0] >= imageSize[0]) {
      uv[0] = imageSize[0] - 1;
    }
    if (uv[1] >= imageSize[1]) {
      uv[1] = imageSize[1] - 1;
    }
    if (t < 0) {
      t = 0;
    }
    if (t > 10) {
      t = 10;
    }
    Vec3u color = heatmapColor(10-t);
    texture(uv[0] * 3, imageSize[1] - uv[1] - 1) = color[0];
    texture(uv[0] * 3 + 1, imageSize[1] - uv[1] - 1) = color[1];
    texture(uv[0] * 3 + 2, imageSize[1] - uv[1] - 1) = color[2];
  }
  Box3f box;
  std::vector<Vec3f> coords(points.size());
  for (size_t i = 0; i < points.size(); i++) {
    coords[i] = points[i].v;
  }
  box = ComputeBBox(coords);

  AccGrid grid;
  grid.h = 1;
  float padding = grid.h / 2;  
  grid.origin = box.vmin-Vec3f(padding);
  Vec3f gridDim = box.vmax - box.vmin + 2.0f * Vec3f(padding);
  Vec3u gridSize(unsigned(gridDim[0] / grid.h) + 1, unsigned(gridDim[1] / grid.h) + 1,
                 unsigned(gridDim[2] / grid.h) + 1);
  grid.Allocate(gridSize);
  for (size_t i = 0; i < points.size();i++) {
    grid.Add(points[i].v, i);
  }
  //Array3D8u dg = grid.GetDebugGrid();
  //Vec3f voxRes(1);
  //SaveVolAsObjMesh("F:/meshes/shellVar/pt_accel.obj", dg, (float*)(&voxRes), 1);
  TrigMesh meshIn;
  meshIn.LoadStl("F:/meshes/shellVar/cap.stl");
  const float h = 0.5f;
  std::shared_ptr<AdapSDF> sdf = std::make_shared<AdapSDF>();
  //std::shared_ptr<AdapUDF> sdf = std::make_shared<AdapUDF>();
  SDFImpAdap* imp = new SDFImpAdap(sdf);
  SDFMesh sdfMesh(imp);
  sdfMesh.SetMesh(&meshIn);
  sdfMesh.SetVoxelSize(h);
  sdfMesh.SetBand(5);
  sdfMesh.Compute();
  Vec3u sdfSize = sdf->dist.GetSize();

  //Array2D8u debugsdf(sdfSize[0], sdfSize[1]);
  //unsigned z = unsigned(sdfSize[2] * 0.75);
  //for (z = 120; z < 180; z++) {
  //  for (unsigned y = 0; y < sdfSize[1]; y++) {
  //    for (unsigned x = 0; x < sdfSize[0]; x++) {
  //      short val = sdf->dist(x, y, z);
  //      if (val > 32760) {
  //        val = 0;
  //      }
  //      val /= 50;
  //      if (val < -100) {
  //        val = -100;
  //      }
  //      if (val > 125) {
  //        val = 125;
  //      }
  //      debugsdf(x, y) = 100 + val;
  //    }
  //  }
  //  SavePngGrey("F:/meshes/shellVar/sdfSlice" + std::to_string(z) + ".png",
  //              debugsdf);
  //}
  float minShell = 0.2;
  float maxShell = 0.75;

  for (unsigned z = 0; z < sdfSize[2]; z++) {
    for (unsigned y = 0; y < sdfSize[1]; y++) {
      for (unsigned x = 0; x < sdfSize[0]; x++) {
        Vec3f coord(x * h, y * h, z * h);
        coord += sdf->origin;
        std::vector<unsigned> nearby = grid.GetNearby(coord);
        if (nearby.size() == 0) {
          sdf->dist(x, y, z) += maxShell/sdf->distUnit;
          continue;
        }
        unsigned closestIndex = 0;
        float minDist = -1;
        for (size_t i = 0; i < nearby.size(); i++) {
          const SurfacePoint& sp = points[nearby[i]];
          float dist = (sp.v - coord).norm2();
          if (i == 0 || dist < minDist) {
            closestIndex = nearby[i];
            minDist = dist;
          }
        }
        float t = thickness[closestIndex];
        float offset = maxShell;
        //std::cout << t << "\n";
        if (t < maxShell * 10) {
          offset = std::max(minShell, t * 0.1f); 
        }
        sdf->dist(x, y, z) += offset / sdf->distUnit;
      }      
    }
  }

  TrigMesh surf;
  sdfMesh.MarchingCubes(0, &surf);

  float minThick = 0.2;
  float maxThick = 0.75;

  //SaveLineSegsObj("F:/meshes/shellVar/rays.obj", rays);
  //SavePngGrey("F:/meshes/shellVar/textureThickness.png", texture);
  //SavePngColor("F:/meshes/shellVar/colorThickness.png", texture);
  surf.SaveObj("F:/meshes/shellVar/surf_-0.2.obj");
}
