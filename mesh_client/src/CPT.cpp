#include "CPT.h"
#include "TrigMesh.h"
#include "CPT.h"
#include "trigAABBIntersect.hpp"
#include "PointTrigDist.h"
#include "Timer.hpp"

#include <algorithm>
#include <functional>
#include <numeric>

#define MAX_GRID_SIZE 1000000

///voxelize 1 triangle.
void Voxelize(size_t tidx, SDFMesh* sdf);

///voxel center and triangle distance
void ExactDistance(SDFMesh* sdf);

bool trigCubeIntersect(float * verts, Vec3i& cube, float voxSize);

void bbox(const std::vector<float>& verts, BBoxInt& box,
  const Vec3f& voxelSize);

int cpt(SDFMesh& sdf)
{
  BBox box;
  ComputeBBox(sdf.mesh->v, box);
  sdf.box = box;

  Vec3u gridSize;
  float h = sdf.voxelSize;
  float margin = h * (0.5+sdf.band);

  for (unsigned i = 0; i < 3; i++) {
    gridSize[i] = (box.mx[i] - box.mn[i] + 2*margin) / h;
    sdf.gridOrigin[i] = box.mn[i] - margin;
    if (gridSize[i] > MAX_GRID_SIZE) {
      std::cout << "grid size is too large. check config." << gridSize[i] << "\n";
      return -1;
    }
  }
  sdf.sdf.Allocate(gridSize[0], gridSize[1], gridSize[2]);
  sdf.idxGrid.Allocate(gridSize[0], gridSize[1], gridSize[2]); 

  size_t numTrigs = sdf.mesh->t.size()/3;
  std::vector<size_t> tidx(numTrigs);
  std::iota(tidx.begin(), tidx.end(), 0);
  std::function<void(size_t)> voxFun = std::bind(&Voxelize, std::placeholders::_1, &sdf);

  Timer timer;
  timer.start();

  std::for_each(tidx.begin(), tidx.end(), voxFun);

  timer.end();
  float seconds = timer.getSeconds();
  std::cout << "Voxelize seconds " << seconds << "\n";

  timer.start();

  ExactDistance(&sdf);

  timer.end();
  seconds = timer.getSeconds();
  std::cout << "ExactDistance seconds " << seconds << "\n";

  return 0;
}

void GetTrig(std::vector<float> & trig, size_t tidx, const TrigMesh * mesh,
  const Vec3f & origin)
{
  const unsigned dim = 3;
  trig.resize(9);
  for (unsigned vi = 0; vi < 3; vi++) {
    unsigned vIdx = mesh->t[3 * tidx + vi];
    for (unsigned d = 0; d < dim; d++) {
      trig[3 * vi + d] = mesh->v[3 * vIdx + d] - origin[d];
    }
  }
}

void ExactDistance(SDFMesh* sdf) 
{
  ///\TODO use iterator instead of going through all voxels.
  Vec3u gridSize = sdf->idxGrid.GetSize();
  TreePointer ptr(& (sdf->idxGrid));
  TreePointer distPtr(&(sdf->sdf));
  const int ZAxis = 2;
  float h = sdf->voxelSize;
  for (size_t x = 0; x < gridSize[0]; x++) {
    for (size_t y = 0; y < gridSize[1]; y++) {
      ptr.PointToLeaf(x, y, 0);
      for (size_t z = 0; z < gridSize[2]; z++) {
        bool exists = ptr.HasValue();
        if (!exists) {
          ptr.Increment(ZAxis);
          continue;
        }
        size_t listIdx = 0;
        GetVoxelValue(ptr, listIdx);
        const std::vector<size_t> &trigs = sdf->trigList[listIdx];
        Vec3f center = sdf->gridOrigin + h * Vec3f(x+0.5f, y+0.5f, z+0.5f);
        std::vector<float> trig;
        TrigDistInfo minInfo;
        unsigned minT=0;
        for (size_t t = 0; t < trigs.size(); t++) {
          size_t tidx = trigs[t];
          GetTrig(trig, tidx, sdf->mesh, Vec3f(0, 0, 0));
          Vec3f bary;
          TrigDistInfo info = PointTrigDist(center, trig.data());
          if (t == 0 || info.sqrDist < minInfo.sqrDist) {
            minT = t;
            minInfo = info;
          }
        }
        Vec3f n = sdf->mesh->GetNormal(trigs[minT], minInfo.bary);
        float sign = 1.0f;
        if (Dot(n, center - minInfo.closest)<0) {
          sign = -1.0f;
        }

        if (x == 93 && y == 44 && z == 8) {
          std::cout << "debug\n";
          TrigMesh localMesh = SubSet(*(sdf->mesh), trigs);
          localMesh.SaveObj("local.obj");

          Vec3f mn = center - 1.5f * Vec3f(h);
          Vec3f mx = center + 1.5f * Vec3f(h);
          TrigMesh Voxel = MakeCube(mn, mx);
          Voxel.SaveObj("voxel.obj");

          std::vector<size_t> minList(1, trigs[minT]);
          TrigMesh closestMesh = SubSet(*(sdf->mesh), minList);
          closestMesh.SaveObj("closest.obj");
        }

        distPtr.PointToLeaf(x, y, z);
        distPtr.CreatePath();
        float dist = sign * std::sqrt(minInfo.sqrDist);
        dist /= h;
        AddVoxelValue(distPtr, dist);
        ptr.Increment(ZAxis);
      }
    }
  }
}

