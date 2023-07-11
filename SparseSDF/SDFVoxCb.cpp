#include "SDFVoxCb.h"
#include "meshutil.h"
#include "PointTrigDist.h"
#include <iostream>
bool PtInBox(const Vec3f& pt, const Vec3f& boxMin, Vec3f& boxMax) {
  for (unsigned dim = 0; dim < 3; dim++) {
    if (pt[dim]<boxMin[dim] || pt[dim]>boxMax[dim]) {
      return false;
    }
  }
  return true;
}

void SDFVoxCb::operator()(unsigned x, unsigned y, unsigned z, size_t trigIdx) {
  // update distances of 8 vertices in the sdf->dist array
  Triangle trig = m->GetTriangleVerts(trigIdx);
  const unsigned NUM_CUBE_VERTS = 8;
  const unsigned CUBE_VERTS[NUM_CUBE_VERTS][3] = {
      {0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {1, 1, 0},
      {0, 0, 1}, {1, 0, 1}, {0, 1, 1}, {1, 1, 1}};

  Vec3f voxelMin(x, y, z);
  float h = sdf->voxSize;
  voxelMin = h * voxelMin + sdf->origin;
  Vec3f voxelMax = voxelMin + Vec3f(h);
  for (unsigned ci = 0; ci < NUM_CUBE_VERTS; ci++) {
    unsigned vx = x + CUBE_VERTS[ci][0];
    unsigned vy = y + CUBE_VERTS[ci][1];
    unsigned vz = z + CUBE_VERTS[ci][2];
    if (vx == 59 && vy == 13 && vz == 32) {
     std::cout << "debug\n";
      std::cout << m->nv[20101][0] << "\n";
     std::cout << m->nv[20102][0] << "\n";
    }

    Vec3f vertCoord(vx * h + sdf->origin[0], vy * h + sdf->origin[1],
                    vz * h + sdf->origin[2]);
    TrigDistInfo distInfo = PointTrigDist(vertCoord, (float*)(&trig));
    Vec3f normal = m->GetNormal(trigIdx, distInfo.bary);
    //vector from closest point to voxel point.
    Vec3f trigPt = vertCoord - distInfo.closest;
    float d = std::sqrt(distInfo.sqrDist);
    d = d / sdf->distUnit;
    short shortd = short(d);
    short oldDist = std::abs(sdf->dist(vx, vy, vz));
    if (oldDist > shortd) {
      if (normal.dot(trigPt) < 0) {
        sdf->dist(vx, vy, vz) = -shortd;
      } else {
        sdf->dist(vx, vy, vz) = shortd;
      }
    }    
  }

  //allocate  fine grid.
  Vec3u gridIdx(x, y, z);
  bool hasCell = sdf->HasCellDense(gridIdx);
  if (!hasCell) {
    sdf->AddSparseCell(gridIdx);
  }
  SparseNode4 < unsigned>& node = sdf->GetSparseNode4(x, y, z);
  unsigned cellIdx = *node.GetChildDense(x & 3, y & 3, z & 3);
  FixedGrid5& grid = sdf->sparseData[cellIdx];
  if (!hasCell) {
    grid.Fill(AdapSDF::MAX_DIST);
  }
}


void SDFVoxCb::BeginTrig(size_t trigIdx) {
}

/// free any triangle specific data.
void SDFVoxCb::EndTrig(size_t trigIdx) {}


void SDFFineVoxCb::operator()(unsigned x, unsigned y, unsigned z, size_t trigIdx) {
  // update distances of 8 vertices in the sdf->dist array
  Triangle trig = m->GetTriangleVerts(trigIdx);
  const unsigned NUM_CUBE_VERTS = 8;
  const unsigned CUBE_VERTS[NUM_CUBE_VERTS][3] = {
      {0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {1, 1, 0},
      {0, 0, 1}, {1, 0, 1}, {0, 1, 1}, {1, 1, 1}};

  Vec3f voxelMin(x, y, z);
  const unsigned N = FixedGrid5::N;
  // using 5 vertices per fine cell divides a cell into 4 subvoxels.
  float h = sdf->voxSize / (N-1);
  voxelMin = h * voxelMin + sdf->origin;
  Vec3f voxelMax = voxelMin + Vec3f(h);

  unsigned cx = x / (N-1);
  unsigned cy = y / (N-1);
  unsigned cz = z / (N-1);
  Vec3u baseIdx(cx,cy,cz);
  bool hasCell = sdf->HasCellSparse(baseIdx);
  if (!hasCell) {
    // a bug. cell should have been allocated
    return;
  }

  SparseNode4<unsigned>& node = sdf->GetSparseNode4(cx, cy, cz);
  const unsigned* child = node.GetChild(cx & 3, cy & 3, cz & 3);
  if (child == nullptr) {
    //shouldn't be here
    return;
  }
  unsigned cellIdx = *child;
  FixedGrid5& grid = sdf->sparseData[cellIdx];

  unsigned fineX = x % (N - 1);
  unsigned fineY = y % (N - 1);
  unsigned fineZ = z % (N - 1);

  for (unsigned ci = 0; ci < NUM_CUBE_VERTS; ci++) {
    unsigned vx = x + CUBE_VERTS[ci][0];
    unsigned vy = y + CUBE_VERTS[ci][1];
    unsigned vz = z + CUBE_VERTS[ci][2];
    Vec3f vertCoord(vx * h + sdf->origin[0], vy * h + sdf->origin[1],
                    vz * h + sdf->origin[2]);
    TrigDistInfo distInfo = PointTrigDist(vertCoord, (float*)(&trig));
    Vec3f normal = m->GetNormal(trigIdx, distInfo.bary);
    // vector from closest point to voxel point.
    Vec3f trigPt = vertCoord - distInfo.closest;
    float d = std::sqrt(distInfo.sqrDist);
    d = d / sdf->distUnit;
    short shortd = short(d);
    
    unsigned subx = fineX + CUBE_VERTS[ci][0];
    unsigned suby = fineY + CUBE_VERTS[ci][1];
    unsigned subz = fineZ + CUBE_VERTS[ci][2];

    short oldDist = std::abs(grid(subx, suby, subz));
    if (oldDist > shortd) {
      if (normal.dot(trigPt) < 0) {
        sdf->dist(subx, suby, subz) = -shortd;
      } else {
        sdf->dist(subx, suby, subz) = shortd;
      }
    }
  }

}