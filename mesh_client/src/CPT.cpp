#include "CPT.h"
#include "Vec2.h"
#include "Array2D.h"
#include "TrigMesh.h"
#include "trigAABBIntersect.hpp"
#include "PointTrigDist.h"
#include "Timer.hpp"
#include "BBox.h"
#include <algorithm>
#include <functional>
#include <numeric>
#include <set>

#define MAX_GRID_SIZE 1000000

///voxelize 1 triangle.
void Voxelize(size_t tidx, SDFMesh* sdf);

///voxel center and triangle distance
void ExactDistance(SDFMesh* sdf);

bool trigCubeIntersect(float* verts, Vec3i& cube, float voxSize);

void bbox(const std::vector<float>& verts, BBoxInt& box,
  const Vec3f& voxelSize);

void RasterizeTrig2D(Array2D<uint8_t>& out, const Vec2f & a, 
  const Vec2f & b, const Vec2f & c);

void RasterizeTrig2D(Array2D<uint8_t>& out, const Vec2f& a,
  const Vec2f& b, const Vec2f& c)
{
  Vec2f v[3];
  //sort vertices by decreasing y coord.
  if (a[1] >= b[1]) {
    if (c[1] >= a[1]) {
      v[0] = c;
      v[1] = a;
      v[2] = b;
    }
    else {
      if (b[1] >= c[1]) {
        v[0] = a;
        v[1] = b;
        v[2] = c;
      }
      else {
        v[0] = a;
        v[1] = c;
        v[2] = b;
      }
    }
  }
  else {
    if (c[1] >= b[1]) {
      v[0] = c;
      v[1] = b;
      v[2] = a;
    }
    else {
      if (a[1] >= c[1]) {
        v[0] = b;
        v[1] = a;
        v[2] = c;
      }
      else {
        v[0] = b;
        v[1] = c;
        v[2] = a;
      }
    }
  }

  float xl = v[2][0], xr=v[2][0];
  float y = v[2][1];
}