void Voxelize(size_t tidx, SDFMesh* sdf)
{
  BBoxInt box;
  std::vector<float> trig;
  GetTrig(trig, tidx, sdf->mesh, sdf->gridOrigin);
  const unsigned dim = 3;
  bbox(trig, box, sdf->voxelSize);
  const Vec3u& gridSize = sdf->idxGrid.GetSize();
  for (unsigned d = 0; d < dim; d++) {
    if (box.mn[d] > 0) {
      box.mn[d] -= 1;
    }
    if (box.mx[d] < int(gridSize[d]) - 1) {
      box.mx[d] += 1;
    }
  } 
  const unsigned zAxis = 2;
  TreePointer ptr(&sdf->idxGrid);
  TreePointer sdfPtr(&(sdf->sdf));
  for (int ix = box.mn[0]; ix <= (box.mx[0]); ix++) {
    for (int iy = box.mn[1]; iy <= (box.mx[1]); iy++) {
      //ptr.PointToLeaf(ix, iy, box.mn[2]);
      for (int iz = box.mn[2]; iz <= (box.mx[2]); iz++) {
        Vec3i gi(ix, iy, iz);
        if (trigCubeIntersect(trig.data(), gi, sdf->voxelSize)) {
          ptr.PointToLeaf(ix, iy, iz);
          ptr.CreatePath();
          size_t listIdx = 0;
          if (ptr.HasValue()) {
            GetVoxelValue(ptr, listIdx);
          } else {
            listIdx = sdf->trigList.size();
            AddVoxelValue(ptr, listIdx);
            sdf->trigList.push_back(std::vector<size_t>());
          }
          sdf->trigList[listIdx].push_back(tidx);
        }
        ptr.Increment(zAxis);
      }
    }
  }
}

bool trigCubeIntersect(float * verts, Vec3i& cube, float voxSize)
{
  float boxcenter[3] = { (float)((0.5f + cube[0]) * voxSize),
                      (float)((0.5f + cube[1]) * voxSize),
                      (float)((0.5f + cube[2]) * voxSize) };
  ///make cube half size 1.5 to catch triangles within 1 neighbor.
  ///TODO not very efficient.
  float halfSize = voxSize * 1.5f;
  float boxhalfsize[3] = { halfSize, halfSize, halfSize };
  return triBoxOverlap(boxcenter, boxhalfsize, verts);
}

void vec2grid(const float* v, Vec3i & gridIdx,
  const Vec3f & voxelSize)
{
  const int dim = 3;
  for (int ii = 0; ii < dim; ii++) {
    gridIdx[ii] = (int)(v[ii] / voxelSize[ii]);
  }
}

//negative indices are clamped to 0.
void bbox(const std::vector<float> & verts, BBoxInt & box,
  const Vec3f & voxelSize)
{
  Vec3i vidx;
  vec2grid(&verts[0], vidx, voxelSize);
  const int dim = 3;
  unsigned int nVert = verts.size()/3;
  for (int jj = 0; jj < dim; jj++) {
    box.mn[jj] = vidx[jj];
    box.mx[jj] = vidx[jj];
  }

  for (int jj = 1; jj < nVert; jj++) {
    vec2grid(&verts[3*jj], vidx, voxelSize);
    for (int kk = 0; kk < dim; kk++) {
      if (vidx[kk] < box.mn[kk]) {
        box.mn[kk] = vidx[kk];
      }
      if (vidx[kk] > box.mx[kk]) {
        box.mx[kk] = vidx[kk];
      }
    }
  }
}