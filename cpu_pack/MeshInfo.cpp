#include "MeshInfo.h"
#include "Log.h"
#include "Matrix4f.h"

#include <AdapSDF.h>
#include <AdapDF.h>
#include <FastSweep.h>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <sstream>
#include <filesystem>

namespace fs = std::filesystem;

int LoadMesh(TrigMesh &m, const fs::path & p) {
  std::string ext = p.extension().string();
  std::transform(ext.begin(), ext.end(),ext.begin(), [](unsigned char c) { return std::tolower(c); });
  int ret = -1;
  if (ext == ".stl") {
    ret = m.LoadStl(p.string());
  } else if (ext == ".obj") {
    ret = m.LoadObj(p.string());
  } else {
    return -1;
  }
  return ret;
}

int LoadMeshInfo(MeshInfo &info, const fs::path &p) {
  int ret = LoadMesh(info.mesh, p);
  if (ret != 0) {
    return ret;
  }
  info.name = p.stem().string();
  info.box = ComputeBBox(info.mesh.v);  
  info.rb.SetMesh(info.mesh);
  return ret;
}

std::vector<MeshInfo> LoadAllMeshInfo(const std::string &meshDir, bool lazy) {
  std::vector<MeshInfo> fruits;
  fs::path inPath(meshDir);

  if (!fs::exists(inPath) || !fs::is_directory(inPath)) {
    std::cout << inPath.string() << " not a valid directory\n";
    return fruits;
  }
  for (const auto &entry : fs::directory_iterator(inPath)) {
    if (!entry.is_regular_file()) {
      continue;
    }
    fs::path p = entry.path();
    LOGD("Filename: " << p.filename() << " | " << p.stem() << " | "
                      << p.extension() << "\n");

    std::string stem = p.stem().string();
    if (stem.ends_with("_m")) {
      // skip mirrored meshes.
      continue;
    }
    std::string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
        return std::tolower(c);
    });
    if(ext != ".obj" && ext != ".stl"){
      LOGD("unknown extension " << ext << " skip\n");
      continue;
    }
    MeshInfo info;
    if (lazy) {
      info.name = stem;
      info.filePath = p.string();
      fruits.push_back(info);
      continue;
    }
    int ret = LoadMeshInfo(info, p);    
    if (ret != 0) {
      std::cout << " error " << ret << " loading " << p.filename().string() << "\n";
      continue;
    }
    info.filePath = p.string();
    fruits.push_back(info);
  }

  return fruits;
}

std::vector<int> SortBySize(const std::vector<MeshInfo> &items) {
  std::vector<int> indices(items.size());
  std::iota(indices.begin(), indices.end(), 0);
  std::sort(indices.begin(), indices.end(),
            [&](int i, int j) { return items[i].BoxDiagonal() > items[j].BoxDiagonal(); });
  return indices;
}

