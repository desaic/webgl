#include "Array2D.h"
#include "Array3D.h"
#include "BBox.h"
#include "cpu_voxelizer.h"
#include <iostream>
#include <fstream>

#include "Stopwatch.h"
#include "meshutil.h"

#include <algorithm>
#include <numeric>
#include <deque>
#include <cctype>
#include <cstring>
#include <random>
#include <thread>
#include <filesystem>

namespace fs = std::filesystem;

Vec3f closestPointTriangle(const Vec3f & p, const Vec3f & a, const Vec3f & b, const Vec3f & c);

std::array<unsigned, 3> ComputeGridSize(const Box3f &box, float dx) {
  Vec3f boxSize = box.vmax - box.vmin;
  std::array<unsigned,3> size;
  for (unsigned d = 0; d < 3; d++) {
    size[d] = unsigned(boxSize[d] / dx) + 2;
  }
  return size;
}

std::array<float, 3> ToArray(const Vec3f &v) {
  return {v[0], v[1], v[2]};
}

static bool InBound(Vec3f x, Vec3u size) {
  return x[0] >= 0 && x[1] >= 0 && x[2] >= 0 && x[0] < size[0] && x[1] < size[1] && x[2] < size[2];
}

static bool InBound(Vec3u x, Vec3u size) {
  return x[0] < size[0] && x[1] < size[1] && x[2] < size[2];
}

std::vector<Vec3f> SamplePoints(const Box3f & box){
  // Sample 1 million random points within the bounding box
  const size_t numSamples = 1000000;
  std::vector<Vec3f> samples;
  samples.reserve(numSamples);

  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_real_distribution<float> distX(box.vmin[0], box.vmax[0]);
  std::uniform_real_distribution<float> distY(box.vmin[1], box.vmax[1]);
  std::uniform_real_distribution<float> distZ(box.vmin[2], box.vmax[2]);

  for (size_t i = 0; i < numSamples; i++) {
    samples.push_back(Vec3f(distX(gen), distY(gen), distZ(gen)));
  }
  return samples;
}

float dot(const Vec3f & a, const Vec3f & b){
  return a.dot(b);
}

Vec3f closestPointTriangle(const Vec3f & p, const Vec3f & a, const Vec3f & b, const Vec3f & c)
{
  const Vec3f ab = b - a;
  const Vec3f ac = c - a;
  const Vec3f ap = p - a;

  const float d1 = dot(ab, ap);
  const float d2 = dot(ac, ap);
  if (d1 <= 0.f && d2 <= 0.f) return a; //#1

  const Vec3f bp = p - b;
  const float d3 = dot(ab, bp);
  const float d4 = dot(ac, bp);
  if (d3 >= 0.f && d4 <= d3) return b; //#2

  const Vec3f cp = p - c;
  const float d5 = dot(ab, cp);
  const float d6 = dot(ac, cp);
  if (d6 >= 0.f && d5 <= d6) return c; //#3

  const float vc = d1 * d4 - d3 * d2;
  if (vc <= 0.f && d1 >= 0.f && d3 <= 0.f)
  {
      const float v = d1 / (d1 - d3);
      return a + v * ab; //#4
  }
    
  const float vb = d5 * d2 - d1 * d6;
  if (vb <= 0.f && d2 >= 0.f && d6 <= 0.f)
  {
      const float v = d2 / (d2 - d6);
      return a + v * ac; //#5
  }
    
  const float va = d3 * d6 - d5 * d4;
  if (va <= 0.f && (d4 - d3) >= 0.f && (d5 - d6) >= 0.f)
  {
      const float v = (d4 - d3) / ((d4 - d3) + (d5 - d6));
      return b + v * (c - b); //#6
  }

  const float denom = 1.f / (va + vb + vc);
  const float v = vb * denom;
  const float w = vc * denom;
  return a + v * ab + w * ac; //#0
}

class TrigGrid {
  public:
    void Build(const TrigMesh &m, float voxSize) {
      mesh = &m;
      voxelSize = voxSize;
      box = ComputeBBox(mesh->v);
      Vec3f boxSize = box.vmax - box.vmin;

      VoxConf conf;
      conf.origin = ToArray(box.vmin);
      conf.unit = {voxelSize, voxelSize, voxelSize};
      conf.gridSize = ComputeGridSize(box, voxelSize);
      std::cout << conf.gridSize[0] << " " << conf.gridSize[1] << " " << conf.gridSize[2] << "\n";

      origin = Vec3f(conf.origin[0], conf.origin[1], conf.origin[2]);

      grid.Allocate(conf.gridSize, 0);
      // tLists[0] is reserved for empty voxels
      tLists.push_back({});
      Utils::Stopwatch timer;
      timer.Start();
      cpu_voxelize_mesh_cb(conf, mesh, [&](unsigned int x, unsigned int y, unsigned int z, unsigned int tIdx) {
        uint32_t &gridVal = grid(x, y, z);
        if (gridVal == 0) {
          // First triangle for this voxel - create new list
          tLists.push_back({tIdx});
          gridVal = tLists.size() - 1;
        } else {
          // Voxel already has triangles - append to existing list
          tLists[gridVal].push_back(tIdx);
        }
      });
      float elapsedMs = timer.ElapsedMS();
      std::cout << "vox time ms " << elapsedMs << "\n";
    }

