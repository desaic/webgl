#include "SDFVoxCb.h"
#include "meshutil.h"
#include "PointTrigDist.h"

bool PtInBox(const Vec3f& pt, const Vec3f& boxMin, Vec3f& boxMax) {
  for (unsigned dim = 0; dim < 3; dim++) {
    if (pt[dim]<boxMin[dim] || pt[dim]>boxMax[dim]) {
      return false;
    }
  }
  return true;
}

void SDFVoxCb::operator()(unsigned x, unsigned y, unsigned z, size_t trigIdx) {
  Vec3u coarseIdx(x / 4, y / 4, z / 4);
  // update distances of 8 vertices in the sdf->dist array
  Triangle trig = m->GetTriangleVerts(trigIdx);
  const unsigned NUM_CUBE_VERTS = 8;
  const unsigned CUBE_VERTS[NUM_CUBE_VERTS][3] = {
      {0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {1, 1, 0},
      {0, 0, 1}, {1, 0, 1}, {0, 1, 1}, {1, 1, 1}};

  Vec3f voxelMin(x, y, z);
  voxelMin = sdf->voxSize * voxelMin + sdf->origin;
  Vec3f voxelMax = voxelMin + Vec3f(sdf->voxSize);
  Vec3u gridIdx(x, y, z);
  for (unsigned ci = 0; ci < NUM_CUBE_VERTS; ci++) {
    unsigned vx = x + CUBE_VERTS[ci][0];
    unsigned vy = y + CUBE_VERTS[ci][1];
    unsigned vz = z + CUBE_VERTS[ci][2];
    Vec3f vertCoord(vx * sdf->voxSize + sdf->origin[0],
                    vy * sdf->voxSize + sdf->origin[1],
                    vz * sdf->voxSize + sdf->origin[2]);
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
}


void SDFVoxCb::BeginTrig(size_t trigIdx) {
  std::vector<SurfacePoint> points;
  SamplePointsOneTrig(trigIdx, *m, points, sdf->pointSpacing/2);
  const Vec3f& dx = sdf->voxSize;
  for (size_t i = 0; i < points.size(); i++) {
    Vec3f p = points[i].v - sdf->origin;
    Vec3u gridIdx(unsigned(p[0]/dx[0]),unsigned(p[1]/dx[1]), unsigned(p[2]/dx[2]));
    sdf->AddPoint(points[i].v, gridIdx);
  }
}

/// free any triangle specific data.
void SDFVoxCb::EndTrig(size_t trigIdx) {}