int cpt(SDFMesh& sdf)
{
  BBox box;
  ComputeBBox(sdf.mesh->v, box);
  sdf.box = box;

  Vec3u gridSize;
  float h = sdf.voxelSize;
  float margin = h * (0.5 + sdf.band);

  for (unsigned i = 0; i < 3; i++) {
    gridSize[i] = (box.mx[i] - box.mn[i] + 2 * margin) / h;
    sdf.gridOrigin[i] = box.mn[i] - margin;
    if (gridSize[i] > MAX_GRID_SIZE) {
      std::cout << "grid size is too large. check config." << gridSize[i] << "\n";
      return -1;
    }
  }
  sdf.sdf.Allocate(gridSize[0], gridSize[1], gridSize[2]);
  sdf.idxGrid.Allocate(gridSize[0], gridSize[1], gridSize[2]);

  size_t numTrigs = sdf.mesh->t.size() / 3;
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

void GetTrig(std::vector<float>& trig, size_t tidx, const TrigMesh* mesh,
  const Vec3f& origin)
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

unsigned closestTrig(const std::vector<size_t>& trigs,
  SDFMesh* sdf, size_t x, size_t y, size_t z,
  TrigDistInfo& minInfo)
{
  float h = sdf->voxelSize;
  Vec3f center = sdf->gridOrigin + h * Vec3f(x + 0.5f, y + 0.5f, z + 0.5f);
  std::vector<float> trig;
  unsigned minT = 0;
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
  return minT;
}

void AddTrigs(SDFMesh* sdf, TreePointer& ptr, std::set<size_t>& s)
{
  if (!ptr.HasValue()) {
    return;
  }
  size_t listIdx = 0;
  GetVoxelValue(ptr, listIdx);
  const std::vector<size_t>& trigs = sdf->trigList[listIdx];
  std::copy(trigs.begin(), trigs.end(), std::inserter(s, s.end()));
}

void GetNeighborTrigs(std::vector<size_t>& trigs,
  SDFMesh* sdf, size_t x, size_t y, size_t z,
  const TreePointer& ptr)
{
  const int dim = 3;
  std::set<size_t> trigSet;
  for (int axis = 0; axis < dim; axis++) {
    TreePointer localPtr = ptr;
    bool exists = localPtr.Increment(axis);
    if (exists) {
      AddTrigs(sdf, localPtr, trigSet);
    }
    localPtr = ptr;
    exists = localPtr.Decrement(axis);
    if (exists) {
      AddTrigs(sdf, localPtr, trigSet);
    }
  }
  std::vector<size_t> v(trigSet.begin(), trigSet.end());
}

void ExactDistance(SDFMesh* sdf)
{
  ///\TODO use iterator instead of going through all voxels.
  Vec3u gridSize = sdf->idxGrid.GetSize();
  TreePointer ptr(&(sdf->idxGrid));
  TreePointer distPtr(&(sdf->sdf));
  const int ZAxis = 2;
  float h = sdf->voxelSize;
  for (size_t x = 0; x < gridSize[0]; x++) {
    for (size_t y = 0; y < gridSize[1]; y++) {
      ptr.PointTo(x, y, 0);
      for (size_t z = 0; z < gridSize[2]; z++) {
        bool exists = ptr.HasValue();
        if (!exists) {
          ptr.Increment(ZAxis);
          continue;
        }
        size_t listIdx = 0;
        GetVoxelValue(ptr, listIdx);

        const std::vector<size_t>& trigs = sdf->trigList[listIdx];
        if (trigs.size() == 0) {
          //should never happen.
          continue;
        }
        TrigDistInfo minInfo;
        unsigned minT = closestTrig(trigs, sdf, x, y, z, minInfo);
        size_t trigIdx = trigs[minT];

        //need to check neighboring cells
        if (minInfo.sqrDist >= 0.25) {
          std::vector<size_t> nbrTrigs;
          GetNeighborTrigs(nbrTrigs, sdf, x, y, z, ptr);
          if (nbrTrigs.size() > 0) {
            TrigDistInfo nbrInfo;
            unsigned nbrMinT = closestTrig(nbrTrigs, sdf, x, y, z, nbrInfo);
            if (nbrInfo.sqrDist < minInfo.sqrDist) {
              trigIdx = nbrTrigs[nbrMinT];
              minInfo = nbrInfo;
            }
          }
        }

        float dist = std::sqrt(minInfo.sqrDist);
        dist /= h;
        distPtr.PointTo(x, y, z);
        distPtr.CreatePath();

        AddVoxelValue(distPtr, dist);
        ptr.Increment(ZAxis);
      }
    }
  }
}

int ComputeOBB(const std::vector<float>& trig, OBBox& obb, float band)
{
  Vec3f v[3], e[3];
  v[0] = Vec3f(trig[0], trig[1], trig[2]);
  v[1] = Vec3f(trig[3], trig[4], trig[5]);
  v[2] = Vec3f(trig[6], trig[7], trig[8]);
  e[0] = v[1] - v[0];
  e[1] = v[2] - v[1];
  e[2] = v[0] - v[2];
  float len[3];
  len[0] = e[0].norm2();
  len[1] = e[1].norm2();
  len[2] = e[2].norm2();

  unsigned longestEdge = 0;
  float maxLen = len[0];
  float eps = 1e-6;
  for (unsigned i = 1; i < 3; i++) {
    if (len[i] > maxLen) {
      maxLen = len[i];
      longestEdge = i;
    }
  }
  unsigned v0 = longestEdge;
  obb.origin = v[v0];
  if (maxLen < eps) {
    return -1;
  }
  Vec3f n = e[2].cross(e[0]);
  float area2 = n.norm2();
  if (area2 < eps * eps) {
    return -1;
  }
  float area = std::sqrt(area2);
  float edgeLen = std::sqrt(maxLen);
  Vec3f unitx = (1.0f / edgeLen) * e[longestEdge];
  obb.axes[2] = (1.0f / area) * n;
  obb.axes[0] = e[longestEdge] + (2* band) * unitx;
  obb.axes[1] = obb.axes[2].cross(unitx);
  obb.origin -= band * unitx;
  obb.origin -= band * obb.axes[1];
  obb.origin -= band * obb.axes[2];
  //calc axis lengths
  float h = area / edgeLen;
  obb.axes[1] = (h + 2 * band) * obb.axes[1];
  obb.axes[2] = 2 * band * obb.axes[2];
  return 0;
}

void TestOBB(std::vector<float> & trig, OBBox & obb)
{
  for (size_t i = 0; i < trig.size(); i++) {
    std::cout << trig[i] << " ";
  }
  std::cout << "\n";

  std::cout << to_string(obb.origin)<<" ";
  std::cout << to_string(obb.axes[0]) << " ";
  std::cout << to_string(obb.axes[1]) << " ";
  std::cout << to_string(obb.axes[2]) << "\n";
}

void AddTrigToVoxel(size_t tidx, int ix, int iy, int iz, 
  TreePointer * ptr, SDFMesh * sdf)
{
  Vec3i gi(ix, iy, iz);
  
  ptr->PointTo(ix, iy, iz);
  ptr->CreatePath();
  size_t listIdx = 0;
  if (ptr->HasValue()) {
    GetVoxelValue(*ptr, listIdx);
  }
  else {
    listIdx = sdf->trigList.size();
    AddVoxelValue(*ptr, listIdx);
    sdf->trigList.push_back(std::vector<size_t>());
  }
  sdf->trigList[listIdx].push_back(tidx);  
}

void VoxelizeOBB() {

}

void Voxelize(size_t tidx, SDFMesh* sdf)
{
  BBoxInt box;
  OBBox obb;

  std::vector<float> trig;
  GetTrig(trig, tidx, sdf->mesh, sdf->gridOrigin);
  const unsigned dim = 3;
  bbox(trig, box, sdf->voxelSize);

  //ComputeOBB(trig, obb, sdf->exactBand);
  //TestOBB(trig, obb);
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
      for (int iz = box.mn[2]; iz <= (box.mx[2]); iz++) {
        Vec3i gi(ix, iy, iz);
        if (trigCubeIntersect(trig.data(), gi, sdf->voxelSize)) {
          AddTrigToVoxel(tidx, ix, iy, iz, &ptr, sdf);
        }
        ptr.Increment(zAxis);
      }
    }
  }
}