namespace {

// true if the grid edge from a to b crosses a triangle of the cells around it.
bool EdgeCrossesMesh(AdapSDF &sdf, const TrigMesh &mesh, const Vec3u &a, int axis, std::vector<size_t> &trigs) {
  trigs.clear();
  unsigned c[3] = {a[0], a[1], a[2]};
  // cells around the edge: the two cells on each side across both perpendicular axes.
  const int u = (axis + 1) % 3, w = (axis + 2) % 3;
  for (int du = 0; du < 2; du++) {
    for (int dw = 0; dw < 2; dw++) {
      unsigned cc[3] = {c[0], c[1], c[2]};
      if (du && cc[u] == 0) continue;
      if (dw && cc[w] == 0) continue;
      cc[u] -= du;
      cc[w] -= dw;
      sdf.GatherTrigs(cc[0], cc[1], cc[2], trigs);
    }
  }
  if (trigs.empty()) return false;
  const Vec3f p0 = sdf.voxSize * Vec3f(float(a[0]), float(a[1]), float(a[2])) + sdf.origin;
  Vec3f d(0, 0, 0);
  d[axis] = sdf.voxSize;
  const float eps = 1e-4f;
  for (size_t idx : trigs) {
    const Triangle t = mesh.GetTriangleVerts(idx);
    const Vec3f e1 = t.v[1] - t.v[0], e2 = t.v[2] - t.v[0];
    const Vec3f pv = d.cross(e2);
    const float det = e1.dot(pv);
    if (std::fabs(det) < 1e-12f) continue;
    const float inv = 1.0f / det;
    const Vec3f tv = p0 - t.v[0];
    const float bu = tv.dot(pv) * inv;
    if (bu < -eps || bu > 1.0f + eps) continue;
    const Vec3f qv = tv.cross(e1);
    const float bv = d.dot(qv) * inv;
    if (bv < -eps || bu + bv > 1.0f + eps) continue;
    const float tt = e2.dot(qv) * inv;
    if (tt >= -eps && tt <= 1.0f + eps) return true;
  }
  return false;
}

// sign of every band vertex from a flood fill from the outside across grid edges that do not cross
// the mesh, instead of from triangle normals (wrong on inverted or non manifold parts). what the
// flood cannot reach is inside, so no inner cavities. needs the triangle lists (before
// ClearTemporaryArrays) and a margin around the mesh so the grid boundary is outside.
void FloodSignFromOutside(AdapSDF &sdf, const TrigMesh &mesh) {
  const Vec3u size = sdf.dist.GetSize();
  const size_t n = size_t(size[0]) * size[1] * size[2];
  auto lin = [&](unsigned x, unsigned y, unsigned z) { return x + size_t(size[0]) * (y + size_t(size[1]) * z); };
  std::vector<uint8_t> outside(n, 0);
  std::vector<size_t> queue;
  for (unsigned z = 0; z < size[2]; z++)
    for (unsigned y = 0; y < size[1]; y++)
      for (unsigned x = 0; x < size[0]; x++) {
        if (x == 0 || y == 0 || z == 0 || x == size[0] - 1 || y == size[1] - 1 || z == size[2] - 1) {
          outside[lin(x, y, z)] = 1;
          queue.push_back(lin(x, y, z));
        }
      }
  std::vector<size_t> trigs;
  for (size_t head = 0; head < queue.size(); head++) {
    const size_t l = queue[head];
    const unsigned x = unsigned(l % size[0]), y = unsigned((l / size[0]) % size[1]), z = unsigned(l / (size_t(size[0]) * size[1]));
    const unsigned v[3] = {x, y, z};
    for (int axis = 0; axis < 3; axis++) {
      for (int sgn = -1; sgn <= 1; sgn += 2) {
        if ((sgn < 0 && v[axis] == 0) || (sgn > 0 && v[axis] + 1 >= size[axis])) continue;
        unsigned nb[3] = {x, y, z};
        nb[axis] += sgn;
        const size_t nl = lin(nb[0], nb[1], nb[2]);
        if (outside[nl]) continue;
        // the edge runs from the lower vertex to the upper one.
        const Vec3u lo(sgn < 0 ? nb[0] : x, sgn < 0 ? nb[1] : y, sgn < 0 ? nb[2] : z);
        if (EdgeCrossesMesh(sdf, mesh, lo, axis, trigs)) continue;
        outside[nl] = 1;
        queue.push_back(nl);
      }
    }
  }
  short *d = sdf.dist.GetData().data();
  for (size_t i = 0; i < n; i++) {
    if (std::abs(int(d[i])) >= AdapSDF::MAX_DIST) continue;
    const short a = short(std::abs(int(d[i])));
    d[i] = outside[i] ? a : short(-a);
  }
}

}  // namespace

void MeshInfo::ComputeSDFCached(){
  if(this->sdf){
    //naive logic for only computing it once.
    return;
  }
  this->sdf = std::make_shared<AdapSDF>();
  const float MIN_H = 0.2f;
  Vec3f boxSize = this->box.vmax - this->box.vmin;
  float maxLen = std::max(std::max(boxSize[0], boxSize[1]), boxSize[2]);
  sdf->voxSize = std::max(maxLen / 128.0f, MIN_H);
  float distUnit = 0.01f * sdf->voxSize;
  sdf->band = 16;
  sdf->distUnit = distUnit;
  sdf->BuildTrigList(&mesh);
  sdf->Compress();
  mesh.ComputePseudoNormals();
  sdf->ComputeCoarseDist();
  FloodSignFromOutside(*sdf, mesh);
  Array3D8u frozen;
  // band=8 (1.6cm at MIN_H) only reaches the surface region -- fine for
  // this sdf's original use (contact/collision queries near a surface),
  // but any point deeper than that reads back as AdapDF::MAX_DIST, a
  // large POSITIVE "far/unswept" sentinel, not a genuine "outside"
  // classification. TryFillSpot's SignedOverlapFraction/
  // ExistingInsideCandidateFraction (PackingDriver.cpp) use THIS sdf's
  // sign to test "is this point inside the mesh" for overlap checks, which
  // silently passes as "not inside" once a query point is buried more than
  // ~1.6cm into a mesh bigger than that (e.g. a raspberry sitting deep in
  // a plum) -- this is what let raspberries settle genuinely half-
  // submerged into bigger fruit undetected. Size the band from the mesh's
  // own extent (in voxels) so the sweep reaches its full interior
  // regardless of item size, same reasoning ComputeSDF (PackingScene.cpp,
  // container sdf) already uses a very large band for.
  int band = int(std::ceil(maxLen / sdf->voxSize)) + 4;
  FastSweepPar(sdf->dist, sdf->voxSize, distUnit, band, frozen);
}