    float NearestTriangle(const Vec3f& point, float maxDist) const {
      // Convert point to grid coordinates
      Vec3f gridCoord = (point - origin) * (1.0f / voxelSize);
      Vec3i gridIdx = Vec3i(int(gridCoord[0]), int(gridCoord[1]), int(gridCoord[2]));

      float minDist = maxDist;
      Vec3u gridSize = grid.GetSize();
      // Search 3x3x3 neighborhood
      for (int dz = -1; dz <= 1; dz++) {
        int gz = gridIdx[2] + dz;
        if (gz < 0 || gz >= int(gridSize[2])) continue;

        for (int dy = -1; dy <= 1; dy++) {
          int gy = gridIdx[1] + dy;
          if (gy < 0 || gy >= int(gridSize[1])) continue;

          for (int dx_iter = -1; dx_iter <= 1; dx_iter++) {
            int gx = gridIdx[0] + dx_iter;
            if (gx < 0 || gx >= int(gridSize[0])) continue;

            uint32_t listIdx = grid(gx, gy, gz);
            if (listIdx == 0) continue; // Empty voxel

            const std::vector<unsigned>& trigList = tLists[listIdx];

            // Check distance to each triangle in this voxel
            for (unsigned tIdx : trigList) {
              Vec3f a, b, c;
              size_t vIdx0 = mesh->t[3 * tIdx];
              size_t vIdx1 = mesh->t[3 * tIdx + 1];
              size_t vIdx2 = mesh->t[3 * tIdx + 2];
              a = Vec3f(mesh->v[3 * vIdx0], mesh->v[3 * vIdx0 + 1], mesh->v[3 * vIdx0 + 2]);
              b = Vec3f(mesh->v[3 * vIdx1], mesh->v[3 * vIdx1 + 1], mesh->v[3 * vIdx1 + 2]);
              c = Vec3f(mesh->v[3 * vIdx2], mesh->v[3 * vIdx2 + 1], mesh->v[3 * vIdx2 + 2]);

              Vec3f closestPt = closestPointTriangle(point, a, b, c);
              float dist = (point - closestPt).norm();
              if (dist < minDist) {
                minDist = dist;
              }
            }
          }
        }
      }

      return minDist;
    }

    //dx will be reduced if grid is too large
    static const int MAX_GRID_SIZE = 1000;

  private:
    Array3D<uint32_t> grid;
    // tlists can be compressed to remove std::vector overhead 24bytes/cell
    std::vector<std::vector<unsigned>> tLists;
    Box3f box;
    Vec3f origin;
    float voxelSize = 1e-2;
    // does not own pointer, set during Build()
    const TrigMesh* mesh = nullptr;
};

void TestVox(){
  TrigMesh mesh;
  std::string dataDir = "/media/desaic/ssd2/data/";
  mesh.LoadObj(dataDir + "/cat.obj");
  Utils::Stopwatch timer;
  
  Box3f box = ComputeBBox(mesh.v);
  float dx = 0.02;
  std::cout<<box.vmin[0]<<" "<<box.vmin[1]<<" "<<box.vmin[2]<<"\n";
  Vec3f boxSize = box.vmax - box.vmin;
  std::cout<<boxSize[0]<<" "<<boxSize[1]<<" "<<boxSize[2]<<"\n";

  TrigGrid trigGrid;
  trigGrid.Build(mesh, dx);

  std::vector<Vec3f> samples = SamplePoints(box);

  // Compute distances for all sample points
  std::vector<float> distances(samples.size());
  const float MAX_DIST = 1e6f;

  timer.Start();
  for (size_t si = 0; si < samples.size(); si++) {
    distances[si] = trigGrid.NearestTriangle(samples[si], MAX_DIST);
  }

  float elapsedMs = timer.ElapsedMS();
  std::cout << "distance query time ms " << elapsedMs << "\n";
  std::cout << "average time per query us " << (elapsedMs * 1000.0f / samples.size()) << "\n";

  // Save close points to OBJ file
  std::ofstream outFile(dataDir + "/out/close_points.obj");
  if (outFile.is_open()) {
    size_t closeCount = 0;
    for (size_t si = 0; si < samples.size(); si++) {
      if (distances[si] <= dx) {
        const Vec3f& p = samples[si];
        outFile << "v " << p[0] << " " << p[1] << " " << p[2] << "\n";
        closeCount++;
      }
    }
    outFile.close();
    std::cout << "Saved " << closeCount << " close points (within " << dx << ") to close_points.obj\n";
  } else {
    std::cout << "Failed to open output file\n";
  }
}

int main(int argc, char * argv[]){  
  TestVox();
  return 0;
}