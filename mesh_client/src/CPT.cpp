#include "CPT.h"
#include "TrigMesh.h"
#include "CPT.h"
#include "trigAABBIntersect.hpp"

#include <algorithm>
#include <functional>
#include <numeric>

#define MAX_GRID_SIZE 1000000

///voxelize 1 triangle.
void Voxelize(size_t tidx, SDFMesh* sdf);

bool trigCubeIntersect(float * verts, Vec3i& cube,
  const Vec3f& voxSize);

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
  std::for_each(tidx.begin(), tidx.end(), voxFun);
  return 0;
}

void Voxelize(size_t tidx, SDFMesh * sdf)
{
  BBoxInt box;
  std::vector<float> trig(9);
  const unsigned dim = 3;
  for (unsigned vi = 0; vi < 3; vi++) {
    unsigned vIdx = sdf->mesh->t[3 * tidx + vi];
    for (unsigned d = 0; d < dim; d++) {
      trig[3 * vi + d] = sdf->mesh->v[3 * vIdx + d] - sdf->gridOrigin[d];
    }
  }
  
  bbox(trig, box, sdf->voxelSize);
  TreePointer ptr(&sdf->idxGrid);

  for (int ix = box.mn[0]; ix <= (box.mx[0]); ix++) {
    for (int iy = box.mn[1]; iy <= (box.mx[1]); iy++) {
      for (int iz = box.mn[2]; iz <= (box.mx[2]); iz++) {
        Vec3i gi(ix, iy, iz);
        if (trigCubeIntersect(trig.data(), gi, sdf->voxelSize)) {
          bool exists = ptr.PointToLeaf(ix, iy, iz);
          if (!exists) {
            ptr.CreateLeaf(ix, iy, iz);
          }
          size_t listIdx = 0;
          if (ptr.HasValue()) {
            GetVoxelValue(ptr, listIdx);
          } else {
            AddVoxelValue(ptr, listIdx);
            listIdx = sdf->trigList.size();
            sdf->trigList.push_back(std::vector<size_t>());
          }
          sdf->trigList[listIdx].push_back(tidx);
        }
      }
    }
  }
}

bool trigCubeIntersect(float * verts, Vec3i& cube,
  const Vec3f & voxSize)
{
  float boxcenter[3] = { (float)((0.5 + cube[0]) * voxSize[0]),
                      (float)((0.5 + cube[1]) * voxSize[1]),
                      (float)((0.5 + cube[2]) * voxSize[2]) };
  float boxhalfsize[3] = { (float)voxSize[0] / (1.99f),
                      (float)voxSize[1] / (1.99f),
                      (float)voxSize[2] / (1.99f) };
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