bool trigCubeIntersect(float* verts, Vec3i& cube, float voxSize)
{
  float boxcenter[3] = { (float)((0.5f + cube[0]) * voxSize),
                      (float)((0.5f + cube[1]) * voxSize),
                      (float)((0.5f + cube[2]) * voxSize) };
  ///make cube half size 1.5 to catch triangles within 1 neighbor.
  ///TODO not very efficient.
  float halfSize = 1.501 * voxSize;
  float boxhalfsize[3] = { halfSize, halfSize, halfSize };
  return triBoxOverlap(boxcenter, boxhalfsize, verts);
}

void vec2grid(const float* v, Vec3i& gridIdx,
  const Vec3f& voxelSize)
{
  const int dim = 3;
  for (int ii = 0; ii < dim; ii++) {
    gridIdx[ii] = (int)(v[ii] / voxelSize[ii]);
  }
}

//negative indices are clamped to 0.
void bbox(const std::vector<float>& verts, BBoxInt& box,
  const Vec3f& voxelSize)
{
  Vec3i vidx;
  vec2grid(&verts[0], vidx, voxelSize);
  const int dim = 3;
  unsigned int nVert = verts.size() / 3;
  for (int jj = 0; jj < dim; jj++) {
    box.mn[jj] = vidx[jj];
    box.mx[jj] = vidx[jj];
  }

  for (int jj = 1; jj < nVert; jj++) {
    vec2grid(&verts[3 * jj], vidx, voxelSize);
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
