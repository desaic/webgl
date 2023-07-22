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

Vec3f GetPseudoNormal(const TrigMesh& mesh, size_t trigIdx,
                      TrigPointType distType) {
  Vec3f normal;
  switch (distType) { 
  case TrigPointType::V0:
      return normal;
  default:
      return mesh.GetTrigNormal(trigIdx);
  }
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

  const auto it = trigInfo.find(trigIdx);
  if (it == trigInfo.end()) {
    //something very wrong
    return;
  }
  const TrigFrame& frame = it->second;
  
  for (unsigned ci = 0; ci < NUM_CUBE_VERTS; ci++) {
    unsigned vx = x + CUBE_VERTS[ci][0];
    unsigned vy = y + CUBE_VERTS[ci][1];
    unsigned vz = z + CUBE_VERTS[ci][2];

    Vec3f vertCoord(vx * h + sdf->origin[0], vy * h + sdf->origin[1],
                    vz * h + sdf->origin[2]);
    Vec3f pv0 = vertCoord - trig.v[0];
    float px = pv0.dot(frame.x);
    float py = pv0.dot(frame.y);
    TrigDistInfo distInfo = PointTrigDist2D(
        px, py, frame.v1x, frame.v2x, frame.v2y);
    Vec3f normal = m->GetNormal(trigIdx, distInfo.bary);
    Vec3f closest = distInfo.bary[0] * trig.v[0] +
                    distInfo.bary[1] * trig.v[1] + distInfo.bary[2] * trig.v[2];
    float trigz = pv0.dot(frame.z);
    // vector from closest point to voxel point.
    Vec3f trigPt = vertCoord - closest;
    float d = std::sqrt(distInfo.sqrDist+trigz*trigz);

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

  //allocate fine grid and triangle list.
  Vec3u gridIdx(x, y, z);
  if (!sdf->HasCellDense(gridIdx)) {
    unsigned cellIdx = sdf->AddDenseCell(gridIdx);
  }
}

void SDFVoxCb::BeginTrig(size_t trigIdx) {
  Vec3f x, y, z;
  Triangle trig = m->GetTriangleVerts(trigIdx);
  Vec3f n = m->GetTrigNormal(trigIdx);
  TrigFrame frame;
  ComputeTrigFrame((float*)(trig.v),n, frame);  
  trigInfo[trigIdx]=frame;

}

/// free any triangle specific data.
void SDFVoxCb::EndTrig(size_t trigIdx) {
  auto it =trigInfo.find(trigIdx); 
  if (it != trigInfo.end()) {
    trigInfo.erase(it);
  }  
}

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
  const auto it = trigInfo.find(trigIdx);
  if (it == trigInfo.end()) {
    // something very wrong
    return;
  }
  const TrigFrame& frame = it->second;
  
  for (unsigned ci = 0; ci < NUM_CUBE_VERTS; ci++) {
    unsigned vx = x + CUBE_VERTS[ci][0];
    unsigned vy = y + CUBE_VERTS[ci][1];
    unsigned vz = z + CUBE_VERTS[ci][2];
    Vec3f vertCoord(vx * h + sdf->origin[0], vy * h + sdf->origin[1],
                    vz * h + sdf->origin[2]);
    Vec3f pv0 = vertCoord - trig.v[0];
    float px = pv0.dot(frame.x);
    float py = pv0.dot(frame.y);
    TrigDistInfo distInfo =
        PointTrigDist2D(px, py, frame.v1x, frame.v2x, frame.v2y);
    Vec3f normal = m->GetNormal(trigIdx, distInfo.bary);
    Vec3f closest = distInfo.bary[0] * trig.v[0] +
                    distInfo.bary[1] * trig.v[1] + distInfo.bary[2] * trig.v[2];
    float trigz = pv0.dot(frame.z);
    // vector from closest point to voxel point.
    Vec3f trigPt = vertCoord - closest;
    float d = std::sqrt(distInfo.sqrDist + trigz * trigz);


    short shortd = short(d);
    
    unsigned subx = fineX + CUBE_VERTS[ci][0];
    unsigned suby = fineY + CUBE_VERTS[ci][1];
    unsigned subz = fineZ + CUBE_VERTS[ci][2];

    short oldDist =grid(subx, suby, subz);
    if (oldDist > shortd) {
        grid(subx, suby, subz) = shortd;
    }
  }

}

void SDFFineVoxCb::BeginTrig(size_t trigIdx) {
  Vec3f x, y, z;
  Triangle trig = m->GetTriangleVerts(trigIdx);
  Vec3f n = m->GetTrigNormal(trigIdx);
  TrigFrame frame;
  ComputeTrigFrame((float*)(trig.v), n, frame);
  trigInfo[trigIdx] = frame;
}

/// free any triangle specific data.
void SDFFineVoxCb::EndTrig(size_t trigIdx) {
  auto it = trigInfo.find(trigIdx);
  if (it != trigInfo.end()) {
    trigInfo.erase(it);
  }
}

void TrigListVoxCb::operator()(unsigned x, unsigned y, unsigned z,
                               size_t trigIdx) {
  Vec3u gridIdx(x, y, z);
  unsigned cellIdx = sdf->AddDenseCell(gridIdx);  
  std::vector<std::vector<size_t> >& trigList = sdf->trigList;

  if (cellIdx == trigList.size()) {
    trigList.push_back(std::vector<size_t>());
  } else if (cellIdx > trigList.size()) {
    std::cout << "bug TrigListVoxCb::operator()\n";
  }
  trigList[cellIdx].push_back(trigIdx);
  std::vector<Vec3u> &debugIndex=sdf->debugIndex;
  if (cellIdx == debugIndex.size()) {
    debugIndex.push_back(Vec3u(x,y,z));
  } else if (cellIdx > trigList.size()) {
    std::cout << "bug debugIndex TrigListVoxCb::operator()\n";
  }

  Vec3u idx = debugIndex[cellIdx];
  if(! (idx  == Vec3u(x,y,z))){
    std::cout << "bug index mapping\n";
  }
}
